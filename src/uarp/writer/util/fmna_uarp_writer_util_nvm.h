/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef FMNA_UARP_WRITER_UTIL_NVM_H_
#define FMNA_UARP_WRITER_UTIL_NVM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>

/**
 * @defgroup fmna_uarp_writer_util_nvm NVM management utility used by defined FMNA UARP payload writers
 * @brief NVM management utility used by defined FMNA UARP payload writers
 *
 * @{
 */

struct fmna_uarp_writer_util_nvm_ctx {
	const struct flash_area *fap;
};

int fmna_uarp_writer_util_nvm_start(struct fmna_uarp_writer_util_nvm_ctx *ctx,
				    uint8_t fa_id,
				    uint8_t *buf,
				    size_t buf_len,
				    size_t payload_size);
int fmna_uarp_writer_util_nvm_write(struct fmna_uarp_writer_util_nvm_ctx *ctx,
				    const uint8_t *chunk,
				    size_t chunk_size);
int fmna_uarp_writer_util_nvm_finish(struct fmna_uarp_writer_util_nvm_ctx *ctx, bool success);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* FMNA_UARP_WRITER_UTIL_NVM_H_ */
