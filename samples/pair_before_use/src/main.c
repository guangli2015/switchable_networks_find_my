/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/services/hrs.h>

#include <fmna.h>

#include <zephyr/settings/settings.h>

#include <dk_buttons_and_leds.h>

#define HR_SENSOR_BT_ID BT_ID_DEFAULT
#define FMNA_BT_ID      1
#define BT_ID_COUNT     2

BUILD_ASSERT(BT_ID_COUNT == CONFIG_BT_ID_MAX, "BT identities misconfigured");

#define FMNA_PEER_SOUND_DURATION K_SECONDS(5)

#define FMNA_SOUND_LED        DK_LED1
#define FMNA_PAIRED_STATE_LED DK_LED3
#define FMNA_ACTIVATION_LED   DK_LED4

#define FMNA_ADV_RESUME_BUTTON             DK_BTN1_MSK
#define FMNA_ACTIVATION_BUTTON             DK_BTN1_MSK
#define FMNA_SN_LOOKUP_BUTTON              DK_BTN2_MSK
#define FMNA_FACTORY_SETTINGS_RESET_BUTTON DK_BTN4_MSK

#define FMNA_ACTIVATION_MIN_HOLD_TIME_MS 3000

#define FMNA_DEVICE_NAME_SUFFIX " - Find My"

#define HR_SENSOR_PAIRING_BUTTON      DK_BTN3_MSK

#define HR_SENSOR_DEVICE_NAME "HR Sensor"
#define HR_SENSOR_FMNA_DEVICE_NAME \
	HR_SENSOR_DEVICE_NAME FMNA_DEVICE_NAME_SUFFIX

#define BATTERY_LEVEL_CHANGE_BUTTON   DK_BTN4_MSK

#define BATTERY_LEVEL_MAX         100
#define BATTERY_LEVEL_MIN         0
#define BATTERY_LEVEL_CHANGE_RATE 7

static bool fmna_location_available;
static bool fmna_pairing_mode;
static bool fmna_paired;

static const struct bt_data hr_sensor_ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_HRS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_DIS_VAL))
};
static struct bt_data hr_sensor_sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, HR_SENSOR_DEVICE_NAME, sizeof(HR_SENSOR_DEVICE_NAME) - 1)
};
static struct bt_le_ext_adv *hr_sensor_adv_set;
static bool hr_sensor_pairing_mode;
static struct bt_conn *hr_sensor_conn;

static uint8_t battery_level = 100;

static void fmna_enable_work_handle(struct k_work *item);
static void fmna_disable_work_handle(struct k_work *item);
static void fmna_sound_timeout_work_handle(struct k_work *item);
static void hr_sensor_advertising_work_handle(struct k_work *item);

static K_WORK_DELAYABLE_DEFINE(fmna_enable_work, fmna_enable_work_handle);
static K_WORK_DELAYABLE_DEFINE(fmna_disable_work, fmna_disable_work_handle);
static K_WORK_DELAYABLE_DEFINE(fmna_sound_timeout_work, fmna_sound_timeout_work_handle);
static K_WORK_DEFINE(hr_sensor_advertising_work, hr_sensor_advertising_work_handle);

static void hr_sensor_device_name_set(bool force)
{
	struct bt_data *sd_dev_name = &hr_sensor_sd[0];
	static bool suffix_present = false;
	bool use_suffix;

	/* Suffix should be present when the HR sensor is in the pairing
	 * mode and when the Find My Network is enabled.
	 */
	use_suffix = (hr_sensor_pairing_mode && fmna_location_available);

	if ((force) || (use_suffix != suffix_present)) {
		int err;
		const char* device_name = use_suffix ?
			HR_SENSOR_FMNA_DEVICE_NAME : HR_SENSOR_DEVICE_NAME;

		err = bt_set_name(device_name);
		if (err) {
			printk("bt_set_name failed (err %d)\n", err);
			return;
		}

		printk("HR Sensor device name set to: %s\n", device_name);

		/* Align device name in the scan response data. */
		__ASSERT_NO_MSG(sd_dev_name->type == BT_DATA_NAME_COMPLETE);
		sd_dev_name->data = device_name;
		sd_dev_name->data_len = strlen(device_name);

		suffix_present = use_suffix;

		if (hr_sensor_adv_set) {
			err = bt_le_ext_adv_set_data(hr_sensor_adv_set,
						     hr_sensor_ad, ARRAY_SIZE(hr_sensor_ad),
						     hr_sensor_sd, ARRAY_SIZE(hr_sensor_sd));
			if (err) {
				printk("bt_le_ext_adv_set_data failed (err %d)\n", err);
				return;
			}
		}
	}
}

static void fmna_sound_stop_indicate(void)
{
	printk("Stopping the sound from being played\n");

	dk_set_led(FMNA_SOUND_LED, 0);
}

static void fmna_sound_timeout_work_handle(struct k_work *item)
{
	int err;

	err = fmna_sound_completed_indicate();
	if (err) {
		printk("fmna_sound_completed_indicate failed (err %d)\n", err);
		return;
	}

	printk("Sound playing timed out\n");

	fmna_sound_stop_indicate();
}

static void fmna_sound_start(enum fmna_sound_trigger sound_trigger)
{
	k_work_reschedule(&fmna_sound_timeout_work, FMNA_PEER_SOUND_DURATION);

	dk_set_led(FMNA_SOUND_LED, 1);

	printk("Starting to play sound...\n");
}

static void fmna_sound_stop(void)
{
	printk("Received a request from FMN to stop playing sound\n");

	k_work_cancel_delayable(&fmna_sound_timeout_work);

	fmna_sound_stop_indicate();
}

static const struct fmna_sound_cb fmna_sound_callbacks = {
	.sound_start = fmna_sound_start,
	.sound_stop = fmna_sound_stop,
};

static void fmna_location_availability_changed(bool available)
{
	printk("Find My location %s\n", available ? "enabled" : "disabled");

	fmna_location_available = available;

	hr_sensor_device_name_set(false);
}

static void fmna_pairing_mode_exited(void)
{
	printk("Exited the FMN pairing mode\n");

	fmna_pairing_mode = false;
}

static void fmna_paired_state_changed(bool new_paired_state)
{
	printk("The FMN accessory transitioned to the %spaired state\n",
	       new_paired_state ? "" : "un");

	fmna_paired = new_paired_state;
	fmna_pairing_mode = !new_paired_state;

	/* FMNA specification guidelines for pair before use accessories:
	 * The FMN pairing mode cannot be active when the Bluetooth peer is
	 * connected to the device for its primary purpose (HR sensor in
	 * the context of this sample).
	 *
	 * The fmna_pairing_mode_cancel API call is required here because the
	 * CONFIG_FMNA_PAIRING_MODE_AUTO_ENTER Kconfig option is enabled.
	 */
	if (fmna_pairing_mode && hr_sensor_conn) {
		int err;

		err = fmna_pairing_mode_cancel();
		if (err) {
			printk("Cannot cancel the FMN pairing mode (err: %d)\n", err);
		} else {
			printk("FMN pairing mode cancelled\n");

			fmna_pairing_mode = false;
		}
	}

	dk_set_led(FMNA_PAIRED_STATE_LED, new_paired_state);
}

static const struct fmna_info_cb fmna_info_callbacks = {
	.location_availability_changed = fmna_location_availability_changed,
	.pairing_mode_exited = fmna_pairing_mode_exited,
	.paired_state_changed = fmna_paired_state_changed,
};

static int fmna_id_create(uint8_t id)
{
	int ret;
	bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
	size_t count = ARRAY_SIZE(addrs);

	bt_id_get(addrs, &count);
	if (id < count) {
		return 0;
	}

	do {
		ret = bt_id_create(NULL, NULL);
		if (ret < 0) {
			return ret;
		}
	} while (ret != id);

	return 0;
}

static bool factory_settings_restore_check(void)
{
	uint32_t button_state;

	dk_read_buttons(&button_state, NULL);

	return (button_state & FMNA_FACTORY_SETTINGS_RESET_BUTTON);
}

static int fmna_initialize(void)
{
	int err;

	err = fmna_sound_cb_register(&fmna_sound_callbacks);
	if (err) {
		printk("fmna_sound_cb_register failed (err %d)\n", err);
		return err;
	}

	err = fmna_id_create(FMNA_BT_ID);
	if (err) {
		printk("fmna_id_create failed (err %d)\n", err);
		return err;
	}

	err = fmna_id_set(FMNA_BT_ID);
	if (err) {
		printk("fmna_id_set failed (err %d)\n", err);
		return err;
	}

	if (factory_settings_restore_check()) {
		err = fmna_factory_reset();
		if (err) {
			printk("fmna_factory_reset failed (err %d)\n", err);
			return err;
		}
	}

	err = fmna_battery_level_set(battery_level);
	if (err) {
		printk("fmna_battery_level_set failed (err %d)\n", err);
		return err;
	}

	err = fmna_info_cb_register(&fmna_info_callbacks);
	if (err) {
		printk("fmna_info_cb_register failed (err %d)\n", err);
		return err;
	}

	err = fmna_enable();
	if (err) {
		printk("fmna_enable failed (err %d)\n", err);
		return err;
	}

	dk_set_led(FMNA_ACTIVATION_LED, 1);

	return 0;
}

static void fmna_adv_resume_action_handle(void)
{
	int err;

	if (!fmna_paired && !fmna_pairing_mode) {
		/* FMNA specification guidelines for pair before use accessories:
		 * The FMN pairing mode cannot be active when the Bluetooth peer is
		 * connected to the device for its primary purpose (HR sensor in
		 * the context of this sample).
		 */
		if (hr_sensor_conn) {
			printk("FMN pairing mode cannot be resumed due to "
			       "the active HR sensor connection\n");
			return;
		}

		err = fmna_pairing_mode_enter();
		if (err) {
			printk("Cannot resume the FMN pairing mode (err: %d)\n", err);
		} else {
			printk("FMN pairing mode resumed\n");

			fmna_pairing_mode = true;
		}
	}
}

static void fmna_enable_work_handle(struct k_work *item)
{
	int err;

	if (factory_settings_restore_check()) {
		err = fmna_factory_reset();
		if (err) {
			printk("fmna_factory_reset failed (err %d)\n", err);
			return;
		}
	}

	err = fmna_enable();
	if (err) {
		printk("fmna_enable failed (err %d)\n", err);

		k_work_reschedule(&fmna_enable_work, K_SECONDS(1));
	} else {
		printk("FMN enabled\n");

		dk_set_led(FMNA_ACTIVATION_LED, 1);
	}
}

static void fmna_disable_work_handle(struct k_work *item)
{
	int err;

	err = fmna_disable();
	if (err) {
		printk("fmna_disable failed (err: %d)\n", err);

		k_work_reschedule(&fmna_disable_work, K_SECONDS(1));
	} else {
		printk("FMN disabled\n");

		/* Reset the necessary flags. */
		fmna_location_available = false;

		dk_set_led(FMNA_ACTIVATION_LED, 0);
	}
}

static void fmna_activation_action_handle(void)
{
	/* In case we are in error retry mode. */
	k_work_cancel_delayable(&fmna_enable_work);
	k_work_cancel_delayable(&fmna_disable_work);

	if (fmna_is_ready()) {
		fmna_disable_work_handle(NULL);
	} else {
		fmna_enable_work_handle(NULL);
	}
}

static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	int err;
	uint32_t buttons = button_state & has_changed;

	if (has_changed & (FMNA_ADV_RESUME_BUTTON | FMNA_ACTIVATION_BUTTON)) {
		static int64_t prev_uptime;

		if (button_state & (FMNA_ADV_RESUME_BUTTON | FMNA_ACTIVATION_BUTTON)) {
			/* On button press */
			prev_uptime = k_uptime_get();
		} else {
			/* On button release */
			int64_t hold_time;

			hold_time = (k_uptime_get() - prev_uptime);
			if (hold_time > FMNA_ACTIVATION_MIN_HOLD_TIME_MS) {
				fmna_activation_action_handle();
			} else {
				fmna_adv_resume_action_handle();
			}
		}
	}

	if (buttons & FMNA_SN_LOOKUP_BUTTON) {
		err = fmna_serial_number_lookup_enable();
		if (err) {
			printk("Cannot enable FMN Serial Number lookup (err: %d)\n", err);
		} else {
			printk("FMN Serial Number lookup enabled\n");
		}
	}

	if (buttons & HR_SENSOR_PAIRING_BUTTON) {
		hr_sensor_pairing_mode = !hr_sensor_pairing_mode;

		hr_sensor_device_name_set(false);

		printk(hr_sensor_pairing_mode ?
		       "HR sensor enters the pairing mode\n" :
		       "HR sensor exits from the pairing mode\n");
	}

	if (buttons & BATTERY_LEVEL_CHANGE_BUTTON) {
		battery_level = (battery_level > BATTERY_LEVEL_CHANGE_RATE) ?
			(battery_level - BATTERY_LEVEL_CHANGE_RATE) : BATTERY_LEVEL_MAX;

		err = fmna_battery_level_set(battery_level);
		if (err) {
			printk("fmna_battery_level_set failed (err %d)\n", err);
		}

		err = bt_bas_set_battery_level(battery_level);
		if (err) {
			printk("bt_bas_set_battery_level failed (err %d)\n", err);
		}

		printk("Setting battery level to: %d %%\n", battery_level);
	}
}

static int dk_library_initialize(void)
{
	int err;

	err = dk_leds_init();
	if (err) {
		printk("LEDs init failed (err %d)\n", err);
		return err;
	}

	err = dk_buttons_init(button_changed);
	if (err) {
		printk("Buttons init failed (err: %d)\n", err);
		return err;
	}

	return 0;
}

static int hr_sensor_advertising_start(void)
{
	int err;
	struct bt_le_ext_adv_start_param ext_adv_start_param = {0};
	struct bt_le_adv_param param = {0};

	if (hr_sensor_adv_set) {
		err = bt_le_ext_adv_delete(hr_sensor_adv_set);
		if (err) {
			printk("bt_le_ext_adv_delete returned error: %d\n", err);
			return err;
		}
	}

	param.id           = HR_SENSOR_BT_ID;
	param.sid          = HR_SENSOR_BT_ID;
	param.options      = BT_LE_ADV_OPT_CONN;
	param.interval_min = BT_GAP_ADV_FAST_INT_MIN_2;
	param.interval_max = BT_GAP_ADV_FAST_INT_MAX_2;
	err = bt_le_ext_adv_create(&param, NULL, &hr_sensor_adv_set);
	if (err) {
		printk("Could not create HR sensor advertising set (err %d)\n", err);
		return err;
	}

	err = bt_le_ext_adv_set_data(hr_sensor_adv_set, hr_sensor_ad, ARRAY_SIZE(hr_sensor_ad),
				     hr_sensor_sd, ARRAY_SIZE(hr_sensor_sd));
	if (err) {
		printk("Could not set data for HR sensor advertising set (err %d)\n", err);
		return err;
	}

	err = bt_le_ext_adv_start(hr_sensor_adv_set, &ext_adv_start_param);
	if (err) {
		printk("Advertising for HR sensor set failed to start (err %d)\n", err);
		return err;
	}

	printk("HR sensor advertising successfully started\n");

	return err;
}

static void hr_sensor_advertising_work_handle(struct k_work *item)
{
	hr_sensor_advertising_start();
}

static bool hr_sensor_conn_check(struct bt_conn *conn)
{
	struct bt_conn_info conn_info;

	bt_conn_get_info(conn, &conn_info);

	return (conn_info.id == HR_SENSOR_BT_ID);
}

static void hr_sensor_auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	/* Filter out peers not connected to the HR sensor application. */
	if (!hr_sensor_conn_check(conn)) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled HR Sensor (%s)\n", addr);
}

static enum bt_security_err hr_sensor_pairing_accept(
	struct bt_conn *conn,
	const struct bt_conn_pairing_feat * const feat)
{
	/* Filter out peers not connected to the HR sensor application. */
	if (!hr_sensor_conn_check(conn)) {
		return BT_SECURITY_ERR_SUCCESS;
	}

	if (hr_sensor_pairing_mode) {
		printk("HR Sensor: confirming pairing attempt\n");

		return BT_SECURITY_ERR_SUCCESS;
	} else {
		printk("HR Sensor: rejecting pairing attempt\n");
		printk("HR Sensor: enter the pairing mode before next attempt\n");

		return BT_SECURITY_ERR_PAIR_NOT_ALLOWED;
	}
}

static void hr_sensor_pairing_complete(struct bt_conn *conn, bool bonded)
{
	/* Filter out peers not connected to the HR sensor application. */
	if (!hr_sensor_conn_check(conn)) {
		return;
	}

	if (hr_sensor_pairing_mode) {
		hr_sensor_pairing_mode = false;

		printk("HR sensor exits from the pairing mode\n");
	}

	hr_sensor_device_name_set(false);
}

static void hr_sensor_auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	/* Filter out peers not connected to the HR sensor application. */
	if (!hr_sensor_conn_check(conn)) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Passkey for HR Sensor (%s): %06u\n", addr, passkey);
}

static struct bt_conn_auth_cb hr_sensor_auth_cb_display = {
	.cancel = hr_sensor_auth_cancel,
	.pairing_accept = hr_sensor_pairing_accept,
	.passkey_display = hr_sensor_auth_passkey_display,
};

static struct bt_conn_auth_info_cb hr_sensor_auth_info_cb_display = {
	.pairing_complete = hr_sensor_pairing_complete,
};

static void hr_sensor_connected(struct bt_conn *conn, uint8_t conn_err)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN];

	/* Filter out peers not connected to the HR sensor application. */
	if (!hr_sensor_conn_check(conn)) {
		return;
	}

	if (conn_err) {
		printk("HR connection establishment error: %d\n", conn_err);
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("HR Peer connected: %s\n", addr);

	hr_sensor_conn = conn;

	/* FMNA specification guidelines for pair before use accessories:
	 * Cancel the FMN pairing mode once the Bluetooth peer connects to the
	 * device for its primary purpose (HR sensor in the context of this
	 * sample).
	 */
	if (fmna_pairing_mode) {
		err = fmna_pairing_mode_cancel();
		if (err) {
			printk("Cannot cancel the FMN pairing mode (err: %d)\n", err);
		} else {
			printk("FMN pairing mode cancelled\n");

			fmna_pairing_mode = false;
		}
	}

	/* FMNA specification guidelines for pair before use accessories:
	 * Disable the FMN paired advertising once the Bluetooth peer connects
	 * to the device for its primary purpose (HR sensor in the context of
	 * this sample).
	 */
	err = fmna_paired_adv_disable();
	if (err) {
		printk("fmna_paired_adv_disable failed (err %d)\n", err);
		return;
	}
}

static void hr_sensor_disconnected(struct bt_conn *conn, uint8_t reason)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN];

	/* Filter out peers not connected to the HR sensor application. */
	if (!hr_sensor_conn_check(conn)) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("HR Peer disconnected (reason %u): %s\n", reason, addr);

	hr_sensor_conn = NULL;

	/* FMNA specification guidelines for pair before use accessories:
	 * Enable the FMN paired advertising once the Bluetooth connection
	 * for the accessory primary purpose (HR sensor in the context of
	 * this sample) is terminated.
	 */
	err = fmna_paired_adv_enable();
	if (err) {
		printk("fmna_paired_adv_enable failed (err %d)\n", err);
		return;
	}

	/* Process the disconnect logic in the workqueue so that
	 * the BLE stack is finished with the connection bookkeeping
	 * logic and advertising is possible.
	 */
	k_work_submit(&hr_sensor_advertising_work);
}

static void hr_sensor_security_changed(struct bt_conn *conn,
				       bt_security_t level,
				       enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	/* Filter out peers not connected to the HR sensor application. */
	if (!hr_sensor_conn_check(conn)) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	if (err) {
		printk("HR Peer security failed: %s level %u err %d\n",
			addr, level, err);
	} else {
		printk("HR Peer security changed: %s level %u\n", addr, level);
	}
}

static struct bt_conn_cb hr_sensor_conn_callbacks = {
	.connected = hr_sensor_connected,
	.disconnected = hr_sensor_disconnected,
	.security_changed = hr_sensor_security_changed,
};

static int ble_stack_initialize(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return err;
	}

	err = settings_load();
	if (err) {
		printk("Settings loading failed (err %d)\n", err);
		return err;
	}

	printk("Bluetooth initialized\n");

	return 0;
}


static void hr_sensor_bond_remove(const struct bt_bond_info *info, void *user_data)
{
	int err;
	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(&info->addr, addr_str, sizeof(addr_str));

	printk("HR Sensor: removing bond information for: %s\n", addr_str);

	err = bt_unpair(HR_SENSOR_BT_ID, &info->addr);
	if (err) {
		printk("HR Sensor: unable to remove bond information: %d\n", err);
	}
}

static void hr_sensor_initialize(void)
{
	bt_conn_cb_register(&hr_sensor_conn_callbacks);
	bt_conn_auth_cb_register(&hr_sensor_auth_cb_display);
	bt_conn_auth_info_cb_register(&hr_sensor_auth_info_cb_display);

	if (factory_settings_restore_check()) {
		printk("HR Sensor: performing reset to default factory settings\n");

		bt_foreach_bond(HR_SENSOR_BT_ID, hr_sensor_bond_remove, NULL);
	}

	hr_sensor_device_name_set(true);
}

static void identities_print(void)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	bt_addr_le_t addrs[BT_ID_COUNT];
	size_t count = ARRAY_SIZE(addrs);

	bt_id_get(addrs, &count);

	if (count != BT_ID_COUNT) {
		printk("Wrong number of identities\n");
		k_oops();
	}

	bt_addr_le_to_str(&addrs[HR_SENSOR_BT_ID], addr_str, sizeof(addr_str));
	printk("HR sensor identity %d: %s\n", HR_SENSOR_BT_ID, addr_str);

	bt_addr_le_to_str(&addrs[FMNA_BT_ID], addr_str, sizeof(addr_str));
	printk("Find My identity %d: %s\n", FMNA_BT_ID, addr_str);
}

static void hr_sensor_measurement_simulate(void)
{
	static uint8_t heartrate = 90U;

	/* Heartrate measurements simulation */
	while (1) {
		k_sleep(K_SECONDS(1));

		heartrate++;
		if (heartrate == 160U) {
			heartrate = 90U;
		}

		bt_hrs_notify(heartrate);
	}
}

void main(void)
{
	int err;

	printk("Starting the FMN Pair before use application\n");

	err = dk_library_initialize();
	if (err) {
		printk("DK library init failed (err %d)\n", err);
		return;
	}

	err = ble_stack_initialize();
	if (err) {
		printk("BLE stack init failed (err %d)\n", err);
		return;
	}

	hr_sensor_initialize();

	err = fmna_initialize();
	if (err) {
		printk("FMNA init failed (err %d)\n", err);
		return;
	}

	printk("FMNA initialized\n");

	identities_print();

	err = hr_sensor_advertising_start();
	if (err) {
		printk("HR sensor advertising failed (err %d)\n", err);
		return;
	}

	hr_sensor_measurement_simulate();
}
