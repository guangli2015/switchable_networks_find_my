/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <app_version.h>

#include <zephyr/kernel.h>

#include <zephyr/bluetooth/uuid.h>

#include <zephyr/dfu/mcuboot.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>

#include "app_dfu.h"
#include "app_dfu_bt_adv.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(dfu, LOG_LEVEL_INF);

#define DFU_MODE_TIMEOUT_MIN (5)

static bool initialized;

static bool dfu_mode;
static bool persistent;

static const struct app_dfu_cb *registered_cb;

static void dfu_mode_timeout_work_handle(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(dfu_mode_timeout_work, dfu_mode_timeout_work_handle);

static void dfu_mode_change(bool new_mode)
{
	if (dfu_mode == new_mode) {
		return;
	}

	LOG_INF("DFU: Mode %sabled", new_mode ? "en" : "dis");

	dfu_mode = new_mode;

	app_dfu_bt_adv_manage(dfu_mode);

	if (!app_dfu_is_confirmed() && !dfu_mode) {
		LOG_WRN("DFU: The current image has not been confirmed");
		LOG_WRN("DFU: The old image will be restored during the next system reboot "
			"unless the new image is confirmed");
	}

	if (registered_cb && registered_cb->state_changed) {
		registered_cb->state_changed(dfu_mode);
	}
}

bool app_dfu_bt_gatt_operation_allow(const struct bt_uuid *uuid)
{
	if (!initialized) {
		return true;
	}

	if (bt_uuid_cmp(uuid, SMP_BT_CHR_UUID) != 0) {
		return true;
	}

	if (!dfu_mode) {
		LOG_WRN("DFU: SMP characteristic access denied, DFU mode is not active");
		return false;
	}

	if (!persistent) {
		(void) k_work_reschedule(&dfu_mode_timeout_work, K_MINUTES(DFU_MODE_TIMEOUT_MIN));
	}

	return true;
}

int app_dfu_bt_id_set(uint8_t bt_id)
{
	int err;

	if (initialized) {
		LOG_ERR("DFU: Cannot change the Bluetooth identity after initialization");
		return -EACCES;
	}

	err = app_dfu_bt_adv_id_set(bt_id);
	if (err) {
		LOG_ERR("DFU: app_dfu_bt_adv_id_set failed (err %d)", err);
		return err;
	}

	return 0;
}

static void dfu_mode_timeout_work_handle(struct k_work *work)
{
	LOG_INF("DFU: Timeout expired");

	dfu_mode_change(false);
}

void app_dfu_mode_enter(bool persistent_mode)
{
	if (!initialized) {
		return;
	}

	if (dfu_mode) {
		if (persistent) {
			LOG_WRN("DFU: DFU mode is already active");
		} else {
			LOG_INF("DFU: Refreshing the DFU mode timeout");

			(void) k_work_reschedule(&dfu_mode_timeout_work,
						 K_MINUTES(DFU_MODE_TIMEOUT_MIN));
		}

		return;
	}

	persistent = persistent_mode;

	if (persistent) {
		LOG_INF("DFU: Enterning the DFU mode in the persistent mode");
	} else {
		LOG_INF("DFU: Entering the DFU mode for %d minute(s)", DFU_MODE_TIMEOUT_MIN);

		(void) k_work_reschedule(&dfu_mode_timeout_work, K_MINUTES(DFU_MODE_TIMEOUT_MIN));
	}

	dfu_mode_change(true);
}

void app_dfu_mode_exit(void)
{
	if (!initialized) {
		return;
	}

	if (!dfu_mode) {
		return;
	}

	LOG_INF("DFU: Exiting the DFU mode");

	if (!persistent) {
		(void) k_work_cancel_delayable(&dfu_mode_timeout_work);
	}

	dfu_mode_change(false);
}

bool app_dfu_is_confirmed(void)
{
	BUILD_ASSERT(IS_ENABLED(CONFIG_BOOTLOADER_MCUBOOT));

	if (IS_ENABLED(CONFIG_BOOTLOADER_MCUBOOT)) {
		return boot_is_img_confirmed();
	}

	/* Should not happen */
	__ASSERT(0, "Unsupported bootloader");

	return false;
}

static enum mgmt_cb_return image_confirmed_cb(uint32_t event,
					      enum mgmt_cb_return prev_status,
					      int32_t *rc, uint16_t *group,
					      bool *abort_more, void *data,
					      size_t data_size)
{
	LOG_INF("DFU: Image confirmed");

	if (registered_cb && registered_cb->image_confirmed) {
		registered_cb->image_confirmed();
	}

	return MGMT_CB_OK;
}

static struct mgmt_callback mgmt_callback = {
	.callback = image_confirmed_cb,
	.event_id = MGMT_EVT_OP_IMG_MGMT_DFU_CONFIRMED,
};

int app_dfu_cb_register(const struct app_dfu_cb *cb)
{
	__ASSERT_NO_MSG(!initialized);

	__ASSERT_NO_MSG(!registered_cb);
	__ASSERT_NO_MSG(cb);

	registered_cb = cb;

	return 0;
}

int app_dfu_init(void)
{
	int err;

	if (initialized) {
		return 0;
	}

	LOG_INF("DFU: Firmware version: %d.%d.%d", APP_VERSION_MAJOR,
						   APP_VERSION_MINOR,
						   APP_PATCHLEVEL);

	mgmt_callback_register(&mgmt_callback);

	err = app_dfu_bt_adv_init();
	if (err) {
		LOG_ERR("app_dfu_bt_adv_init failed (err %d)", err);
		return err;
	}

	initialized = true;

	return 0;
}
