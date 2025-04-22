/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef FMNA_ADV_H_
#define FMNA_ADV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>

#include "fmna_keys.h"

struct fmna_adv_nearby_config {
	bool fast_mode;
	bool is_maintained;
	uint8_t primary_key[FMNA_PUBLIC_KEY_LEN];
};

struct fmna_adv_separated_config {
	bool fast_mode;
	bool is_maintained;
	uint8_t primary_key[FMNA_PUBLIC_KEY_LEN];
	uint8_t separated_key[FMNA_PUBLIC_KEY_LEN];
};

int fmna_adv_stop(void);

int fmna_adv_start_unpaired(bool change_address);

int fmna_adv_start_nearby(const struct fmna_adv_nearby_config *config);

int fmna_adv_start_separated(const struct fmna_adv_separated_config *config);

int fmna_adv_init(uint8_t id);

int fmna_adv_uninit(void);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_ADV_H_ */
