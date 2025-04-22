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

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(ui, LOG_LEVEL_DBG);

#define APPLE_SELECTION_INDICATE_LED		DK_LED1
#define GOOGLE_SELECTION_INDICATE_LED		DK_LED2

#define DK_LED1_BLINK_INTERVAL_MS		1000
#define DK_LED1_DFU_BLINK_INTERVAL_MS		250
#define DK_LED3_ADV_PROV_BLINK_INTERVAL_MS	250
#define DK_LED3_ADV_NOT_PROV_BLINK_INTERVAL_MS	1000

#define LED_WORKQ_PRIORITY	K_PRIO_PREEMPT(0)
#define LED_WORKQ_STACK_SIZE	512

#define SELECTED_NETWORK_BITMASK	(BIT(APP_UI_MODE_SELECTED_APPLE) | \
					 BIT(APP_UI_MODE_SELECTED_GOOGLE))

static K_THREAD_STACK_DEFINE(led_workq_stack, LED_WORKQ_STACK_SIZE);
static struct k_work_q led_workq;

static void dk_led1_work_handle(struct k_work *item);
static K_WORK_DELAYABLE_DEFINE(dk_led1_work, dk_led1_work_handle);

static void dk_led2_work_handle(struct k_work *item);
static K_WORK_DELAYABLE_DEFINE(dk_led2_work, dk_led2_work_handle);

static void dk_led3_work_handle(struct k_work *item);
static K_WORK_DELAYABLE_DEFINE(dk_led3_work, dk_led3_work_handle);

static void dk_led4_work_handle(struct k_work *item);
static K_WORK_DELAYABLE_DEFINE(dk_led4_work, dk_led4_work_handle);

static struct {
	struct k_work_delayable *work;
	const uint32_t displayed_state_bm;
} led_works_map[] = {
	{
		.work = &dk_led1_work,
		.displayed_state_bm = (BIT(APP_UI_SELECTED_STATE_APP_RUNNING) |
				       BIT(APP_UI_SELECTED_STATE_DFU_MODE))
	},
	{
		.work = &dk_led2_work,
		.displayed_state_bm = BIT(APP_UI_SELECTED_STATE_RINGING)
	},
	{
		.work = &dk_led3_work,
		.displayed_state_bm = (BIT(APP_UI_SELECTED_STATE_PROVISIONED) |
				       BIT(APP_UI_SELECTED_STATE_ADVERTISING))
	},
	{
		.work = &dk_led4_work,
		.displayed_state_bm = BIT(APP_UI_SELECTED_STATE_ID_MODE)
	},
};

static ATOMIC_DEFINE(ui_state_status, APP_UI_SELECTED_STATE_COUNT);

BUILD_ASSERT(APP_UI_SELECTED_STATE_COUNT <= (sizeof(uint32_t) * __CHAR_BIT__));

static bool workq_initialized;

static void btn_handle(uint32_t button_state, uint32_t has_changed)
{
	union app_ui_request request;

	if (has_changed & button_state & DK_BTN1_MSK) {
		request.selected = APP_UI_SELECTED_REQUEST_ADVERTISING_MODE_CHANGE;
		app_ui_request_broadcast(request, SELECTED_NETWORK_BITMASK);
	}

	if (has_changed & button_state & DK_BTN2_MSK) {
		request.selected = APP_UI_SELECTED_REQUEST_RINGING_STOP;
		app_ui_request_broadcast(request, BIT(APP_UI_MODE_SELECTED_GOOGLE));
	}

	if (has_changed & button_state & DK_BTN3_MSK) {
		request.selected = APP_UI_SELECTED_REQUEST_DFU_MODE_ENTER;
		app_ui_request_broadcast(request, SELECTED_NETWORK_BITMASK);
	}

	if (has_changed & button_state & DK_BTN4_MSK) {
		request.selected = APP_UI_SELECTED_REQUEST_ID_MODE_ENTER;
		app_ui_request_broadcast(request, SELECTED_NETWORK_BITMASK);
	}
}

static struct button_handler button_handler = {
	.cb = btn_handle,
};

static void mode_switch_btn_handle(void)
{
	uint32_t button_state;
	uint32_t has_changed;
	union app_ui_request request;

	dk_read_buttons(&button_state, &has_changed);

	if (button_state & DK_BTN4_MSK) {
		request.selected = APP_UI_SELECTED_REQUEST_FACTORY_RESET;
		app_ui_request_broadcast(request, SELECTED_NETWORK_BITMASK);
	}
}

static void dk_led1_work_handle(struct k_work *item)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(item);
	static bool run_led_on;

	if (atomic_test_bit(ui_state_status, APP_UI_SELECTED_STATE_APP_RUNNING)) {
		uint32_t blink_interval_ms = atomic_test_bit(ui_state_status,
							     APP_UI_SELECTED_STATE_DFU_MODE) ?
					     DK_LED1_DFU_BLINK_INTERVAL_MS :
					     DK_LED1_BLINK_INTERVAL_MS;

		run_led_on = !run_led_on;
		(void) k_work_reschedule_for_queue(&led_workq, dwork, K_MSEC(blink_interval_ms));
	} else {
		run_led_on = false;
	}

	dk_set_led(DK_LED1, run_led_on);
}

static void dk_led2_work_handle(struct k_work *item)
{
	bool ringing = atomic_test_bit(ui_state_status, APP_UI_SELECTED_STATE_RINGING);

	dk_set_led(DK_LED2, ringing);
}

static void dk_led3_work_handle(struct k_work *item)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(item);
	static bool provisioning_led_on;

	bool provisioned = atomic_test_bit(ui_state_status, APP_UI_SELECTED_STATE_PROVISIONED);
	bool adv_on = atomic_test_bit(ui_state_status, APP_UI_SELECTED_STATE_ADVERTISING);

	if (adv_on) {
		uint32_t blink_interval_ms = provisioned ?
					 DK_LED3_ADV_PROV_BLINK_INTERVAL_MS :
					 DK_LED3_ADV_NOT_PROV_BLINK_INTERVAL_MS;

		provisioning_led_on = !provisioning_led_on;
		(void) k_work_reschedule_for_queue(&led_workq, dwork, K_MSEC(blink_interval_ms));
	} else {
		provisioning_led_on = provisioned;
	}

	dk_set_led(DK_LED3, provisioning_led_on);
}

static void dk_led4_work_handle(struct k_work *item)
{
	ARG_UNUSED(item);

	bool enabled = atomic_test_bit(ui_state_status, APP_UI_SELECTED_STATE_ID_MODE);

	dk_set_led(DK_LED4, enabled);
}

static int ui_selected_state_change_indicate(union app_ui_state state, bool active)
{
	__ASSERT_NO_MSG(state.selected < APP_UI_SELECTED_STATE_COUNT);

	atomic_set_bit_to(ui_state_status, state.selected, active);

	for (size_t i = 0; i < ARRAY_SIZE(led_works_map); i++) {
		if (led_works_map[i].displayed_state_bm & BIT(state.selected)) {
			(void) k_work_reschedule_for_queue(&led_workq,
							   led_works_map[i].work,
							   K_NO_WAIT);
		}
	}

	return 0;
}

static void led_blink(uint32_t led, uint8_t cnt, uint32_t on_ms,
		      uint32_t off_ms, uint32_t delayed_ms)
{
	k_msleep(delayed_ms);

	for (uint8_t i = 0; i < cnt; i++) {
		dk_set_led(led, true);
		k_msleep(on_ms);
		dk_set_led(led, false);
		k_msleep(off_ms);
	}
}

static void network_selected_led_blink(uint32_t led)
{
	led_blink(led, 3, 100, 100, 100);
}

static void mode_switch_led_indicate(void)
{
	enum app_ui_mode mode = app_ui_mode_get();

	if (mode == APP_UI_MODE_SELECTED_APPLE) {
		network_selected_led_blink(APPLE_SELECTION_INDICATE_LED);
	} else if (mode == APP_UI_MODE_SELECTED_GOOGLE) {
		network_selected_led_blink(GOOGLE_SELECTION_INDICATE_LED);
	}
}

static int ui_selected_init(void)
{
	if (!workq_initialized) {
		k_work_queue_init(&led_workq);
		k_work_queue_start(&led_workq, led_workq_stack,
				K_THREAD_STACK_SIZEOF(led_workq_stack), LED_WORKQ_PRIORITY,
				NULL);

		workq_initialized = true;
	}

	dk_button_handler_add(&button_handler);

	mode_switch_led_indicate();
	mode_switch_btn_handle();

	return 0;
}

static int ui_selected_uninit(void)
{
	dk_button_handler_remove(&button_handler);

	for (size_t i = 0; i < ARRAY_SIZE(led_works_map); i++) {
		(void) k_work_cancel_delayable(led_works_map[i].work);
	}

	return 0;
}

static struct app_ui_cb ui_selected_callbacks = {
	.init = ui_selected_init,
	.change_indicate = ui_selected_state_change_indicate,
	.uninit = ui_selected_uninit,
};

static int ui_selected_register(void)
{
	app_ui_register((BIT(APP_UI_MODE_SELECTED_APPLE) | BIT(APP_UI_MODE_SELECTED_GOOGLE)),
			&ui_selected_callbacks);

	return 0;
}

SYS_INIT(ui_selected_register, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
