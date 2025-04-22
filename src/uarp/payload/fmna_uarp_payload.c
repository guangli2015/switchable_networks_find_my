/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(fmna_uarp_payload, CONFIG_FMNA_UARP_PAYLOAD_LOG_LEVEL);

#include "fmna_uarp_payload.h"

const struct fmna_uarp_payload *fmna_uarp_payload_find(
	const char tag_4cc[FMNA_UARP_PAYLOAD_4CC_SIZE])
{
	STRUCT_SECTION_FOREACH(fmna_uarp_payload, payload) {
		if (memcmp(payload->tag_4cc, tag_4cc, sizeof(payload->tag_4cc)) == 0) {
			return payload;
		}
	}

	return NULL;
}

int fmna_uarp_payload_foreach(int (*cb)(const struct fmna_uarp_payload *payload, void *user_data),
			      void *user_data)
{
	int err;

	if (!cb) {
		return -EINVAL;
	}

	STRUCT_SECTION_FOREACH(fmna_uarp_payload, payload) {
		err = cb(payload, user_data);
		if (err) {
			return err;
		}
	}

	return 0;
}
