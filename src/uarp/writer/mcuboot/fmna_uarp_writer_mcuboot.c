/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>

#include <bootutil/bootutil_public.h>

#include "fmna_uarp_writer_util_nvm.h"
#include "fmna_uarp_writer.h"
#include "fmna_uarp_writer_mcuboot.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(fmna_uarp_writer_mcuboot, CONFIG_FMNA_UARP_WRITER_MCUBOOT_LOG_LEVEL);

static atomic_t in_progress = ATOMIC_INIT(false);

static struct fmna_uarp_writer_util_nvm_ctx nvm_util_ctx;
static uint8_t buf[CONFIG_FMNA_UARP_WRITER_MCUBOOT_BUF_SIZE] __aligned(4);

static bool is_confirmed_mcuboot_image(uint8_t fa)
{
	int err;
	const struct flash_area *fap;
	struct boot_swap_state state;

	err = flash_area_open(fa, &fap);
	if (err) {
		LOG_ERR("flash_area_open failed (err %d)", err);
		return false;
	}

	err = boot_read_swap_state(fap, &state);
	flash_area_close(fap);
	if (err != 0) {
		LOG_ERR("boot_read_swap_state failed (err %d)", err);
		return false;
	}

	if (state.magic == BOOT_MAGIC_UNSET) {
		/* Swap mode:
		 *   This is initial/preprogramed image and can neither be
		 *   reverted nor physically confirmed.
		 *   Treat this image as confirmed.
		 * Direct-XIP mode:
		 *   Magic is required to even consider booting the image.
		 */
		return true;
	}

	return state.image_ok == BOOT_FLAG_SET;
}

static int test_mcuboot_image(uint8_t fa)
{
	int err;
	const struct flash_area *fap;

	err = flash_area_open(fa, &fap);
	if (err) {
		LOG_ERR("flash_area_open failed (err %d)", err);
		return err;
	}

	err = boot_set_next(fap, false, false);
	if (err) {
		LOG_ERR("boot_set_next failed (err %d)", err);
	}

	flash_area_close(fap);

	return err;
}

static int confirm_mcuboot_image(uint8_t fa)
{
	int err;
	const struct flash_area *fap;

	err = flash_area_open(fa, &fap);
	if (err) {
		LOG_ERR("flash_area_open failed (err %d)", err);
		return err;
	}

	err = boot_set_next(fap, true, true);
	if (err) {
		LOG_ERR("boot_set_next failed (err %d)", err);
	}

	flash_area_close(fap);

	return err;
}

static int fmna_uarp_writer_mcuboot_transfer_start(void *ctx, size_t payload_size)
{
	int err;
	const struct fmna_uarp_writer_mcuboot_ctx *context = ctx;

	if (!context) {
		LOG_ERR("Invalid context");
		return -EINVAL;
	}

	if (!is_confirmed_mcuboot_image(context->running_fa_id)) {
		LOG_ERR("Currently running MCUboot image has not been confirmed");
		return -EINVAL;
	} else {
		LOG_INF("Currently running MCUboot image is confirmed");
	}

	/* Ensure that payload are sequentially processed. */
	if (!atomic_cas(&in_progress, false, true)) {
		LOG_ERR("Previous transfer has not been finished");
		return -EBUSY;
	}

	err = fmna_uarp_writer_util_nvm_start(&nvm_util_ctx,
					      context->write_fa_id,
					      buf,
					      ARRAY_SIZE(buf),
					      payload_size);
	if (err) {
		LOG_ERR("fmna_uarp_writer_util_nvm_start failed, err %d", err);
		atomic_set(&in_progress, false);
		return err;
	}

	return 0;
}

static int fmna_uarp_writer_mcuboot_transfer_write(void *ctx,
						   const uint8_t *chunk,
						   size_t chunk_size)
{
	int err;

	ARG_UNUSED(ctx);

	/* Ensure that writer has been started. */
	if (!atomic_get(&in_progress)) {
		LOG_ERR("Transfer has not been started");
		return -EBUSY;
	}

	err = fmna_uarp_writer_util_nvm_write(&nvm_util_ctx, chunk, chunk_size);
	if (err) {
		LOG_ERR("fmna_uarp_writer_util_nvm_write failed, err %d", err);
		return err;
	}

	return 0;
}

static int fmna_uarp_writer_mcuboot_transfer_finish(void *ctx, bool success)
{
	int err;
	const struct fmna_uarp_writer_mcuboot_ctx *context = ctx;

	if (!context) {
		LOG_ERR("Invalid context");
		return -EINVAL;
	}

	/* Ensure that writer has been started. */
	if (!atomic_get(&in_progress)) {
		LOG_ERR("Transfer has not been started");
		return -EBUSY;
	}

	err = fmna_uarp_writer_util_nvm_finish(&nvm_util_ctx, success);
	if (err) {
		LOG_ERR("fmna_uarp_writer_util_nvm_finish failed, err %d", err);
	} else if (success) {
		err = test_mcuboot_image(context->write_fa_id);
		if (err) {
			LOG_ERR("test_mcuboot_image failed, err %d", err);
		}
	}

	atomic_set(&in_progress, false);

	return err;
}

static int fmna_uarp_writer_mcuboot_image_confirm(void *ctx)
{
	int err;
	const struct fmna_uarp_writer_mcuboot_ctx *context = ctx;

	if (!context) {
		LOG_ERR("Invalid context");
		return -EINVAL;
	}

	err = confirm_mcuboot_image(context->running_fa_id);
	if (err) {
		LOG_ERR("confirm_mcuboot_image failed, err %d", err);
	}

	return err;
}

FMNA_UARP_WRITER_API_DEF(fmna_uarp_writer_mcuboot_api,
			 fmna_uarp_writer_mcuboot_transfer_start,
			 fmna_uarp_writer_mcuboot_transfer_write,
			 fmna_uarp_writer_mcuboot_transfer_finish,
			 fmna_uarp_writer_mcuboot_image_confirm);
