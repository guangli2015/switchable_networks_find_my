/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>

#include <zephyr/sys/reboot.h>

#include "app_dfu.h"
#include "app_ui.h"
#include "app_ui_unselected.h"
#include "app_network_selector.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(unselected, LOG_LEVEL_INF);

/* Semaphore timeout in seconds. */
#define INIT_SEM_TIMEOUT (60)

/* Timeout in minutes to reset the device and restore the old image in case
 * when the currently booted image will not be confirmed.
 */
#define IMAGE_CONFIRMATION_TIMEOUT_MIN	(2)

static void init_work_handle(struct k_work *work);
static K_SEM_DEFINE(init_work_sem, 0, 1);
static K_WORK_DEFINE(init_work, init_work_handle);

static void selected_work_handle(struct k_work *work);
static K_WORK_DEFINE(selected_work, selected_work_handle);

static void image_confirmation_timeout_handle(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(image_confirmation_timeout_work, image_confirmation_timeout_handle);

/** Networks available to be chosen in the selection mode. */
static const struct {
	enum app_network_selector network;
	enum app_ui_unselected_request request;
} network_ui_map[] = {
	{
		.network = APP_NETWORK_SELECTOR_APPLE,
		.request = APP_UI_UNSELECTED_REQUEST_NETWORK_APPLE,
	},
	{
		.network = APP_NETWORK_SELECTOR_GOOGLE,
		.request = APP_UI_UNSELECTED_REQUEST_NETWORK_GOOGLE,
	},
};

static enum app_network_selector network = APP_NETWORK_SELECTOR_UNSELECTED;

static void ui_request_handle(union app_ui_request request)
{
	__ASSERT_NO_MSG(network == APP_NETWORK_SELECTOR_UNSELECTED);

	if (IS_ENABLED(CONFIG_APP_DFU) && !app_dfu_is_confirmed()) {
		LOG_WRN("Network selection is disabled, waiting for the image confirmation");
		return;
	}

	for (size_t i = 0; i < ARRAY_SIZE(network_ui_map); i++) {
		if (network_ui_map[i].request == request.unselected) {
			network = network_ui_map[i].network;
			app_network_selector_set(network);
			k_work_submit(&selected_work);
			break;
		}
	}
}

static int factory_reset_perform(void)
{
	/* Intentionally left empty.
	 * There is no way to leave unselected network beside choosing
	 * one of the available networks.
	 */

	return 0;
}

static void image_confirmation_timeout_handle(struct k_work *work)
{
	__ASSERT_NO_MSG(!app_dfu_is_confirmed());

	LOG_INF("DFU: Image confirmation timeout expired, restoring the old image...");

	sys_reboot(SYS_REBOOT_COLD);

	/* Should not reach. */
	k_panic();
}

static void image_confirmed_handle(void)
{
	__ASSERT_NO_MSG(app_dfu_is_confirmed());

	LOG_INF("DFU: The current image is confirmed, the network selection has been unlocked");

	k_work_cancel_delayable(&image_confirmation_timeout_work);

	app_ui_unselected_network_choice_present();
}

static struct app_dfu_cb dfu_callbacks = {
	.image_confirmed = image_confirmed_handle,
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

	err = app_dfu_cb_register(&dfu_callbacks);
	if (err) {
		LOG_ERR("app_dfu_cb_register failed (err %d)", err);
		return err;
	}

	err = app_dfu_init();
	if (err) {
		LOG_ERR("app_dfu_init failed (err %d)", err);
		return err;
	}

	app_dfu_mode_enter(true);

	if (!app_dfu_is_confirmed()) {
		LOG_INF("DFU: The current image is not confirmed, entering the DFU mode to "
			"allow confirm operation");
		LOG_INF("DFU: Network selection is temporarily disabled until the new image "
			"is confirmed");
		LOG_INF("DFU: If the device will not be confirmed within %d minutes, the device "
			"will be reset and the old image will be restored",
			IMAGE_CONFIRMATION_TIMEOUT_MIN);
		LOG_INF("DFU: Waiting for image confirmation...");

		k_work_reschedule(&image_confirmation_timeout_work,
				  K_MINUTES(IMAGE_CONFIRMATION_TIMEOUT_MIN));
	}

	return 0;
}

static void init_work_handle(struct k_work *work)
{
	int err;

	err = app_ui_mode_set(APP_UI_MODE_UNSELECTED);
	if (err) {
		LOG_ERR("Failed to set the unselected UI mode (err %d)", err);
		return;
	}

	if (IS_ENABLED(CONFIG_APP_DFU)) {
		err = dfu_init();
		if (err) {
			LOG_ERR("dfu_init failed (err %d)", err);
			return;
		}

		/* If the currently booted image is not confirmed yet,
		 * the network choice will be presented once confirmed later.
		 * Otherwise, the network choice will be presented immediately.
		 */
		if (app_dfu_is_confirmed()) {
			app_ui_unselected_network_choice_present();
		}
	} else {
		/* If the DFU module is not enabled, the network choice
		 * will be presented immediately.
		 */
		app_ui_unselected_network_choice_present();
	}

	k_sem_give(&init_work_sem);
}

static void selected_work_handle(struct k_work *work)
{
	union app_ui_state state = {.unselected = APP_UI_UNSELECTED_STATE_SELECTION_MENU};

	app_ui_state_change_indicate(state, false);

	if (IS_ENABLED(CONFIG_APP_DFU)) {
		app_dfu_mode_exit();
	}

	/* Newline added to keep the log after reboot. */
	LOG_INF("Rebooting to enter the selected network...\n");

	sys_reboot(SYS_REBOOT_COLD);
}

void app_network_unselected_run(void)
{
	int err;
	union app_ui_state state = {.unselected = APP_UI_UNSELECTED_STATE_SELECTION_MENU};

	(void) k_work_submit(&init_work);
	err = k_sem_take(&init_work_sem, K_SECONDS(INIT_SEM_TIMEOUT));
	if (err) {
		k_panic();
	}

	app_ui_state_change_indicate(state, true);
}

APP_UI_REQUEST_LISTENER_REGISTER(ui_network_unselected,
				 APP_UI_MODE_UNSELECTED,
				 ui_request_handle);

APP_NETWORK_SELECTOR_DESC_REGISTER(network_unselected,
				   APP_NETWORK_SELECTOR_UNSELECTED,
				   app_network_unselected_run,
				   factory_reset_perform);
