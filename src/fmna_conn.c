/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "events/fmna_config_event.h"
#include "events/fmna_event.h"
#include "fmna_conn.h"
#include "fmna_state.h"
#include "fmna_gatt_fmns.h"
//#include "app_network_selector.h"
#include <zephyr/sys/util.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

#define MAX_CONN_WORK_CHECK_PERIOD K_MSEC(100)

BUILD_ASSERT(!(IS_ENABLED(CONFIG_FMNA_BT_PAIRING_NO_BONDING) &&
	     IS_ENABLED(CONFIG_BT_BONDING_REQUIRED)),
	     "CONFIG_FMNA_BT_PAIRING_NO_BONDING cannot be used together "
	     "with CONFIG_BT_BONDING_REQUIRED");

struct conn_owner_finder {
	struct bt_conn ** const owners;
	uint8_t * const owner_cnt;
	const uint8_t owner_array_size;
};

struct conn_status_finder {
	const struct {
		enum fmna_conn_multi_status_bit status_bit;
	} in;
	struct {
		bool is_found;
	} out;
};

struct conn_disconnecter {
	int disconnect_num;
	struct bt_conn *req_conn;
};

struct fmna_conn {
	uint32_t multi_status;
	bool is_valid;
	bool is_disconnecting;
};

static struct fmna_conn conns[CONFIG_BT_MAX_CONN];
static uint8_t max_connections = CONFIG_FMNA_MAX_CONN;
static uint8_t fmna_bt_id;

static void max_conn_work_handle(struct k_work *item);

static struct max_conn_work {
	struct k_work_delayable item;
	struct bt_conn *conn;
	struct bt_conn *disconnecting_conns[CONFIG_BT_MAX_CONN];
} max_conn_work;

struct conn_timeout_work {
	struct k_work_delayable item;
	struct bt_conn *conn;
} conn_timeout_works[CONFIG_BT_MAX_CONN];

static void conn_timeout_work_handle(struct k_work *item)
{
	BUILD_ASSERT(CONFIG_FMNA_CONN_SECURITY_TIMEOUT >= 0);

	int err;
	struct k_work_delayable *work_d;
	struct conn_timeout_work *conn_timeout_work;

	__ASSERT(CONFIG_FMNA_CONN_SECURITY_TIMEOUT > 0,
		"Connection security timeout should be greater than zero");

	work_d = k_work_delayable_from_work(item);
	conn_timeout_work = CONTAINER_OF(work_d, struct conn_timeout_work, item);

	__ASSERT(conn_timeout_work->conn,
		"Timeout work should have a connection associated with it");

	LOG_WRN("Link encryption timed out");

	err = bt_conn_disconnect(conn_timeout_work->conn,
				 BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	if (err && (err != -ENOTCONN)) {
		LOG_ERR("fmna_conn: bt_conn_disconnect returned error: %d", err);
	}

	conn_timeout_work->conn = NULL;
}

static void conn_timeout_work_init(void)
{
	if (CONFIG_FMNA_CONN_SECURITY_TIMEOUT == 0) {
		return;
	}

	for (size_t i = 0; i < ARRAY_SIZE(conn_timeout_works); i++) {
		k_work_init_delayable(&conn_timeout_works[i].item,
				      conn_timeout_work_handle);
	}
}

static void conn_timeout_work_schedule(struct bt_conn *conn)
{
	struct conn_timeout_work *conn_timeout_work;

	if (CONFIG_FMNA_CONN_SECURITY_TIMEOUT == 0) {
		return;
	}

	conn_timeout_work = &conn_timeout_works[bt_conn_index(conn)];

	conn_timeout_work->conn = conn;
	(void)k_work_schedule(&conn_timeout_work->item,
			      K_SECONDS(CONFIG_FMNA_CONN_SECURITY_TIMEOUT));
}

static void conn_timeout_work_cancel(struct bt_conn *conn)
{
	struct conn_timeout_work *conn_timeout_work;

	if (CONFIG_FMNA_CONN_SECURITY_TIMEOUT == 0) {
		return;
	}

	conn_timeout_work = &conn_timeout_works[bt_conn_index(conn)];

	(void)k_work_cancel_delayable(&conn_timeout_work->item);
	conn_timeout_work->conn = NULL;
}

bool fmna_conn_check(struct bt_conn *conn)
{
	struct bt_conn_info conn_info;

	if (!fmna_state_is_enabled()) {
		return false;
	}

	bt_conn_get_info(conn, &conn_info);

	return (conn_info.id == fmna_bt_id);
}

static enum bt_security_err pairing_accept(
	struct bt_conn *conn,
	const struct bt_conn_pairing_feat * const feat)
{
	ARG_UNUSED(feat);

	__ASSERT(fmna_conn_check(conn), "FMNA identity is expected!");

	if (fmna_state_is_paired()) {
		LOG_WRN("Already paired, rejecting incoming BT pairing request");
		return BT_SECURITY_ERR_PAIR_NOT_ALLOWED;
	}

	return BT_SECURITY_ERR_SUCCESS;
}

static struct bt_conn_auth_cb auth_cb = {
	.pairing_accept = pairing_accept,
};
extern int google_adv_stop(void);
enum app_network_selector {
	/** Selector for the unselected network. */
	APP_NETWORK_SELECTOR_UNSELECTED,

	/** Selector for the Apple Find My network. */
	APP_NETWORK_SELECTOR_APPLE,

	/** Selector for the Google Find My Device network. */
	APP_NETWORK_SELECTOR_GOOGLE,

	/* New networks can be added here. */

	/** Number of available networks. */
	APP_NETWORK_SELECTOR_COUNT,
};
extern int app_network_selector_set(enum app_network_selector network);
static bool execute_once =false;
static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN];
	struct fmna_conn *fmna_conn = &conns[bt_conn_index(conn)];
	LOG_INF("connected_apple######");
//add by andrew
	if(false == execute_once)
	{
		app_network_selector_set(APP_NETWORK_SELECTOR_APPLE);
		google_adv_stop();
		execute_once =true;
		k_msleep(200);
		
	}
	if (!fmna_state_is_enabled()) {
		return;
	}

	if (conn_err) {
		LOG_ERR("Connection establishment error: %d", conn_err);
		return;
	}

	if (!fmna_conn_check(conn)) {
		return;
	}

	err = bt_conn_auth_cb_overlay(conn, &auth_cb);
	if (err) {
		LOG_ERR("bt_conn_auth_cb_overlay returned error: %d", err);
	}

	if (IS_ENABLED(CONFIG_FMNA_BT_PAIRING_NO_BONDING)) {
		err = bt_conn_set_bondable(conn, false);
		if (err) {
			LOG_ERR("bt_conn_set_bondable returned error: %d", err);
		}
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_DBG("FMN Peer connected: %s", addr);

	fmna_conn->is_valid = true;
	bt_conn_ref(conn);

	conn_timeout_work_schedule(conn);

	FMNA_EVENT_CREATE(event, FMNA_EVENT_PEER_CONNECTED, conn);
	APP_EVENT_SUBMIT(event);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	struct fmna_conn *fmna_conn = &conns[bt_conn_index(conn)];

	if (!fmna_state_is_enabled()) {
		return;
	}

	if (!fmna_conn_check(conn)) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_DBG("FMN Peer disconnected (reason %u): %s", reason, addr);

	fmna_conn->is_disconnecting = true;
	conn_timeout_work_cancel(conn);

	bt_conn_unref(conn);

	FMNA_EVENT_CREATE(event, FMNA_EVENT_PEER_DISCONNECTED, conn);
	APP_EVENT_SUBMIT(event);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err sec_err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (!fmna_conn_check(conn)) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	if (sec_err) {
		int err;

		LOG_ERR("FMN Peer security failed: %s level %u sec_err %d\n",
			addr, level, sec_err);

		conn_timeout_work_cancel(conn);

		err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		if (err && (err != -ENOTCONN)) {
			LOG_ERR("fmna_conn: bt_conn_disconnect returned error: %d", err);
		} else {
			LOG_INF("FMN peer disconnected");
		}
	} else {
		LOG_DBG("FMN Peer security changed: %s level %u", addr, level);

		if ((level >= BT_SECURITY_L2) && (level <= BT_SECURITY_L4)) {
			conn_timeout_work_cancel(conn);
		}
	}

	FMNA_EVENT_CREATE(event, FMNA_EVENT_PEER_SECURITY_CHANGED, conn);
	event->peer_security_changed.err = sec_err;
	event->peer_security_changed.level = level;
	APP_EVENT_SUBMIT(event);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
};

static void conn_owner_iterator(struct bt_conn *conn, void *user_data)
{
	struct conn_owner_finder *conn_owner_finder = user_data;

	if (fmna_conn_multi_status_bit_check(
		conn, FMNA_CONN_MULTI_STATUS_BIT_OWNER_CONNECTED)) {
		if (*conn_owner_finder->owner_cnt < conn_owner_finder->owner_array_size) {
			conn_owner_finder->owners[*conn_owner_finder->owner_cnt] = conn;
		}

		*conn_owner_finder->owner_cnt += 1;
	}
}

static void conn_status_iterator(struct bt_conn *conn, void *user_data)
{
	struct conn_status_finder *conn_status_finder =
		(struct conn_status_finder *) user_data;

	if (fmna_conn_multi_status_bit_check(conn, conn_status_finder->in.status_bit)) {
		conn_status_finder->out.is_found = true;
	}
}

uint8_t fmna_conn_connection_num_get(void)
{
	uint8_t cnt = 0;

	for (size_t i = 0; i < ARRAY_SIZE(conns); i++) {
		if (conns[i].is_valid && !conns[i].is_disconnecting) {
			cnt++;
		}
	}

	return cnt;
}

bool fmna_conn_limit_check(void)
{
	uint8_t cnt = fmna_conn_connection_num_get();

	return (cnt < max_connections);
}

int fmna_conn_owner_find(struct bt_conn *owner_conns[], uint8_t *owner_conn_cnt)
{
	struct conn_owner_finder conn_owner_finder = {
		.owners = owner_conns,
		.owner_cnt = owner_conn_cnt,
		.owner_array_size = *owner_conn_cnt,
	};

	/* Reset the owner connection counter. */
	*conn_owner_finder.owner_cnt = 0;

	bt_conn_foreach(BT_CONN_TYPE_LE, conn_owner_iterator, &conn_owner_finder);

	if (conn_owner_finder.owner_array_size < *conn_owner_finder.owner_cnt) {
		return -ENOMEM;
	}

	return 0;
}

bool fmna_conn_multi_status_bit_check(struct bt_conn *conn,
				      enum fmna_conn_multi_status_bit status_bit)
{
	struct fmna_conn *fmna_conn = &conns[bt_conn_index(conn)];

	__ASSERT(status_bit < (sizeof(fmna_conn->multi_status) * __CHAR_BIT__),
		 "FMNA Status bit is invalid: %d", status_bit);

	if (!fmna_conn->is_valid) {
		return false;
	}

	return (fmna_conn->multi_status & BIT(status_bit));
}

void fmna_conn_multi_status_bit_set(struct bt_conn *conn,
				    enum fmna_conn_multi_status_bit status_bit)
{
	struct fmna_conn *fmna_conn = &conns[bt_conn_index(conn)];

	if (!fmna_conn->is_valid) {
		return;
	}

	__ASSERT(status_bit < (sizeof(fmna_conn->multi_status) * __CHAR_BIT__),
		 "FMNA Status bit is invalid: %d", status_bit);

	WRITE_BIT(fmna_conn->multi_status, status_bit, 1);
}

void fmna_conn_multi_status_bit_clear(struct bt_conn *conn,
				      enum fmna_conn_multi_status_bit status_bit)
{
	struct fmna_conn *fmna_conn = &conns[bt_conn_index(conn)];

	if (!fmna_conn->is_valid) {
		return;
	}

	__ASSERT(status_bit < (sizeof(fmna_conn->multi_status) * __CHAR_BIT__),
		 "FMNA Status bit is invalid: %d", status_bit);

	WRITE_BIT(fmna_conn->multi_status, status_bit, 0);
}

int fmna_conn_init(uint8_t bt_id)
{
	k_work_init_delayable(&max_conn_work.item, max_conn_work_handle);

	/* Reset the state. */
	fmna_bt_id = bt_id;
	max_connections = CONFIG_FMNA_MAX_CONN;
	memset(conns, 0, sizeof(conns));

	conn_timeout_work_init();

	return 0;
}

static void conn_uninit_iterator(struct bt_conn *conn, void *user_data)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN];
	struct bt_conn_info conn_info;

	conn_timeout_work_cancel(conn);

	bt_conn_get_info(conn, &conn_info);

	if (conn_info.state != BT_CONN_STATE_CONNECTED) {
		return;
	}

	if (conn_info.id == fmna_bt_id) {
		/* Disconnect Find My peer. */
		err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		if (err) {
			LOG_ERR("fmna_conn: bt_conn_disconnect returned error: %d", err);
			return;
		}

		/* Remove connection reference established by this module. */
		bt_conn_unref(conn);

		bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
		LOG_DBG("Disconnecting FMN Peer: %s", addr);
	}
}

int fmna_conn_uninit(void)
{
	/* Disconnect all Find My peers. */
	bt_conn_foreach(BT_CONN_TYPE_LE, conn_uninit_iterator, NULL);

	return 0;
}

static void peer_disconnected(struct bt_conn *conn)
{
	struct fmna_conn *fmna_conn = &conns[bt_conn_index(conn)];

	memset(fmna_conn, 0, sizeof(*fmna_conn));
}

static void unpaired_state_transition_handle(void)
{
	max_connections = CONFIG_FMNA_MAX_CONN;
}

static void state_changed(void)
{
	enum fmna_state current_state;

	current_state = fmna_state_get();
	switch (current_state) {
	case FMNA_STATE_UNPAIRED:
		unpaired_state_transition_handle();
		break;
	default:
		break;
	}
}

static void persistent_conn_request_handle(struct bt_conn *conn, uint8_t persistent_conn_status)
{
	int err;
	uint16_t resp_opcode;
	struct conn_status_finder conn_status_finder = {
		.in.status_bit = FMNA_CONN_MULTI_STATUS_BIT_PERSISTENT_CONNECTION,
		.out.is_found = false,
	};

	LOG_INF("FMN Config CP: responding to persistent connection request: %d",
		persistent_conn_status);

	bt_conn_foreach(BT_CONN_TYPE_LE, conn_status_iterator, &conn_status_finder);
	if ((persistent_conn_status & BIT(0)) && !conn_status_finder.out.is_found) {
		fmna_conn_multi_status_bit_set(
			conn, FMNA_CONN_MULTI_STATUS_BIT_PERSISTENT_CONNECTION);
	} else {
		fmna_conn_multi_status_bit_clear(
			conn, FMNA_CONN_MULTI_STATUS_BIT_PERSISTENT_CONNECTION);
	}

	resp_opcode = fmna_config_event_to_gatt_cmd_opcode(
		FMNA_CONFIG_EVENT_SET_PERSISTENT_CONN_STATUS);
	FMNA_GATT_COMMAND_RESPONSE_BUILD(resp_buf, resp_opcode, FMNA_GATT_RESPONSE_STATUS_SUCCESS);
	err = fmna_gatt_config_cp_indicate(conn, FMNA_GATT_CONFIG_COMMAND_RESPONSE_IND, &resp_buf);
	if (err) {
		LOG_ERR("fmna_gatt_config_cp_indicate returned error: %d", err);
	}
}

static void conn_disconnecter_iterator(struct bt_conn *conn, void *user_data)
{
	int err;
	struct conn_disconnecter *conn_disconnecter =
		(struct conn_disconnecter *) user_data;
	struct bt_conn_info conn_info;

	if (conn_disconnecter->disconnect_num <= 0) {
		return;
	}

	if (conn_disconnecter->req_conn == conn) {
		return;
	}

	conn_timeout_work_cancel(conn);

	bt_conn_get_info(conn, &conn_info);

	if (conn_info.state != BT_CONN_STATE_CONNECTED) {
		return;
	}

	if (conn_info.id != fmna_bt_id) {
		return;
	}

	err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	if (err) {
		LOG_ERR("fmna_conn: bt_conn_disconnect returned error: %d", err);
		return;
	}

	conn_disconnecter->disconnect_num--;
	max_conn_work.disconnecting_conns[bt_conn_index(conn)] = conn;
}

static void max_conn_work_handle(struct k_work *item)
{
	struct bt_conn *conn;
	bool disconnects_done = true;

	for (size_t i = 0; i < ARRAY_SIZE(max_conn_work.disconnecting_conns); i++) {
		conn = max_conn_work.disconnecting_conns[i];
		if (conn) {
			if (conns[bt_conn_index(conn)].is_valid) {
				disconnects_done = false;
				break;
			}
		}
	}

	if (disconnects_done) {
		int err;
		uint16_t opcode;

		opcode = fmna_config_event_to_gatt_cmd_opcode(
			FMNA_CONFIG_EVENT_SET_MAX_CONNECTIONS);
		FMNA_GATT_COMMAND_RESPONSE_BUILD(
			cmd_buf, opcode, FMNA_GATT_RESPONSE_STATUS_SUCCESS);
		err = fmna_gatt_config_cp_indicate(
			max_conn_work.conn, FMNA_GATT_CONFIG_COMMAND_RESPONSE_IND, &cmd_buf);
		if (err) {
			LOG_ERR("fmna_gatt_config_cp_indicate returned error: %d", err);
		}

		max_conn_work.conn = NULL;
	} else {
		k_work_reschedule(&max_conn_work.item, MAX_CONN_WORK_CHECK_PERIOD);
	}
}

static void max_connections_request_handle(struct bt_conn *conn, uint8_t max_conns)
{
	int err;
	uint16_t opcode;
	bool use_event;
	struct conn_disconnecter conn_disconnecter;

	LOG_INF("FMN Config CP: responding to max connections settings request: %d",
		max_conns);

	if (max_conns == 0) {
		LOG_INF("Cannot set max connections to 0");
		return;
	}

	if (max_conns > CONFIG_FMNA_MAX_CONN) {
		LOG_WRN("Cannot support max connections value due to the limit: %d",
			CONFIG_FMNA_MAX_CONN);
		max_conns = CONFIG_FMNA_MAX_CONN;
	}

	conn_disconnecter.disconnect_num = (fmna_conn_connection_num_get() - max_conns);
	conn_disconnecter.req_conn = conn;

	use_event = (max_connections != max_conns);
	max_connections = max_conns;

	if (conn_disconnecter.disconnect_num > 0) {
		/* Disconnect excessive links. */
		bt_conn_foreach(BT_CONN_TYPE_LE, conn_disconnecter_iterator, &conn_disconnecter);

		if (!max_conn_work.conn) {
			max_conn_work.conn = conn;
			memset(max_conn_work.disconnecting_conns, 0,
			       sizeof(max_conn_work.disconnecting_conns));

			k_work_reschedule(&max_conn_work.item, MAX_CONN_WORK_CHECK_PERIOD);

			LOG_DBG("Delaying Set Max Connections response");
		}
	} else {
		/* Respond to the command. */
		opcode = fmna_config_event_to_gatt_cmd_opcode(
			FMNA_CONFIG_EVENT_SET_MAX_CONNECTIONS);
		FMNA_GATT_COMMAND_RESPONSE_BUILD(cmd_buf, opcode, FMNA_GATT_RESPONSE_STATUS_SUCCESS);
		err = fmna_gatt_config_cp_indicate(conn, FMNA_GATT_CONFIG_COMMAND_RESPONSE_IND, &cmd_buf);
		if (err) {
			LOG_ERR("fmna_gatt_config_cp_indicate returned error: %d", err);
		}
	}

	/* Emit the event. */
	if (use_event) {
		FMNA_EVENT_CREATE(event, FMNA_EVENT_MAX_CONN_CHANGED, conn);
		APP_EVENT_SUBMIT(event);
	}
}

static void multi_status_request_handle(struct bt_conn *conn)
{
	int err;
	uint32_t multi_status;
	uint8_t req_author_index = bt_conn_index(conn);
	NET_BUF_SIMPLE_DEFINE(status_buf, sizeof(multi_status));

	multi_status = conns[req_author_index].multi_status;
	for (size_t i = 0; i < ARRAY_SIZE(conns); i++) {
		if ((!conns[i].is_valid) || (i == req_author_index)) {
			continue;
		}

		if (fmna_conn_multi_status_bit_check(conn,
			FMNA_CONN_MULTI_STATUS_BIT_OWNER_CONNECTED)) {
			WRITE_BIT(multi_status, FMNA_CONN_MULTI_STATUS_BIT_MULTIPLE_OWNERS, 1);
			break;
		}
	}

	LOG_INF("FMN Config CP: responding to connection multi status: 0x%02X",
		multi_status);

	net_buf_simple_add_le32(&status_buf, multi_status);

	err = fmna_gatt_config_cp_indicate(conn, FMNA_GATT_CONFIG_MULTI_STATUS_IND, &status_buf);
	if (err) {
		LOG_ERR("fmna_gatt_config_cp_indicate returned error: %d", err);
	}
}

static bool app_event_handler(const struct app_event_header *aeh)
{
	if (is_fmna_event(aeh)) {
		struct fmna_event *event = cast_fmna_event(aeh);

		switch (event->id) {
		case FMNA_EVENT_PEER_DISCONNECTED:
			peer_disconnected(event->conn);
			return true;
		case FMNA_EVENT_STATE_CHANGED:
			state_changed();
			break;
		default:
			break;
		}

		return false;
	}

	if (is_fmna_config_event(aeh)) {
		struct fmna_config_event *event = cast_fmna_config_event(aeh);

		switch (event->id) {
		case FMNA_CONFIG_EVENT_SET_PERSISTENT_CONN_STATUS:
			persistent_conn_request_handle(event->conn,
						       event->persistent_conn_status);
			break;
		case FMNA_CONFIG_EVENT_SET_MAX_CONNECTIONS:
			max_connections_request_handle(event->conn,
						       event->max_connections);
			break;
		case FMNA_CONFIG_EVENT_GET_MULTI_STATUS:
			multi_status_request_handle(event->conn);
			break;
		default:
			break;
		}

		return false;
	}

	return false;
}

APP_EVENT_LISTENER(fmna_conn, app_event_handler);
APP_EVENT_SUBSCRIBE_FINAL(fmna_conn, fmna_event);
APP_EVENT_SUBSCRIBE(fmna_conn, fmna_config_event);
