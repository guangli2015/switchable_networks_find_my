/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <app_version.h>

#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#include <fmna.h>

#include <zephyr/settings/settings.h>

#include "app_dfu.h"
#include "app_factory_reset.h"
#include "app_network_selector.h"
#include "app_ui.h"
#include "app_ui_selected.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(apple, LOG_LEVEL_INF);

#define INIT_SEM_TIMEOUT (60)

#define FMNA_PEER_SOUND_DURATION K_SECONDS(5)
#define FMNA_UT_SOUND_DURATION   K_SECONDS(1)

#define FMNA_BATTERY_LEVEL 100

static bool paired;
static bool pairing_mode;
static bool factory_reset_requested;

static void sound_timeout_work_handle(struct k_work *item);
static K_WORK_DELAYABLE_DEFINE(sound_timeout_work, sound_timeout_work_handle);

static void init_work_handle(struct k_work *work);

static K_SEM_DEFINE(init_work_sem, 0, 1);
static K_WORK_DEFINE(init_work, init_work_handle);

BUILD_ASSERT((APP_VERSION_MAJOR == CONFIG_FMNA_FIRMWARE_VERSION_MAJOR) &&
	     (APP_VERSION_MINOR == CONFIG_FMNA_FIRMWARE_VERSION_MINOR) &&
	     (APP_PATCHLEVEL == CONFIG_FMNA_FIRMWARE_VERSION_REVISION),
	     "Firmware version mismatch. Update the Find My FW version in the Kconfig file "
	     "to be aligned with the VERSION file.");

BUILD_ASSERT(!IS_ENABLED(CONFIG_MCUMGR_GRP_ZBASIC_STORAGE_ERASE),
	     "Storage erase is not allowed as it could lead to network provisioning data loss");

static void sound_stop_indicate(void)
{
	union app_ui_state state = {.selected = APP_UI_SELECTED_STATE_RINGING};

	LOG_INF("Stopping the sound from being played");

	app_ui_state_change_indicate(state, false);
}

static void sound_timeout_work_handle(struct k_work *item)
{
	int err;

	err = fmna_sound_completed_indicate();
	if (err) {
		LOG_ERR("fmna_sound_completed_indicate failed (err %d)", err);
		return;
	}

	LOG_INF("Sound playing timed out");

	sound_stop_indicate();
}

static void sound_start(enum fmna_sound_trigger sound_trigger)
{
	k_timeout_t sound_timeout;
	union app_ui_state state = {.selected = APP_UI_SELECTED_STATE_RINGING};

	if (sound_trigger == FMNA_SOUND_TRIGGER_UT_DETECTION) {
		LOG_INF("Play sound action triggered by the Unwanted Tracking "
		       "Detection");

		sound_timeout = FMNA_UT_SOUND_DURATION;
	} else {
		LOG_INF("Received a request from FMN to start playing sound "
		       "from the connected peer");

		sound_timeout = FMNA_PEER_SOUND_DURATION;
	}
	k_work_reschedule(&sound_timeout_work, sound_timeout);

	app_ui_state_change_indicate(state, true);

	LOG_INF("Starting to play sound...");
}

static void sound_stop(void)
{
	LOG_INF("Received a request from FMN to stop playing sound");

	k_work_cancel_delayable(&sound_timeout_work);

	sound_stop_indicate();
}

static const struct fmna_sound_cb sound_callbacks = {
	.sound_start = sound_start,
	.sound_stop = sound_stop,
};

static void serial_number_lookup_exited(void)
{
	union app_ui_state state = {.selected = APP_UI_SELECTED_STATE_ID_MODE};

	LOG_INF("FMN Serial Number lookup exited");

	app_ui_state_change_indicate(state, false);
}

static const struct fmna_serial_number_lookup_cb sn_lookup_callbacks = {
	.exited = serial_number_lookup_exited,
};

static void battery_level_request(void)
{
	/* No need to implement because the simulated battery level
	 * is always up to date.
	 */

	LOG_INF("Battery level request");
}

static void pairing_failed(void)
{
	LOG_ERR("FMN pairing has failed");
}

static void pairing_mode_exited(void)
{
	union app_ui_state state = {.selected = APP_UI_SELECTED_STATE_ADVERTISING};

	LOG_INF("Exited the FMN pairing mode");

	pairing_mode = false;

	app_ui_state_change_indicate(state, pairing_mode);
}

static void paired_state_changed(bool new_paired_state)
{
	union app_ui_state state_adv = {.selected = APP_UI_SELECTED_STATE_ADVERTISING};
	union app_ui_state state_provisioned = {.selected = APP_UI_SELECTED_STATE_PROVISIONED};

	LOG_INF("The FMN accessory transitioned to the %spaired state",
	       new_paired_state ? "" : "un");

	if (paired && !new_paired_state) {
		/* The device has been unpaired, return to the network selector. */
		app_factory_reset_schedule(K_NO_WAIT);
	}

	paired = new_paired_state;

	if (new_paired_state) {
		pairing_mode = false;
		app_ui_state_change_indicate(state_adv, pairing_mode);
	}

	app_ui_state_change_indicate(state_provisioned, new_paired_state);
}

static const struct fmna_info_cb info_callbacks = {
	.battery_level_request = battery_level_request,
	.pairing_failed = pairing_failed,
	.pairing_mode_exited = pairing_mode_exited,
	.paired_state_changed = paired_state_changed,
};
extern  bool identifying_info_allow(struct bt_conn *conn, const struct bt_uuid *uuid);
static bool gatt_authorize(struct bt_conn *conn, const struct bt_gatt_attr *attr)
{
	bool authorized = true;

	if (IS_ENABLED(CONFIG_APP_DFU)) {
		authorized = authorized && app_dfu_bt_gatt_operation_allow(attr->uuid);
	}

	struct bt_conn_info info;
    int err = bt_conn_get_info(conn, &info);
    if (err) {
        LOG_ERR("Failed to get conn info: %d", err);
        return -EACCES;
    }

    uint8_t adv_id = info.id;
	if(adv_id == 0)
	{
		authorized = authorized && identifying_info_allow(conn, attr->uuid);
	}

	return authorized;
}

static const struct bt_gatt_authorization_cb gatt_authorization_callbacks = {
	.read_authorize = gatt_authorize,
	.write_authorize = gatt_authorize,
};

 int factory_reset_perform_apple(void)
{
	int ret;
	LOG_INF("aaple_factory_reset_perform###");
	/* Pre-actions. */
	if (IS_ENABLED(CONFIG_APP_DFU)) {
		app_dfu_mode_exit();
	}

	if (fmna_is_ready()) {
		ret = fmna_disable();
		if (ret) {
			LOG_ERR("Factory Reset: fmna_disable failed (err %d)", ret);
			return ret;
		}
	}

	/* Factory reset. */
	ret = fmna_factory_reset();
	if (ret) {
		LOG_ERR("Factory Reset: fmna_factory_reset failed (err %d)", ret);
		return ret;
	}

	/* Post-actions. */
	/* None. */

	return 0;
}

static int bt_id_initialize(void)
{
	int err;

	err = fmna_id_set(CONFIG_APP_NETWORK_BT_ID);
	if (err) {
		LOG_ERR("fmna_id_set failed (err %d)", err);
		return err;
	}

	return 0;
}

SYS_INIT(bt_id_initialize, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

static int fmna_initialize(void)
{
	int err;

	err = fmna_sound_cb_register(&sound_callbacks);
	if (err) {
		LOG_ERR("fmna_sound_cb_register failed (err %d)", err);
		return err;
	}

	err = fmna_serial_number_lookup_cb_register(&sn_lookup_callbacks);
	if (err) {
		LOG_ERR("fmna_serial_number_lookup_cb_register failed (err %d)", err);
		return err;
	}

	err = fmna_battery_level_set(FMNA_BATTERY_LEVEL);
	if (err) {
		LOG_ERR("fmna_battery_level_set failed (err %d)", err);
		return err;
	}

	err = fmna_info_cb_register(&info_callbacks);
	if (err) {
		LOG_ERR("fmna_info_cb_register failed (err %d)", err);
		return err;
	}

	err = fmna_enable();
	if (err) {
		LOG_ERR("fmna_enable failed (err %d)", err);
		return err;
	}

	return err;
}

static void adv_resume_action_handle(void)
{
	int err;
	union app_ui_state state = {.selected = APP_UI_SELECTED_STATE_ADVERTISING};

	if (!paired) {
		err = fmna_pairing_mode_enter();
		if (err) {
			LOG_ERR("Cannot enter the FMN pairing mode (err: %d)", err);
		} else {
			LOG_INF("%s the FMN pairing mode",
			       pairing_mode ? "Extending" : "Enabling");

			pairing_mode = true;
			app_ui_state_change_indicate(state, pairing_mode);
		}
	}
}

static void ui_request_handle(union app_ui_request request)
{
	int err;
	union app_ui_state state_id_mode = {.selected = APP_UI_SELECTED_STATE_ID_MODE};

	if (request.selected == APP_UI_SELECTED_REQUEST_ADVERTISING_MODE_CHANGE) {
		adv_resume_action_handle();
	} else if (request.selected == APP_UI_SELECTED_REQUEST_ID_MODE_ENTER) {
		err = fmna_serial_number_lookup_enable();
		if (err) {
			LOG_ERR("Cannot enable FMN Serial Number lookup (err: %d)", err);
		} else {
			LOG_INF("FMN Serial Number lookup enabled");
			app_ui_state_change_indicate(state_id_mode, true);
		}
	} else if (request.selected == APP_UI_SELECTED_REQUEST_FACTORY_RESET) {
		factory_reset_requested = true;
	} else if (request.selected == APP_UI_SELECTED_REQUEST_DFU_MODE_ENTER) {
		if (IS_ENABLED(CONFIG_APP_DFU)) {
			app_dfu_mode_enter(false);
		}
	}
}

static void dfu_mode_state_changed(bool enabled)
{
	union app_ui_state state = {.selected = APP_UI_SELECTED_STATE_DFU_MODE};

	app_ui_state_change_indicate(state, enabled);
}

static const struct app_dfu_cb dfu_cbs = {
	.state_changed = dfu_mode_state_changed,
};

static int dfu_init(void)
{
	int err;
	size_t id_count = 0;

	/* Validate if the Bluetooth identity was already created by the loading code. */
	bt_id_get(NULL, &id_count);
	__ASSERT_NO_MSG(id_count > CONFIG_APP_DFU_BT_ID);

	err = app_dfu_bt_id_set(CONFIG_APP_DFU_BT_ID);
	if (err) {
		LOG_ERR("app_dfu_bt_id_set failed (err %d)", err);
		return err;
	}

	err = app_dfu_cb_register(&dfu_cbs);
	if (err) {
		LOG_ERR("app_dfu_cb_register failed (err %d)", err);
		return err;
	}

	err = app_dfu_init();
	if (err) {
		LOG_ERR("app_dfu_init failed (err %d)", err);
		return err;
	}

	if (!app_dfu_is_confirmed()) {
		LOG_INF("DFU: The current image is not confirmed, entering the DFU mode to "
			"allow confirm operation");
		app_dfu_mode_enter(false);
	}

	return 0;
}

static void init_work_handle(struct k_work *work)
{
	int err;
	size_t id_count = 0;

	/* Validate if the Bluetooth identity was already created by the loading code. */
	bt_id_get(NULL, &id_count);
	__ASSERT_NO_MSG(id_count > CONFIG_APP_NETWORK_BT_ID);

	err = app_ui_mode_set(APP_UI_MODE_SELECTED_APPLE);
	if (err) {
		LOG_ERR("Failed to set the Apple UI mode (err %d)", err);
		return;
	}

	if (IS_ENABLED(CONFIG_APP_DFU)) {
		err = dfu_init();
		if (err) {
			LOG_ERR("dfu_init failed (err %d)", err);
			return;
		}
	}

	err = app_factory_reset_init(factory_reset_requested);
	if (err) {
		LOG_ERR("FMDN: app_factory_reset_init failed (err %d)", err);
		return;
	}

	err = bt_gatt_authorization_cb_register(&gatt_authorization_callbacks);
	if (err) {
		LOG_ERR("Registering GATT authorization callbacks failed (err %d)", err);
		return;
	}

	err = fmna_initialize();
	if (err) {
		LOG_ERR("FMNA init failed (err %d)", err);
		return;
	}

	/* Auto enable the FMNA pairing advertising on bootup. */
	adv_resume_action_handle();

	k_sem_give(&init_work_sem);
}

void app_network_apple_run(void)
{
	int err;
	union app_ui_state state = {.selected = APP_UI_SELECTED_STATE_APP_RUNNING};

	LOG_INF("Starting the Apple Find My network");

	(void) k_work_submit(&init_work);
	err = k_sem_take(&init_work_sem, K_SECONDS(INIT_SEM_TIMEOUT));
	if (err) {
		k_panic();
	}

	app_ui_state_change_indicate(state, true);
}

APP_UI_REQUEST_LISTENER_REGISTER(ui_network_apple,
				 APP_UI_MODE_SELECTED_APPLE,
				 ui_request_handle);

APP_NETWORK_SELECTOR_DESC_REGISTER(network_apple,
				   APP_NETWORK_SELECTOR_APPLE,
				   app_network_apple_run,
				   factory_reset_perform_apple);
