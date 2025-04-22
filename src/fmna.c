/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "fmna_adv.h"
#include "fmna_battery.h"
#include "fmna_conn.h"
#include "fmna_gatt_ais.h"
#include "fmna_gatt_fmns.h"
#include "fmna_keys.h"
#include "fmna_nfc.h"
#include "fmna_serial_number.h"
#include "fmna_storage.h"
#include "fmna_state.h"
#include "fmna_version.h"
#include "uarp/fmna_uarp_service.h"

#include <fmna.h>

#include <zephyr/settings/settings.h>

#include <zephyr/sys/atomic.h>
#include <zephyr/sys/base64.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fmna, CONFIG_FMNA_LOG_LEVEL);

#define MFI_AUTH_TOKEN_LOG_SHORT_LEN	16

/* Flags used to safely perform enable and disable operations. */
enum {
	FMNA_ENABLE,
	FMNA_DISABLE,
	FMNA_READY,
	FMNA_NUM_FLAGS,
};

BUILD_ASSERT(CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE >= 4096,
	"The workqueue stack size is too small for the FMN");

static ATOMIC_DEFINE(flags, FMNA_NUM_FLAGS);
static struct k_work basic_display_work;

static uint8_t fmna_bt_id = BT_ID_DEFAULT;

static size_t token_length_calculate(const uint8_t token[FMNA_SW_AUTH_TOKEN_BLEN])
{
	size_t idx = FMNA_SW_AUTH_TOKEN_BLEN - 1;

	/* Length of token is calculated by trimming trailing zeroes. */
	while (idx >= 0 && token[idx] == 0) {
		idx--;
	}

	/* Index to length. */
	return idx + 1;
}

static void auth_token_base64_log(uint8_t auth_token[FMNA_SW_AUTH_TOKEN_BLEN], size_t len)
{
	int err = 0;
	char *encoded = NULL;
	size_t encoded_len;

	if (len == 0) {
		LOG_INF("SW Authentication Token (base64 format): (... %d trailing zero bytes ...)",
			FMNA_SW_AUTH_TOKEN_BLEN);
		return;
	}

	err = base64_encode(NULL, 0, &encoded_len, auth_token, len);
	__ASSERT((err == -ENOMEM) && (encoded_len != 0),
		"Failed to calculate Base64 encoded string length");

	encoded = k_malloc(encoded_len);
	err = (encoded == NULL) ? -ENOMEM : 0;

	if (!err) {
		err = base64_encode(encoded, encoded_len, &encoded_len, auth_token, len);
	}

	if (!err) {
		LOG_INF("SW Authentication Token (base64 format):");

		if (IS_ENABLED(CONFIG_FMNA_LOG_MFI_AUTH_TOKEN_BASE64_FULL)) {
			LOG_INF("%s", encoded);
		} else if (IS_ENABLED(CONFIG_FMNA_LOG_MFI_AUTH_TOKEN_BASE64_SHORT)) {
			size_t encoded_trimmed_len = 2 * MFI_AUTH_TOKEN_LOG_SHORT_LEN;
			if (strlen(encoded) > encoded_trimmed_len) {
				const uint8_t *encoded_suffix = &encoded[encoded_len - MFI_AUTH_TOKEN_LOG_SHORT_LEN];
				LOG_INF("%.*s (... %d more chars ...) %.*s",
					MFI_AUTH_TOKEN_LOG_SHORT_LEN, encoded, encoded_len - encoded_trimmed_len,
					MFI_AUTH_TOKEN_LOG_SHORT_LEN, encoded_suffix);
			} else {
				/* Token is short enough already and will not flood the logs. */
				LOG_INF("%s", encoded);
			}
		}
	} else {
		LOG_WRN("Could not log base64 encoded SW Authentication Token, "
			"returned error: %d", err);
	}

	k_free(encoded);
}

static void auth_token_hex_log(uint8_t auth_token[FMNA_SW_AUTH_TOKEN_BLEN], size_t len)
{
	if (IS_ENABLED(CONFIG_FMNA_LOG_MFI_AUTH_TOKEN_HEX_FULL)) {
		LOG_HEXDUMP_INF(auth_token, len, "SW Authentication Token (byte format):");
	} else if (IS_ENABLED(CONFIG_FMNA_LOG_MFI_AUTH_TOKEN_HEX_SHORT)) {
		LOG_HEXDUMP_INF(auth_token, MFI_AUTH_TOKEN_LOG_SHORT_LEN, "SW Authentication Token (byte format):");
		LOG_INF("(... %d more bytes ...)", len - MFI_AUTH_TOKEN_LOG_SHORT_LEN);
	}

	LOG_INF("(... %d trailing zero bytes ...)", FMNA_SW_AUTH_TOKEN_BLEN - len);
}

static void basic_display_work_handler(struct k_work *work)
{
	int err;
	uint8_t uuid[FMNA_SW_AUTH_UUID_BLEN] = {0};
	uint8_t auth_token[FMNA_SW_AUTH_TOKEN_BLEN] = {0};
	uint8_t serial_number[FMNA_SERIAL_NUMBER_BLEN] = {0};
	struct fmna_version ver;

	err = fmna_storage_uuid_load(uuid);
	if (err == -ENOENT) {
		LOG_WRN("MFi Token UUID not found: "
			"please provision a token to the device");
	} else if (err) {
		LOG_ERR("fmna_storage_uuid_load returned error: %d", err);
	} else {
		LOG_HEXDUMP_INF(uuid, sizeof(uuid), "SW UUID:");
	}

	err = fmna_storage_auth_token_load(auth_token);
	if (err == -ENOENT) {
		LOG_WRN("MFi Authentication Token not found: "
			"please provision a token to the device");
	} else if (err) {
		LOG_ERR("fmna_storage_auth_token_load returned error: %d",
			err);
	} else {
		size_t auth_token_len = token_length_calculate(auth_token);

		if (IS_ENABLED(CONFIG_FMNA_LOG_MFI_AUTH_TOKEN_BASE64_SHORT) ||
		    IS_ENABLED(CONFIG_FMNA_LOG_MFI_AUTH_TOKEN_BASE64_FULL)) {
			auth_token_base64_log(auth_token, auth_token_len);
		} else if (IS_ENABLED(CONFIG_FMNA_LOG_MFI_AUTH_TOKEN_HEX_SHORT) ||
			   IS_ENABLED(CONFIG_FMNA_LOG_MFI_AUTH_TOKEN_HEX_FULL)) {
			auth_token_hex_log(auth_token, auth_token_len);
		}
	}

	err = fmna_serial_number_get(serial_number);
	if (err == -ENOENT) {
		LOG_WRN("Serial number not found: "
			"please provision a serial number to the device");
	} else if (err) {
		LOG_ERR("fmna_serial_number_get returned error: %d",
			err);
	} else {
		LOG_HEXDUMP_INF(serial_number, sizeof(serial_number),
				"Serial Number:");
	}

	err = fmna_version_fw_get(&ver);
	if (err) {
		LOG_ERR("fmna_version_fw_get returned error: %d", err);
		memset(&ver, 0, sizeof(ver));
	}

	LOG_INF("Application firmware version: v%d.%d.%d",
		ver.major, ver.minor, ver.revision);

	if (IS_ENABLED(CONFIG_FMNA_QUALIFICATION)) {
		LOG_WRN("The FMN stack is configured for qualification");
		LOG_WRN("The qualification configuration should not be used for production");
	}
}

static int fmna_gatt_services_hidden_mode_set(bool hidden_mode)
{
	int err;

	err = fmna_gatt_ais_hidden_mode_set(hidden_mode);
	if (err) {
		LOG_ERR("fmna_gatt_ais_hidden_mode_set returned error: %d", err);
		return err;
	}

	err = fmna_gatt_service_hidden_mode_set(hidden_mode);
	if (err) {
		LOG_ERR("fmna_gatt_service_hidden_mode_set returned error: %d", err);
		return err;
	}

	if (IS_ENABLED(CONFIG_FMNA_UARP)) {
		err = fmna_uarp_service_hidden_mode_set(hidden_mode);
		if (err) {
			LOG_ERR("fmna_uarp_service_hidden_mode_set returned error: %d", err);
			return err;
		}
	}

	return 0;
}

static int fmna_callback_group_register(const struct fmna_info_cb *cb)
{
	int err;

	err = fmna_state_pairing_failed_cb_register(cb->pairing_failed);
	if (err) {
		LOG_ERR("fmna_state_pairing_failed_cb_register returned error: %d", err);
		return err;
	}

	err = fmna_state_pairing_mode_timeout_cb_register(cb->pairing_mode_exited);
	if (err) {
		LOG_ERR("fmna_state_pairing_mode_timeout_cb_register returned error: %d", err);
		return err;
	}

	err = fmna_state_location_availability_cb_register(cb->location_availability_changed);
	if (err) {
		LOG_ERR("fmna_state_location_availability_cb_register returned error: %d", err);
		return err;
	}

	err = fmna_state_paired_state_changed_cb_register(cb->paired_state_changed);
	if (err) {
		LOG_ERR("fmna_state_paired_state_changed_cb_register returned error: %d", err);
		return err;
	}

	err = fmna_battery_level_request_cb_register(cb->battery_level_request);
	if (err) {
		LOG_ERR("fmna_battery_level_request_cb_register returned error: %d", err);
		return err;
	}

	return err;
}

int fmna_enable(void)
{
	int err;
	bool is_paired = false;

	/* Verify the state of FMN dependencies. */
	if (fmna_is_ready()) {
		LOG_ERR("FMN: FMN stack already enabled");
		return -EALREADY;
	}

	if (!bt_is_ready()) {
		LOG_ERR("FMN: BLE stack should be enabled");
		return -ENOPROTOOPT;
	}

	if (fmna_bt_id == BT_ID_DEFAULT) {
		LOG_ERR("FMN: Invalid Bluetooth identity");
		return -EINVAL;
	}

	if (atomic_test_and_set_bit(flags, FMNA_ENABLE)) {
		LOG_ERR("FMN: FMN stack is already being enabled");
		return -EALREADY;
	}

	/* Initialize FMN modules. */
	err = fmna_battery_init();
	if (err) {
		LOG_ERR("fmna_battery_init returned error: %d", err);
		goto error;
	}

	err = fmna_conn_init(fmna_bt_id);
	if (err) {
		LOG_ERR("fmna_conn_init returned error: %d", err);
		goto error;
	}

	err = fmna_storage_init(false, &is_paired);
	if (err) {
		LOG_ERR("fmna_storage_init returned error: %d", err);
		goto error;
	}

	err = fmna_keys_init(fmna_bt_id, is_paired);
	if (err) {
		LOG_ERR("fmna_keys_init returned error: %d", err);
		goto error;
	}

	if (IS_ENABLED(CONFIG_FMNA_SERVICE_HIDDEN_MODE)) {
		err = fmna_gatt_services_hidden_mode_set(false);
		if (err) {
			LOG_ERR("fmna_gatt_services_hidden_mode_set returned error: %d", err);
			goto error;
		}
	}

	err = fmna_state_init(fmna_bt_id, is_paired);
	if (err) {
		LOG_ERR("fmna_state_init returned error: %d", err);
		goto error;
	}

	if (IS_ENABLED(CONFIG_FMNA_NFC)) {
		err = fmna_nfc_init(fmna_bt_id);
		if (err) {
			LOG_ERR("fmna_nfc_init returned error: %d", err);
			goto error;
		}
	}

	/* Set the ready status for the FMN stack and allow calling the fmna_disable API. */
	atomic_set_bit(flags, FMNA_READY);
	atomic_clear_bit(flags, FMNA_DISABLE);

	/* MFi tokens use a lot of stack, offload basic display logic
	 * to the workqueue.
	 */
	k_work_init(&basic_display_work, basic_display_work_handler);
	k_work_submit(&basic_display_work);

error:
	if (err) {
		/* Allow the API user to call the fmna_enable again. */
		atomic_clear_bit(flags, FMNA_ENABLE);
	}

	return err;
}

int fmna_id_set(uint8_t bt_id)
{
	if (fmna_is_ready()) {
		LOG_ERR("FMN: Bluetooth identity can only be set when FMN stack is disabled");
		return -EACCES;
	}

	if (bt_id == BT_ID_DEFAULT) {
		LOG_ERR("FMN: Invalid Bluetooth identity, BT_ID_DEFAULT is not allowed");
		return -EINVAL;
	}

	fmna_bt_id = bt_id;

	return 0;
}

int fmna_info_cb_register(const struct fmna_info_cb *cb)
{
	if (fmna_is_ready()) {
		LOG_ERR("FMN: Info callbacks can only be registered when FMN stack is disabled");
		return -EACCES;
	}

	return fmna_callback_group_register(cb);
}

int fmna_factory_reset(void)
{
	int ret;

	if (fmna_is_ready()) {
		LOG_ERR("FMN: Factory reset can only be performed when the FMN stack is disabled");
		return -EACCES;
	}

	if (fmna_bt_id == BT_ID_DEFAULT) {
		LOG_ERR("FMN: Invalid Bluetooth identity, BT_ID_DEFAULT is not allowed");
		return -EINVAL;
	}

	/* Ensure that the settings subsys is initialized if fmna_enable
	 * was not previously called.
	 */
	ret = settings_subsys_init();
	if (ret) {
		LOG_ERR("settings_subsys_init returned error: %d", ret);
		return ret;
	}

	LOG_INF("FMN: Performing reset to default factory settings");

	ret = fmna_storage_pairing_data_delete();
	if (ret) {
		LOG_ERR("fmna_storage_pairing_data_delete returned error: %d", ret);
		return ret;
	}

	ret = bt_id_reset(fmna_bt_id, NULL, NULL);
	if (ret != fmna_bt_id) {
		LOG_ERR("bt_id_reset returned error: %d", ret);
		return ret;
	}

	return 0;
}

int fmna_disable(void)
{
	int err;

	if (!fmna_is_ready()) {
		LOG_ERR("FMN: FMN stack already disabled");
		return -EALREADY;
	}

	if (atomic_test_and_set_bit(flags, FMNA_DISABLE)) {
		LOG_ERR("FMN: FMN stack is already being disabled");
		return -EALREADY;
	}

	/* Clear the ready status before disabling the FMN stack. */
	atomic_clear_bit(flags, FMNA_READY);

	/* Uninitialize FMN modules. */
	err = fmna_state_uninit();
	if (err) {
		LOG_ERR("fmna_state_uninit returned error: %d", err);
		goto error;
	}

	err = fmna_conn_uninit();
	if (err) {
		LOG_ERR("fmna_conn_uninit returned error: %d", err);
		goto error;
	}

	if (IS_ENABLED(CONFIG_FMNA_SERVICE_HIDDEN_MODE)) {
		err = fmna_gatt_services_hidden_mode_set(true);
		if (err) {
			LOG_ERR("fmna_gatt_services_hidden_mode_set returned error: %d", err);
			goto error;
		}
	}

	if (IS_ENABLED(CONFIG_FMNA_NFC)) {
		err = fmna_nfc_uninit();
		if (err) {
			LOG_ERR("fmna_nfc_uninit returned error: %d", err);
			goto error;
		}
	}

	/* Allow the API user to enable the FMN stack with the fmna_enable. */
	atomic_clear_bit(flags, FMNA_ENABLE);

error:
	if (err) {
		/* Allow the API user to call the fmna_disable again. */
		atomic_clear_bit(flags, FMNA_DISABLE);
		atomic_set_bit(flags, FMNA_READY);
	}

	return err;
}

bool fmna_is_ready(void)
{
	return atomic_test_bit(flags, FMNA_READY);
}
