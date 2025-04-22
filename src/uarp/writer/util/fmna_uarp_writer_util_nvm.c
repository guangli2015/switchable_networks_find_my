/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>

#include <dfu/dfu_target_stream.h>

#include "fmna_uarp_writer_util_nvm.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(fmna_uarp_writer_util_nvm, CONFIG_FMNA_UARP_WRITER_UTIL_NVM_LOG_LEVEL);

BUILD_ASSERT(!IS_ENABLED(DFU_TARGET_STREAM_SAVE_PROGRESS),
	     "FMNA UARP does not support DFU target progress saving.");

static int init_util_nvm(struct fmna_uarp_writer_util_nvm_ctx *ctx,
			 uint8_t *buf,
			 size_t buf_len,
			 size_t payload_size)
{
	int err;
	struct dfu_target_stream_init init = {
		.id = "fmna_uarp_writer_util_nvm",
		.fdev = flash_area_get_device(ctx->fap),
		.buf = buf,
		.len = buf_len,
		.offset = ctx->fap->fa_off,
		.size = ctx->fap->fa_size,
		.cb = NULL
	};

	if (payload_size > ctx->fap->fa_size) {
		LOG_ERR("Payload too big for flash area, payload_size %zu, fa_size %zu",
			payload_size, ctx->fap->fa_size);
		return -EFBIG;
	}

	err = dfu_target_stream_init(&init);
	if (err) {
		LOG_ERR("dfu_target_stream_init failed, err %d", err);
		return err;
	}

	return 0;
}

int fmna_uarp_writer_util_nvm_start(struct fmna_uarp_writer_util_nvm_ctx *ctx,
				    uint8_t fa_id,
				    uint8_t *buf,
				    size_t buf_len,
				    size_t payload_size)
{
	int err;

	if (!ctx || !buf || (buf_len == 0)) {
		return -EINVAL;
	}

	__ASSERT_NO_MSG(ctx->fap == NULL);
	err = flash_area_open(fa_id, &ctx->fap);
	if (err) {
		LOG_ERR("flash_area_open failed, err %d", err);
		ctx->fap = NULL;
		return err;
	}

	err = init_util_nvm(ctx, buf, buf_len, payload_size);
	if (err) {
		flash_area_close(ctx->fap);
		ctx->fap = NULL;
		return err;
	}

	return 0;
}

int fmna_uarp_writer_util_nvm_write(struct fmna_uarp_writer_util_nvm_ctx *ctx,
				    const uint8_t *chunk,
				    size_t chunk_size)
{
	int err;

	err = dfu_target_stream_write(chunk, chunk_size);
	if (err) {
		LOG_ERR("dfu_target_stream_write failed, err %d", err);
		return err;
	}

	return 0;
}

int fmna_uarp_writer_util_nvm_finish(struct fmna_uarp_writer_util_nvm_ctx *ctx, bool success)
{
	int err;

	if (!ctx) {
		return -EINVAL;
	}

	if (success) {
		err = dfu_target_stream_done(true);
	} else {
		err = dfu_target_stream_reset();
	}

	if (err) {
		LOG_ERR("dfu_target_stream_%s failed, err %d", (success ? "done" : "reset"), err);
	}

	__ASSERT_NO_MSG(ctx->fap != NULL);
	flash_area_close(ctx->fap);
	ctx->fap = NULL;

	return err;
}
