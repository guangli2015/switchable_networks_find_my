/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <stdio.h>

#include <zephyr/kernel.h>

#include <bluetooth/services/fast_pair/fmdn.h>

#include "app_ring.h"
#include "app_ui.h"
#include "app_ui_selected.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(google, LOG_LEVEL_INF);

static uint8_t fmdn_ring_active_comp_bm;
static enum bt_fast_pair_fmdn_ring_src fmdn_ring_src;

BUILD_ASSERT(IS_ENABLED(CONFIG_BT_FAST_PAIR_FMDN_RING_COMP_ONE));

static void ring_state_update(struct bt_fast_pair_fmdn_ring_state_param *param)
{
	int err;
	union app_ui_state state = {.selected = APP_UI_SELECTED_STATE_RINGING};

	err = bt_fast_pair_fmdn_ring_state_update(fmdn_ring_src, param);
	if (err) {
		LOG_ERR("FMDN: bt_fast_pair_fmdn_ring_state_update failed (err %d)",
			err);
		return;
	}

	app_ui_state_change_indicate(
		state,
		(param->active_comp_bm != BT_FAST_PAIR_FMDN_RING_COMP_BM_NONE));

	fmdn_ring_active_comp_bm = param->active_comp_bm;
}

static void ring_provisioning_state_changed(bool provisioned)
{
	/* Clean up the ringing state in case it is necessary. */
	if (!provisioned && (fmdn_ring_active_comp_bm != BT_FAST_PAIR_FMDN_RING_COMP_BM_NONE)) {
		struct bt_fast_pair_fmdn_ring_state_param param = {0};

		param.trigger = BT_FAST_PAIR_FMDN_RING_TRIGGER_GATT_STOPPED;
		param.active_comp_bm = BT_FAST_PAIR_FMDN_RING_COMP_BM_NONE;

		ring_state_update(&param);
	}
}

static struct bt_fast_pair_fmdn_info_cb fmdn_info_cb = {
	.provisioning_state_changed = ring_provisioning_state_changed,
};

static const char *ring_src_str_get(enum bt_fast_pair_fmdn_ring_src src)
{
	static const char * const ring_src_description[] = {
		[BT_FAST_PAIR_FMDN_RING_SRC_FMDN_BT_GATT] = "Bluetooth GATT FMDN",
		[BT_FAST_PAIR_FMDN_RING_SRC_DULT_BT_GATT] = "Bluetooth GATT DULT",
		[BT_FAST_PAIR_FMDN_RING_SRC_DULT_MOTION_DETECTOR] = "Motion Detector DULT",
	};

	__ASSERT((src < ARRAY_SIZE(ring_src_description)) && ring_src_description[src],
		 "Unknown ring source");

	return ring_src_description[src];
};

static void fmdn_ring_start_request(
	enum bt_fast_pair_fmdn_ring_src src,
	const struct bt_fast_pair_fmdn_ring_req_param *ring_param)
{
	struct bt_fast_pair_fmdn_ring_state_param param = {0};
	static const char * const volume_str[] = {
		[BT_FAST_PAIR_FMDN_RING_VOLUME_DEFAULT] = "Default",
		[BT_FAST_PAIR_FMDN_RING_VOLUME_LOW] = "Low",
		[BT_FAST_PAIR_FMDN_RING_VOLUME_MEDIUM] = "Medium",
		[BT_FAST_PAIR_FMDN_RING_VOLUME_HIGH] = "High",
	};

	/* Assuming that the single ringing component is the case. */
	uint8_t active_comp_bm =
		(ring_param->active_comp_bm == BT_FAST_PAIR_FMDN_RING_COMP_BM_ALL) ?
		BT_FAST_PAIR_FMDN_RING_COMP_CASE : ring_param->active_comp_bm;

	if (active_comp_bm != BT_FAST_PAIR_FMDN_RING_COMP_CASE) {
		LOG_WRN("FMDN: skipping the ringing action request due to "
			"invalid component (BM=0x%02X)", ring_param->active_comp_bm);
		LOG_WRN("FMDN: the application supports only the single ringing component: "
			"Case (BM=0x%02X)", BT_FAST_PAIR_FMDN_RING_COMP_CASE);
		return;
	}

	LOG_INF("FMDN: starting ringing action with the following parameters:");
	LOG_INF("\tSource:\t\t%s", ring_src_str_get(src));
	LOG_INF("\tComponents:\tCase=%sactive (BM=0x%02X)",
		((active_comp_bm & BT_FAST_PAIR_FMDN_RING_COMP_CASE) ? "" : "in"),
		ring_param->active_comp_bm);
	LOG_INF("\tTimeout:\t%d [ds]", ring_param->timeout);
	LOG_INF("\tVolume:\t\t%s (0x%02X)",
		(ring_param->volume < ARRAY_SIZE(volume_str)) ?
			volume_str[ring_param->volume] : "Unknown",
		ring_param->volume);

	param.trigger = BT_FAST_PAIR_FMDN_RING_TRIGGER_STARTED;
	param.active_comp_bm = active_comp_bm;
	param.timeout = ring_param->timeout;

	/* In this sample, the application always accepts the new ring source
	 * regardless of the current active source.
	 */
	fmdn_ring_src = src;

	ring_state_update(&param);
}

static void fmdn_ring_timeout_expired(enum bt_fast_pair_fmdn_ring_src src)
{
	struct bt_fast_pair_fmdn_ring_state_param param = {0};

	/* The timeout source cannot differ from the last ring start operation that
	 * was accepted using the bt_fast_pair_fmdn_ring_state_update function.
	 */
	__ASSERT_NO_MSG(src == fmdn_ring_src);

	LOG_INF("FMDN: stopping the ringing action on timeout");
	LOG_INF("\tSource:\t%s", ring_src_str_get(src));

	param.trigger = BT_FAST_PAIR_FMDN_RING_TRIGGER_TIMEOUT_STOPPED;
	param.active_comp_bm = BT_FAST_PAIR_FMDN_RING_COMP_BM_NONE;

	ring_state_update(&param);
}

static void fmdn_ring_stop_request(enum bt_fast_pair_fmdn_ring_src src)
{
	struct bt_fast_pair_fmdn_ring_state_param param = {0};

	LOG_INF("FMDN: stopping the ringing action on GATT request");
	LOG_INF("\tSource:\t%s", ring_src_str_get(src));

	param.trigger = BT_FAST_PAIR_FMDN_RING_TRIGGER_GATT_STOPPED;
	param.active_comp_bm = BT_FAST_PAIR_FMDN_RING_COMP_BM_NONE;

	/* In this sample, the application always accepts the new ring source
	 * regardless of the current active source.
	 */
	fmdn_ring_src = src;

	ring_state_update(&param);
}

static const struct bt_fast_pair_fmdn_ring_cb fmdn_ring_cb = {
	.start_request = fmdn_ring_start_request,
	.timeout_expired = fmdn_ring_timeout_expired,
	.stop_request = fmdn_ring_stop_request,
};

static void fmdn_ring_ui_stop(void)
{
	struct bt_fast_pair_fmdn_ring_state_param param = {0};

	/* It is assumed that the callback executes in the cooperative
	 * thread context as it interacts with the FMDN API.
	 */
	__ASSERT_NO_MSG(!k_is_preempt_thread());
	__ASSERT_NO_MSG(!k_is_in_isr());

	if (fmdn_ring_active_comp_bm == BT_FAST_PAIR_FMDN_RING_COMP_BM_NONE) {
		return;
	}

	param.trigger = BT_FAST_PAIR_FMDN_RING_TRIGGER_UI_STOPPED;
	param.active_comp_bm = BT_FAST_PAIR_FMDN_RING_COMP_BM_NONE;

	ring_state_update(&param);

	LOG_INF("FMDN: stopping the ringing action on button press");
}

static void ui_request_handle(union app_ui_request request)
{
	/* It is assumed that the callback executes in the cooperative
	 * thread context as it interacts with the FMDN API.
	 */
	__ASSERT_NO_MSG(!k_is_preempt_thread());
	__ASSERT_NO_MSG(!k_is_in_isr());

	if (request.selected == APP_UI_SELECTED_REQUEST_RINGING_STOP) {
		fmdn_ring_ui_stop();
	}
}

int app_ring_init(void)
{
	int err;

	err = bt_fast_pair_fmdn_ring_cb_register(&fmdn_ring_cb);
	if (err) {
		LOG_ERR("FMDN: bt_fast_pair_fmdn_ring_cb_register failed (err %d)", err);
		return err;
	}

	err = bt_fast_pair_fmdn_info_cb_register(&fmdn_info_cb);
	if (err) {
		LOG_ERR("Fast Pair: bt_fast_pair_fmdn_info_cb_register failed (err %d)", err);
		return err;
	}

	return 0;
}

APP_UI_REQUEST_LISTENER_REGISTER(ui_network_google_ring,
				 APP_UI_MODE_SELECTED_GOOGLE,
				 ui_request_handle);
