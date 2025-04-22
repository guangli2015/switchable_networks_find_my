#ifndef FMNA_PAIR_EVENT_H_
#define FMNA_PAIR_EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "app_event_manager.h"
#include "fmna_gatt_pkt_manager.h"

enum fmna_pair_event_id {
	FMNA_PAIR_EVENT_INITIATE_PAIRING,
	FMNA_PAIR_EVENT_FINALIZE_PAIRING,
	FMNA_PAIR_EVENT_PAIRING_COMPLETE,
};

struct fmna_pair_buf {
	uint8_t data[FMNA_GATT_PKT_MAX_LEN];
	uint16_t len;
};

struct fmna_pair_event {
	struct app_event_header header;

	enum fmna_pair_event_id id;
	struct bt_conn *conn;
	struct fmna_pair_buf buf;
};

APP_EVENT_TYPE_DECLARE(fmna_pair_event);

#ifdef __cplusplus
}
#endif

#endif /* FMNA_PAIR_EVENT_H_ */
