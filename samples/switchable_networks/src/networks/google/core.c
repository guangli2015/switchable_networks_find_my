/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <app_version.h>

#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#include <bluetooth/services/fast_pair/fast_pair.h>
#include <bluetooth/services/fast_pair/fmdn.h>

#include "app_dfu.h"
#include "app_factory_reset.h"
#include "app_fp_adv.h"
#include "app_network_selector.h"
#include "app_ring.h"
#include "app_ui.h"
#include "app_ui_selected.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(google, LOG_LEVEL_INF);

/* Semaphore timeout in seconds. */
#define INIT_SEM_TIMEOUT (60)

/* Factory reset delay in seconds since the trigger operation. */
#define FACTORY_RESET_DELAY (3)

/* FMDN provisioning timeout in minutes as recommended by the specification. */
#define FMDN_PROVISIONING_TIMEOUT (5)

/* FMDN identification mode timeout in minutes. */
#define FMDN_ID_MODE_TIMEOUT (CONFIG_DULT_ID_READ_STATE_TIMEOUT)

#define FMDN_BATTERY_LEVEL 100

/* FMDN advertising interval 2s (0x0C80 in hex). */
#define FMDN_ADV_INTERVAL (0x0C80)

/* Factory reset reason. */
enum factory_reset_trigger {
	FACTORY_RESET_TRIGGER_NONE,
	FACTORY_RESET_TRIGGER_KEY_STATE_MISMATCH,
	FACTORY_RESET_TRIGGER_PROVISIONING_TIMEOUT,
};

static bool fmdn_provisioned;
static bool fmdn_id_mode;
static bool fp_account_key_present;
static bool fp_adv_ui_request;
static bool factory_reset_requested;

/* Bitmask of connections authenticated by the FMDN extension. */
static uint32_t fmdn_conn_auth_bm;

/* Size in bits of the authenticated connection bitmask. */
#define FMDN_CONN_AUTH_BM_BIT_SIZE (__CHAR_BIT__ * sizeof(fmdn_conn_auth_bm))

/* Used to trigger the clock synchronization after the bootup if provisioned. */
APP_FP_ADV_TRIGGER_REGISTER(fp_adv_trigger_clock_sync, "clock_sync");

/* Used to wait for the FMDN provisioning procedure after the Account Key write. */
APP_FP_ADV_TRIGGER_REGISTER(fp_adv_trigger_fmdn_provisioning, "fmdn_provisioning");

/* Used to manually request the FP advertising by the user.
 * Enabled by default on bootup if not provisioned.
 */
APP_FP_ADV_TRIGGER_REGISTER(fp_adv_trigger_ui, "ui");

static bool factory_reset_executed;
static enum factory_reset_trigger factory_reset_trigger;

static void init_work_handle(struct k_work *work);

static K_SEM_DEFINE(init_work_sem, 0, 1);
static K_WORK_DEFINE(init_work, init_work_handle);

BUILD_ASSERT((APP_VERSION_MAJOR == CONFIG_BT_FAST_PAIR_FMDN_DULT_FIRMWARE_VERSION_MAJOR) &&
	     (APP_VERSION_MINOR == CONFIG_BT_FAST_PAIR_FMDN_DULT_FIRMWARE_VERSION_MINOR) &&
	     (APP_PATCHLEVEL == CONFIG_BT_FAST_PAIR_FMDN_DULT_FIRMWARE_VERSION_REVISION),
	     "Firmware version mismatch. Update the DULT FW version in the Kconfig file "
	     "to be aligned with the VERSION file.");

static void fmdn_factory_reset_prepare(void)
{
	/* Disable advertising requests related to the FMDN. */
	app_fp_adv_request(&fp_adv_trigger_fmdn_provisioning, false);
	app_fp_adv_request(&fp_adv_trigger_clock_sync, false);

	/* Disable advertising request from the UI. */
	fp_adv_ui_request = false;
	app_fp_adv_request(&fp_adv_trigger_ui, fp_adv_ui_request);

	if (IS_ENABLED(CONFIG_APP_DFU)) {
		app_dfu_mode_exit();
	}

	app_fp_adv_rpa_rotation_suspend(false);
}

static void fmdn_factory_reset_executed(void)
{
	/* Clear the trigger state for the scheduled factory reset operations. */
	factory_reset_trigger = FACTORY_RESET_TRIGGER_NONE;
	factory_reset_executed = true;
}

static int factory_reset_perform_google(void)
{
	int ret;

	/* Pre-actions. */
	fmdn_factory_reset_prepare();

	if (app_fp_adv_is_ready()) {
		ret = app_fp_adv_disable();
		if (ret) {
			LOG_ERR("Factory Reset: app_fp_adv_disable failed (err %d)", ret);
			return ret;
		}
	}

	if (bt_fast_pair_is_ready()) {
		ret = bt_fast_pair_disable();
		if (ret) {
			LOG_ERR("Factory Reset: bt_fast_pair_disable failed: %d", ret);
			return ret;
		}
	}

	/* Factory reset. */
	ret = bt_fast_pair_factory_reset();
	if (ret) {
		LOG_ERR("Factory Reset: bt_fast_pair_factory_reset failed: %d", ret);
		return ret;
	}

	ret = bt_id_reset(CONFIG_APP_NETWORK_BT_ID, NULL, NULL);
	if (ret != CONFIG_APP_NETWORK_BT_ID) {
		LOG_ERR("Factory Reset: bt_id_reset failed: err %d", ret);
		return ret;
	}

	/* Post-actions. */
	fmdn_factory_reset_executed();

	return 0;
}

static void fmdn_factory_reset_schedule(enum factory_reset_trigger trigger, k_timeout_t delay)
{
	app_factory_reset_schedule(delay);
	factory_reset_trigger = trigger;
}

static void fmdn_factory_reset_cancel(void)
{
	app_factory_reset_cancel();
	factory_reset_trigger = FACTORY_RESET_TRIGGER_NONE;
}

static enum bt_security_err pairing_accept(struct bt_conn *conn,
					   const struct bt_conn_pairing_feat *const feat)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(feat);

	/* Fast Pair Implementation Guidelines for the locator tag use case:
	 * Provider should reject normal Bluetooth pairing attempts. It should
	 * only accept Fast Pair pairing.
	 */

	LOG_INF("Normal Bluetooth pairing not allowed");

	return BT_SECURITY_ERR_PAIR_NOT_ALLOWED;
}

static const struct bt_conn_auth_cb conn_auth_callbacks = {
	.pairing_accept = pairing_accept,
};

static void fmdn_conn_auth_bm_conn_status_set(const struct bt_conn *conn, bool auth_flag)
{
	uint8_t index;

	BUILD_ASSERT(CONFIG_BT_MAX_CONN <= FMDN_CONN_AUTH_BM_BIT_SIZE);

	index = bt_conn_index(conn);
	__ASSERT_NO_MSG(index < FMDN_CONN_AUTH_BM_BIT_SIZE);

	WRITE_BIT(fmdn_conn_auth_bm, index, auth_flag);
}

static bool fmdn_conn_auth_bm_conn_status_get(const struct bt_conn *conn)
{
	uint8_t index;

	index = bt_conn_index(conn);
	__ASSERT_NO_MSG(index < FMDN_CONN_AUTH_BM_BIT_SIZE);

	return (fmdn_conn_auth_bm & BIT(index));
}

 bool identifying_info_allow(struct bt_conn *conn, const struct bt_uuid *uuid)
{
	const struct bt_uuid *uuid_block_list[] = {
		/* Device Information service characteristics */
		BT_UUID_DIS_FIRMWARE_REVISION,
		/* GAP service characteristics */
		BT_UUID_GAP_DEVICE_NAME,
	};
	bool uuid_match = false;

	if (!fmdn_provisioned) {
		return true;
	}

	if (fmdn_conn_auth_bm_conn_status_get(conn)) {
		return true;
	}

	if (fmdn_id_mode) {
		return true;
	}

	for (size_t i = 0; i < ARRAY_SIZE(uuid_block_list); i++) {
		if (bt_uuid_cmp(uuid, uuid_block_list[i]) == 0) {
			uuid_match = true;
			break;
		}
	}

	if (!uuid_match) {
		return true;
	}

	LOG_INF("Rejecting operation on the identifying information");

	return false;
}

static bool gatt_authorize(struct bt_conn *conn, const struct bt_gatt_attr *attr)
{
	bool authorized = true;

	/* Fast Pair Implementation Guidelines for the locator tag use case:
	 * The Provider shouldn't expose any identifying information
	 * in an unauthenticated manner (e.g. names or identifiers).
	 */

	if (IS_ENABLED(CONFIG_APP_DFU)) {
		authorized = authorized && app_dfu_bt_gatt_operation_allow(attr->uuid);
	}

	authorized = authorized && identifying_info_allow(conn, attr->uuid);

	return authorized;
}

static const struct bt_gatt_authorization_cb gatt_authorization_callbacks = {
	.read_authorize = gatt_authorize,
	.write_authorize = gatt_authorize,
};

static void fp_account_key_written(struct bt_conn *conn)
{
	LOG_INF("Fast Pair: Account Key write");

	/* The first and only Account Key write starts the FMDN provisioning. */
	if (!fp_account_key_present) {
		app_fp_adv_request(&fp_adv_trigger_fmdn_provisioning, true);

		/* Fast Pair Implementation Guidelines for the locator tag use case:
		 * trigger the reset to factory settings if there is no FMDN
		 * provisioning operation within 5 minutes.
		 */
		fmdn_factory_reset_schedule(
			FACTORY_RESET_TRIGGER_PROVISIONING_TIMEOUT,
			K_MINUTES(FMDN_PROVISIONING_TIMEOUT));

		/* Fast Pair Implementation Guidelines for the locator tag use case:
		 * after the Provider was paired, it should not change its MAC address
		 * till FMDN is provisioned or till 5 minutes passes.
		 */
		app_fp_adv_rpa_rotation_suspend(true);
	}

	fp_account_key_present = bt_fast_pair_has_account_key();
}

static struct bt_fast_pair_info_cb fp_info_callbacks = {
	.account_key_written = fp_account_key_written,
};

static void fmdn_id_mode_exited(void)
{
	union app_ui_state state = {.selected = APP_UI_SELECTED_STATE_ID_MODE};

	LOG_INF("FMDN: identification mode exited");

	fmdn_id_mode = false;
	app_ui_state_change_indicate(state, fmdn_id_mode);
}

static void fmdn_read_mode_exited(enum bt_fast_pair_fmdn_read_mode mode)
{
	switch (mode) {
	case BT_FAST_PAIR_FMDN_READ_MODE_DULT_ID:
		fmdn_id_mode_exited();
		break;
	default:
		/* Unsupported mode: should not happen. */
		__ASSERT_NO_MSG(0);
		break;
	}
}

static const struct bt_fast_pair_fmdn_read_mode_cb fmdn_read_mode_cb = {
	.exited = fmdn_read_mode_exited,
};

static void fmdn_id_mode_action_handle(void)
{
	int err;
	union app_ui_state state = {.selected = APP_UI_SELECTED_STATE_ID_MODE};

	if (!fmdn_provisioned) {
		LOG_INF("FMDN: the identification mode is not available in the "
			"unprovisioned state. "
			"Identifying info can always be read in this state.");
		return;
	}

	if (fmdn_id_mode) {
		LOG_INF("FMDN: refreshing the identification mode timeout");
	} else {
		LOG_INF("FMDN: entering the identification mode for %d minute(s)",
			FMDN_ID_MODE_TIMEOUT);
	}

	err = bt_fast_pair_fmdn_read_mode_enter(BT_FAST_PAIR_FMDN_READ_MODE_DULT_ID);
	if (err) {
		LOG_ERR("FMDN: failed to enter the identification mode: err=%d", err);
		return;
	}

	/* Fast Pair Implementation Guidelines for the locator tag use case:
	 * The Provider shouldn't expose any identifying information
	 * in an unauthenticated manner (e.g. names or identifiers).
	 *
	 * The DULT identification mode is also used to allow reading of Bluetooth
	 * characteristics with identifying information for a limited time in the
	 * provisioned state.
	 */
	fmdn_id_mode = true;
	app_ui_state_change_indicate(state, fmdn_id_mode);
}

static void ui_request_handle(union app_ui_request request)
{
	/* It is assumed that the callback executes in the cooperative
	 * thread context as it interacts with the FMDN API.
	 */
	__ASSERT_NO_MSG(!k_is_preempt_thread());
	__ASSERT_NO_MSG(!k_is_in_isr());

	if (request.selected == APP_UI_SELECTED_REQUEST_ID_MODE_ENTER) {
		fmdn_id_mode_action_handle();
	} else if (request.selected == APP_UI_SELECTED_REQUEST_ADVERTISING_MODE_CHANGE) {
		fp_adv_ui_request = !fp_adv_ui_request;
		app_fp_adv_request(&fp_adv_trigger_ui, fp_adv_ui_request);
	} else if (request.selected == APP_UI_SELECTED_REQUEST_FACTORY_RESET) {
		factory_reset_requested = true;
	} else if (request.selected == APP_UI_SELECTED_REQUEST_DFU_MODE_ENTER) {
		if (IS_ENABLED(CONFIG_APP_DFU)) {
			app_dfu_mode_enter(false);
		}
	}
}

static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
	if ((err != BT_SECURITY_ERR_SUCCESS) || (level < BT_SECURITY_L2)) {
		return;
	}

	LOG_INF("FMDN: connection authenticated using the Bluetooth bond: %p", (void *)conn);

	/* Connection authentication flow #1 to allow read operation of the DIS Firmware Revision
	 * characteristic according to the Firmware updates section from the FMDN Accessory
	 * specification:
	 * The connection is validated using the previously established Bluetooth bond (available
	 * only for locator tags that create a Bluetooth bond during the Fast Pair procedure).
	 */
	fmdn_conn_auth_bm_conn_status_set(conn, true);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	fmdn_conn_auth_bm_conn_status_set(conn, false);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.security_changed = security_changed,
	.disconnected = disconnected,
};

static void fmdn_clock_synced(void)
{
	LOG_INF("FMDN: clock information synchronized with the authenticated Bluetooth peer");

	if (fmdn_provisioned) {
		/* Fast Pair Implementation Guidelines for the locator tag use case:
		 * After a power loss, the device should advertise non-discoverable
		 * Fast Pair frames until the next invocation of read beacon parameters.
		 * This lets the Seeker detect the device and synchronize the clock even
		 * if a significant clock drift occurred.
		 */
		app_fp_adv_request(&fp_adv_trigger_clock_sync, false);
	}
}

static void fmdn_conn_authenticated(struct bt_conn *conn)
{
	LOG_INF("FMDN: connection authenticated using Beacon Actions command: %p", (void *)conn);

	/* Connection authentication flow #2 to allow read operation of the DIS Firmware Revision
	 * characteristic according to the Firmware updates section from the FMDN Accessory
	 * specification:
	 * The connection is validated by the FMDN module using the authentication mechanism for
	 * commands sent over the Beacon Actions characteristic and the Fast Pair service
	 * (available only for locator tags that do not create a Bluetooth bond during the Fast
	 * Pair procedure).
	 */
	fmdn_conn_auth_bm_conn_status_set(conn, true);
}

static bool fmdn_provisioning_state_is_first_cb_after_bootup(void)
{
	static bool first_cb_after_bootup = true;
	bool is_first_cb_after_bootup = first_cb_after_bootup;

	first_cb_after_bootup = false;

	return is_first_cb_after_bootup;
}

static void fmdn_provisioning_state_changed(bool provisioned)
{
	bool clock_sync_required = fmdn_provisioning_state_is_first_cb_after_bootup() &&
				   provisioned;
	union app_ui_state state = {.selected = APP_UI_SELECTED_STATE_PROVISIONED};

	LOG_INF("FMDN: state changed to %s",
		provisioned ? "provisioned" : "unprovisioned");

	app_ui_state_change_indicate(state, provisioned);
	fmdn_provisioned = provisioned;

	/* Fast Pair Implementation Guidelines for the locator tag use case:
	 * cancel the provisioning timeout.
	 */
	if (provisioned &&
	    (factory_reset_trigger == FACTORY_RESET_TRIGGER_PROVISIONING_TIMEOUT)) {
		fmdn_factory_reset_cancel();
		app_fp_adv_rpa_rotation_suspend(false);
	}

	/* Fast Pair Implementation Guidelines for the locator tag use case:
	 * trigger the reset to factory settings on the unprovisioning operation
	 * or on the loss of the Owner Account Key.
	 */
	fp_account_key_present = bt_fast_pair_has_account_key();
	if (fp_account_key_present != provisioned) {
		/* Delay the factory reset operation to allow the local device
		 * to send a response to the unprovisioning command and give
		 * the connected peer necessary time to finalize its operations
		 * and shutdown the connection.
		 */
		fmdn_factory_reset_schedule(
			FACTORY_RESET_TRIGGER_KEY_STATE_MISMATCH,
			K_SECONDS(FACTORY_RESET_DELAY));
		return;
	}

	/* Triggered on the unprovisioning operation. */
	if (factory_reset_executed) {
		LOG_INF("The device has been reset to factory settings");
		LOG_INF("Please press a button to put the device in the Fast Pair discoverable "
			"advertising mode");

		factory_reset_executed = false;
		return;
	}

	app_fp_adv_request(&fp_adv_trigger_clock_sync, clock_sync_required);
	app_fp_adv_request(&fp_adv_trigger_fmdn_provisioning, false);

	fp_adv_ui_request = !provisioned;
	app_fp_adv_request(&fp_adv_trigger_ui, fp_adv_ui_request);
}

static struct bt_fast_pair_fmdn_info_cb fmdn_info_cb = {
	.clock_synced = fmdn_clock_synced,
	.conn_authenticated = fmdn_conn_authenticated,
	.provisioning_state_changed = fmdn_provisioning_state_changed,
};

static void fp_adv_state_changed(bool enabled)
{
	union app_ui_state state = {.selected = APP_UI_SELECTED_STATE_ADVERTISING};

	app_ui_state_change_indicate(state, enabled);
}

static const struct app_fp_adv_cb fp_adv_cbs = {
	.state_changed = fp_adv_state_changed,
};

static int bt_id_initialize(void)
{
	int err;

	err = app_fp_adv_id_set(CONFIG_APP_NETWORK_BT_ID);
	if (err) {
		LOG_ERR("Fast Pair: app_fp_adv_id_set failed (err %d)", err);
		return err;
	}

	err = bt_fast_pair_fmdn_id_set(CONFIG_APP_NETWORK_BT_ID);
	if (err) {
		LOG_ERR("FMDN: bt_fast_pair_fmdn_id_set failed (err %d)", err);
		return err;
	}

	return 0;
}

SYS_INIT(bt_id_initialize, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

static int fast_pair_prepare(void)
{
	int err;

	err = app_fp_adv_init(&fp_adv_cbs);
	if (err) {
		LOG_ERR("Fast Pair: app_fp_adv_init failed (err %d)", err);
		return err;
	}

	return 0;
}

static int fmdn_prepare(void)
{
	int err;
	const struct bt_fast_pair_fmdn_adv_param fmdn_adv_param =
		BT_FAST_PAIR_FMDN_ADV_PARAM_INIT(
			FMDN_ADV_INTERVAL, FMDN_ADV_INTERVAL);

	/* Application configuration of the advertising interval is equal to
	 * the default value that is defined in the FMDN module. This API
	 * call is only for demonstration purposes.
	 */
	err = bt_fast_pair_fmdn_adv_param_set(&fmdn_adv_param);
	if (err) {
		LOG_ERR("FMDN: bt_fast_pair_fmdn_adv_param_set failed (err %d)", err);
		return err;
	}

	err = bt_fast_pair_fmdn_info_cb_register(&fmdn_info_cb);
	if (err) {
		LOG_ERR("FMDN: bt_fast_pair_fmdn_info_cb_register failed (err %d)", err);
		return err;
	}

	err = bt_fast_pair_fmdn_read_mode_cb_register(&fmdn_read_mode_cb);
	if (err) {
		LOG_ERR("FMDN: bt_fast_pair_fmdn_read_mode_cb_register failed (err %d)", err);
		return err;
	}

	err = bt_fast_pair_info_cb_register(&fp_info_callbacks);
	if (err) {
		LOG_ERR("FMDN: bt_fast_pair_info_cb_register failed (err %d)", err);
		return err;
	}

	err = bt_fast_pair_fmdn_battery_level_set(FMDN_BATTERY_LEVEL);
	if (err) {
		LOG_ERR("FMDN: bt_fast_pair_fmdn_battery_level_set failed (err %d)", err);
	}

	return 0;
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

	err = app_ui_mode_set(APP_UI_MODE_SELECTED_GOOGLE);
	if (err) {
		LOG_ERR("Failed to set the Google UI mode (err %d)", err);
		return;
	}

	if (IS_ENABLED(CONFIG_APP_DFU)) {
		err = dfu_init();
		if (err) {
			LOG_ERR("dfu_init failed (err %d)", err);
			return;
		}
	}

	err = bt_conn_auth_cb_register(&conn_auth_callbacks);
	if (err) {
		LOG_ERR("Registering authentication callbacks failed (err %d)", err);
		return;
	}
// add by andrew
#if 0
	err = bt_gatt_authorization_cb_register(&gatt_authorization_callbacks);
	if (err) {
		LOG_ERR("Registering GATT authorization callbacks failed (err %d)", err);
		return;
	}
#endif
	err = app_ring_init();
	if (err) {
		LOG_ERR("FMDN: app_ring_init failed (err %d)", err);
		return;
	}

	err = fast_pair_prepare();
	if (err) {
		LOG_ERR("FMDN: fast_pair_prepare failed (err %d)", err);
		return;
	}

	err = fmdn_prepare();
	if (err) {
		LOG_ERR("FMDN: fmdn_prepare failed (err %d)", err);
		return;
	}

	err = app_factory_reset_init(factory_reset_requested);
	if (err) {
		LOG_ERR("FMDN: app_factory_reset_init failed (err %d)", err);
		return;
	}

	err = bt_fast_pair_enable();
	if (err) {
		LOG_ERR("FMDN: bt_fast_pair_enable failed (err %d)", err);
		return;
	}

	err = app_fp_adv_enable();
	if (err) {
		LOG_ERR("FMDN: app_fp_adv_enable failed (err %d)", err);
		return;
	}

	k_sem_give(&init_work_sem);
}

void app_network_google_run(void)
{
	int err;
	union app_ui_state state = {.selected = APP_UI_SELECTED_STATE_APP_RUNNING};

	LOG_INF("Starting the Google Find My Device network");

	/* Switch to the cooperative thread context before interaction
	 * with the Fast Pair and FMDN API.
	 */
	(void) k_work_submit(&init_work);
	err = k_sem_take(&init_work_sem, K_SECONDS(INIT_SEM_TIMEOUT));
	if (err) {
		k_panic();
	}

	app_ui_state_change_indicate(state, true);
}

APP_UI_REQUEST_LISTENER_REGISTER(ui_network_google,
				 APP_UI_MODE_SELECTED_GOOGLE,
				 ui_request_handle);

APP_NETWORK_SELECTOR_DESC_REGISTER(network_google,
				   APP_NETWORK_SELECTOR_GOOGLE,
				   app_network_google_run,
				   factory_reset_perform_google);
