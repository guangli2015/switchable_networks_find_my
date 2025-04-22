/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>

#include <zephyr/settings/settings.h>

#include "app_ui.h"
#include "app_network_selector.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(app, LOG_LEVEL_INF);

#define INIT_SEM_TIMEOUT (60)

static void init_work_handle(struct k_work *work);

static K_SEM_DEFINE(init_work_sem, 0, 1);
static K_WORK_DEFINE(init_work, init_work_handle);

static int app_bt_id_create(void)
{
	int ret;
	size_t count;
	uint8_t max_id = MAX(CONFIG_APP_NETWORK_BT_ID,
			     (IS_ENABLED(CONFIG_APP_DFU) * CONFIG_APP_DFU_BT_ID));

	BUILD_ASSERT(CONFIG_APP_NETWORK_BT_ID != BT_ID_DEFAULT);
	BUILD_ASSERT(CONFIG_APP_NETWORK_BT_ID < CONFIG_BT_ID_MAX);

	BUILD_ASSERT(CONFIG_APP_DFU_BT_ID < CONFIG_BT_ID_MAX);

	/* Check if application identity wasn't already created. */
	bt_id_get(NULL, &count);
	if (count > max_id) {
		return 0;
	}

	/* Create the application identity. */
	do {
		ret = bt_id_create(NULL, NULL);
		if (ret < 0) {
			return ret;
		}
	} while (ret != max_id);

	return 0;
}

static void init_work_handle(struct k_work *work)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("bt_enable failed (err %d)", err);
		return;
	}

	err = settings_load();
	if (err) {
		LOG_ERR("settings_load failed (err %d)", err);
		return;
	}

	err = app_bt_id_create();
	if (err) {
		LOG_ERR("Network identity failed to create (err %d)", err);
		return;
	}

	err = app_ui_init();
	if (err) {
		LOG_ERR("app_ui_init failed (err %d)", err);
		return;
	}

	err = app_network_selector_init();
	if (err) {
		LOG_ERR("app_network_selector_init init failed (err %d)", err);
		return;
	}

	k_sem_give(&init_work_sem);
}

void main(void)
{
	int err;

	LOG_INF("Starting the Switchable Networks application");

	(void) k_work_submit(&init_work);
	err = k_sem_take(&init_work_sem, K_SECONDS(INIT_SEM_TIMEOUT));
	if (err) {
		k_panic();
	}

	app_network_selector_launch();
}
