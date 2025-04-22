/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include <dk_buttons_and_leds.h>

#include "app_ui.h"
#include "app_ui_priv.h"
#include "app_ui_unselected.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(ui, LOG_LEVEL_DBG);

#define UNSELECTED_NETWORK_BITMASK	(BIT(APP_UI_MODE_UNSELECTED))

static ATOMIC_DEFINE(ui_state_status, APP_UI_UNSELECTED_STATE_COUNT);

BUILD_ASSERT(APP_UI_UNSELECTED_STATE_COUNT <= (sizeof(uint32_t) * __CHAR_BIT__));

static const struct {
	enum app_ui_unselected_request request;
	uint32_t network_btn;
	const char *network_name;
} network_btn_map[] = {
	{
		.request = APP_UI_UNSELECTED_REQUEST_NETWORK_APPLE,
		.network_btn = DK_BTN1,
		.network_name = "Apple Find My",
	},
	{
		.request = APP_UI_UNSELECTED_REQUEST_NETWORK_GOOGLE,
		.network_btn = DK_BTN2,
		.network_name = "Google Find My Device",
	},
};

static void btn_handle(uint32_t button_state, uint32_t has_changed)
{
	union app_ui_request request;

	for (size_t i = 0; i < ARRAY_SIZE(network_btn_map); i++) {
		if (has_changed & button_state & BIT(network_btn_map[i].network_btn)) {
			request.unselected = network_btn_map[i].request;
			app_ui_request_broadcast(request, UNSELECTED_NETWORK_BITMASK);
		}
	}
}

static struct button_handler button_handler = {
	.cb = btn_handle,
};

static int ui_unselected_state_change_indicate(union app_ui_state state, bool active)
{
	__ASSERT_NO_MSG(state.unselected < APP_UI_UNSELECTED_STATE_COUNT);

	atomic_set_bit_to(ui_state_status, state.unselected, active);

	if (state.unselected == APP_UI_UNSELECTED_STATE_SELECTION_MENU) {
		dk_set_leds(active ? DK_ALL_LEDS_MSK : DK_NO_LEDS_MSK);
		return 0;
	}

	return 0;
}

static int ui_unselected_init(void)
{
	dk_button_handler_add(&button_handler);

	return 0;
}

static int ui_unselected_uninit(void)
{
	dk_button_handler_remove(&button_handler);

	return 0;
}

static struct app_ui_cb ui_unselected_callbacks = {
	.init = ui_unselected_init,
	.change_indicate = ui_unselected_state_change_indicate,
	.uninit = ui_unselected_uninit,
};

static int ui_unselected_register(void)
{
	app_ui_register(BIT(APP_UI_MODE_UNSELECTED), &ui_unselected_callbacks);

	return 0;
}

SYS_INIT(ui_unselected_register, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

void app_ui_unselected_network_choice_present(void)
{
	uint32_t network_btn;

	LOG_INF("Select the network by pressing one of the following buttons:");

	for (size_t i = 0; i < ARRAY_SIZE(network_btn_map); i++) {
		network_btn = network_btn_map[i].network_btn;

		if (IS_ENABLED(CONFIG_SOC_SERIES_NRF54HX) ||
		    IS_ENABLED(CONFIG_SOC_SERIES_NRF54LX)) {
			/* Intentionally left empty,
			 * network_btn index is equal to DK label.
			 */
		} else if (IS_ENABLED(CONFIG_SOC_SERIES_NRF52X) ||
			   IS_ENABLED(CONFIG_SOC_SERIES_NRF53X)) {
			network_btn++;
		} else {
			__ASSERT(0, "Unsupported SoC series");
		}

		__ASSERT_NO_MSG(network_btn_map[i].network_name != NULL);

		LOG_INF("+ Button %d: %s", network_btn, network_btn_map[i].network_name);
	}
}
