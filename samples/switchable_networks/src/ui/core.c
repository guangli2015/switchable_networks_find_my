/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/kernel.h>

#include <dk_buttons_and_leds.h>

#include "app_ui.h"
#include "app_ui_priv.h"
#include "app_ui_unselected.h"
#include "app_ui_selected.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(ui, LOG_LEVEL_INF);

static enum app_ui_mode current_mode = APP_UI_MODE_COUNT;
static struct app_ui_cb *ui_handlers[APP_UI_MODE_COUNT];

int app_ui_state_change_indicate(union app_ui_state state, bool active)
{
	__ASSERT_NO_MSG(current_mode < APP_UI_MODE_COUNT);

	return ui_handlers[current_mode]->change_indicate(state, active);
}

void app_ui_request_broadcast(union app_ui_request request, uint32_t mode_bitmask)
{
	__ASSERT_NO_MSG(current_mode < APP_UI_MODE_COUNT);

	if (!(BIT(current_mode) & mode_bitmask)) {
		/* Request is not related to the current mode. */
		return;
	}

	STRUCT_SECTION_FOREACH(app_ui_request_listener, listener) {
		if (listener->mode == current_mode) {
			listener->handler(request);
		}
	}
}

static int ui_init(void)
{
	__ASSERT_NO_MSG(current_mode < APP_UI_MODE_COUNT);

	return ui_handlers[current_mode]->init();
}

static int ui_uninit(void)
{
	if (current_mode >= APP_UI_MODE_COUNT) {
		return 0;
	}

	return ui_handlers[current_mode]->uninit();
}

int app_ui_mode_set(enum app_ui_mode mode)
{
	int err;

	if (mode >= APP_UI_MODE_COUNT) {
		LOG_ERR("Invalid UI mode %d", mode);
		return -EINVAL;
	}

	if (current_mode == mode) {
		LOG_INF("UI mode already set to %d", mode);
		return 0;
	}

	err = ui_uninit();
	if (err) {
		LOG_ERR("Failed to uninit the UI mode (err %d)", err);
		return err;
	}

	current_mode = mode;

	err = ui_init();
	if (err) {
		LOG_ERR("Failed to unit the UI mode (err %d)", err);
		return err;
	}

	return 0;
}

enum app_ui_mode app_ui_mode_get(void)
{
	__ASSERT_NO_MSG(current_mode < APP_UI_MODE_COUNT);

	return current_mode;
}

void app_ui_register(uint32_t mode_bitmask, struct app_ui_cb *handler)
{
	__ASSERT_NO_MSG(mode_bitmask);

	__ASSERT_NO_MSG(handler);
	__ASSERT_NO_MSG(handler->init);
	__ASSERT_NO_MSG(handler->change_indicate);
	__ASSERT_NO_MSG(handler->uninit);

	for (size_t i = 0; i < ARRAY_SIZE(ui_handlers); i++) {
		if (BIT(i) & mode_bitmask) {
			__ASSERT_NO_MSG(ui_handlers[i] == NULL);

			ui_handlers[i] = handler;
		}
	}
}

int app_ui_init(void)
{
	int err;

	err = dk_leds_init();
	if (err) {
		LOG_ERR("dk_leds_init failed (err %d)", err);
		return err;
	}

	err = dk_buttons_init(NULL);
	if (err) {
		LOG_ERR("dk_buttons_init failed (err: %d)", err);
		return err;
	}

	for (size_t i = 0; i < ARRAY_SIZE(ui_handlers); i++) {
		__ASSERT_NO_MSG(ui_handlers[i] != NULL);
	}

	return 0;
}
