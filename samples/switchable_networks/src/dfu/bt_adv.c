/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <app_version.h>

#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>

#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>

#include "app_dfu_bt_adv.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(dfu, LOG_LEVEL_INF);

static struct bt_le_ext_adv *dfu_adv_set;

static const struct bt_data dfu_ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, SMP_BT_SVC_UUID_VAL),
};
static struct bt_data dfu_sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1)
};

static struct bt_conn *dfu_conn;
static K_SEM_DEFINE(restart_adv_sem, 0, 1);

static void restart_adv_work_handle(struct k_work *work);
static K_WORK_DEFINE(restart_adv_work, restart_adv_work_handle);

static struct bt_le_adv_param dfu_adv_param = {
	.id = BT_ID_DEFAULT,
	.options = (BT_LE_ADV_OPT_CONN |
		    BT_LE_ADV_OPT_USE_IDENTITY),
	.interval_min = 0x00A0, /* 100ms */
	.interval_max = 0x00A0, /* 100ms */
};

static void adv_connected(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_connected_info *info)
{
	__ASSERT_NO_MSG(!dfu_conn);

	LOG_INF("DFU: Connected");

	dfu_conn = info->conn;
}

static int dfu_adv_set_setup(void)
{
	int err;

	static const struct bt_le_ext_adv_cb dfu_adv_cb = {
		.connected = adv_connected,
	};

	__ASSERT(!dfu_adv_set, "DFU: Invalid state of the advertising set");

	err = bt_le_ext_adv_create(&dfu_adv_param, &dfu_adv_cb, &dfu_adv_set);
	if (err) {
		LOG_ERR("DFU: bt_le_ext_adv_create returned error: %d", err);
		return err;
	}

	err = bt_le_ext_adv_set_data(dfu_adv_set, dfu_ad, ARRAY_SIZE(dfu_ad),
				     dfu_sd, ARRAY_SIZE(dfu_sd));
	if (err) {
		LOG_ERR("DFU: Could not set data for advertising set (err %d)", err);
		return err;
	}

	LOG_INF("DFU: Prepared the advertising set");

	return 0;
}

static int dfu_adv_enable(void)
{
	int err;
	struct bt_le_ext_adv_start_param ext_adv_start_param = {0};

	err = bt_le_ext_adv_start(dfu_adv_set, &ext_adv_start_param);
	if (err) {
		LOG_ERR("DFU: Advertising set failed to start (err %d)", err);
		return err;
	}

	LOG_INF("DFU: Advertising successfully started");

	return 0;
}

static int dfu_adv_disable(void)
{
	int err;

	err = bt_le_ext_adv_stop(dfu_adv_set);
	if (err) {
		LOG_ERR("DFU: Cannot stop advertising (err: %d)", err);
		return err;
	}

	return 0;
}

void app_dfu_bt_adv_manage(bool enabled)
{
	int err;

	if (enabled) {
		LOG_INF("DFU: Enabling advertising");

		err = dfu_adv_enable();
		if (err) {
			LOG_ERR("DFU: dfu_adv_enable failed (err %d)", err);
			return;
		}
	} else {
		LOG_INF("DFU: Disabling advertising");

		err = dfu_adv_disable();
		if (err) {
			LOG_ERR("DFU: dfu_adv_disable failed (err %d)", err);
			return;
		}
	}
}

static void restart_adv_work_handle(struct k_work *work)
{
	int err;

	if (!dfu_adv_set) {
		return;
	}

	if (dfu_conn) {
		return;
	}

	LOG_INF("DFU: Restarting advertising");

	err = dfu_adv_enable();
	if (err) {
		LOG_ERR("DFU: dfu_adv_enable failed (err %d)", err);
		return;
	}
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		(void) k_work_submit(&restart_adv_work);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	if (dfu_conn == conn) {
		LOG_INF("DFU: Disconnected (reason %" PRIu8 ")", reason);

		dfu_conn = NULL;
		k_sem_give(&restart_adv_sem);
	}
}

static void recycled(void)
{
	if (k_sem_take(&restart_adv_sem, K_NO_WAIT) == 0) {
		(void) k_work_submit(&restart_adv_work);
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.recycled = recycled,
};

int app_dfu_bt_adv_id_set(uint8_t bt_id)
{
	if (dfu_adv_set) {
		LOG_ERR("DFU: Cannot change the Bluetooth identity after initialization");
		return -EACCES;
	}

	dfu_adv_param.id = bt_id;

	return 0;
}

int app_dfu_bt_adv_init(void)
{
	int err;

	err = dfu_adv_set_setup();
	if (err) {
		LOG_ERR("DFU: dfu_adv_set_setup failed (err %d)", err);
		return err;
	}

	return 0;
}
