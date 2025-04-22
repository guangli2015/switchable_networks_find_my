/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/kernel.h>

#include "app_factory_reset.h"
#include "app_network_selector.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(app_factory_reset, LOG_LEVEL_INF);

/** Factory reset state. */
enum app_factory_reset_state {
	/* Idle state. */
	APP_FACTORY_RESET_STATE_IDLE,

	/* Pending state. */
	APP_FACTORY_RESET_STATE_PENDING,

	/* Factory reset in progress state. */
	APP_FACTORY_RESET_STATE_IN_PROGRESS,
};

static void factory_reset_work_handle(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(factory_reset_work, factory_reset_work_handle);
static enum app_factory_reset_state factory_reset_state;

static void factory_reset_perform(void)
{
	int err;

	if (factory_reset_state == APP_FACTORY_RESET_STATE_IN_PROGRESS) {
		__ASSERT_NO_MSG(false);
		return;
	}

	factory_reset_state = APP_FACTORY_RESET_STATE_IN_PROGRESS;

	err = app_network_selector_set(APP_NETWORK_SELECTOR_UNSELECTED);
	if (err) {
		LOG_ERR("Factory Reset: Unselecting network failed (err %d)", err);
	}

	/* Should not reach. */
	__ASSERT_NO_MSG(false);
	k_panic();
}

static void factory_reset_work_handle(struct k_work *work)
{
	factory_reset_perform();
}

void app_factory_reset_schedule(k_timeout_t delay)
{
	if (factory_reset_state != APP_FACTORY_RESET_STATE_IDLE) {
		LOG_ERR("Factory Reset: rejecting scheduling operation, already scheduled");
		return;
	}

	(void) k_work_schedule(&factory_reset_work, delay);
	factory_reset_state = APP_FACTORY_RESET_STATE_PENDING;
}

void app_factory_reset_cancel(void)
{
	if (factory_reset_state == APP_FACTORY_RESET_STATE_IN_PROGRESS) {
		LOG_ERR("Factory Reset: rejecting cancelling operation, already in progress");
		return;
	}

	(void) k_work_cancel_delayable(&factory_reset_work);
	factory_reset_state = APP_FACTORY_RESET_STATE_IDLE;
}

int app_factory_reset_init(bool requested)
{
	__ASSERT_NO_MSG(factory_reset_state == APP_FACTORY_RESET_STATE_IDLE);

	if (requested) {
		factory_reset_perform();
	}

	return 0;
}
