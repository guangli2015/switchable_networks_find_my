/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef FMNA_CONN_H_
#define FMNA_CONN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/conn.h>

enum fmna_conn_multi_status_bit {
	FMNA_CONN_MULTI_STATUS_BIT_PERSISTENT_CONNECTION = 0,
	FMNA_CONN_MULTI_STATUS_BIT_PLAYING_SOUND         = 2,
	FMNA_CONN_MULTI_STATUS_BIT_UPDATING_FIRMWARE     = 3,
	FMNA_CONN_MULTI_STATUS_BIT_OWNER_CONNECTED       = 5,
	FMNA_CONN_MULTI_STATUS_BIT_MULTIPLE_OWNERS       = 6,
};

uint8_t fmna_conn_connection_num_get(void);

bool fmna_conn_limit_check(void);

bool fmna_conn_check(struct bt_conn *conn);

int fmna_conn_owner_find(struct bt_conn *owner_conns[], uint8_t *owner_conn_cnt);

bool fmna_conn_multi_status_bit_check(struct bt_conn *conn,
				      enum fmna_conn_multi_status_bit status_bit);

void fmna_conn_multi_status_bit_set(struct bt_conn *conn,
				    enum fmna_conn_multi_status_bit status_bit);

void fmna_conn_multi_status_bit_clear(struct bt_conn *conn,
				      enum fmna_conn_multi_status_bit status_bit);

int fmna_conn_init(uint8_t bt_id);

int fmna_conn_uninit(void);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_CONN_H_ */
