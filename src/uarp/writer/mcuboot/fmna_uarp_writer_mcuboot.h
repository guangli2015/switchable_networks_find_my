/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef FMNA_UARP_WRITER_MCUBOOT_H_
#define FMNA_UARP_WRITER_MCUBOOT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>

#include "fmna_uarp_writer.h"

/**
 * @defgroup fmna_uarp_writer_mcuboot MCUboot-specific FMNA UARP payload writer API
 * @brief MCUboot-specific FMNA UARP payload writer API
 *
 * @{
 */

/** Define the MCUboot-specific FMNA UARP payload writer instance.
 *
 *  @param _name FMNA UARP payload writer structure name.
 *  @param _write_fa_id Write flash area ID of the image partition.
 *         Used for processing the new image.
 *  @param _running_fa_id Flash area ID of the image partition from which
 *         the device is currently running.
 *         Used for confirmation verification before accepting the new image.
 *         Used for confirming currently running image.
 */
#define FMNA_UARP_WRITER_MCUBOOT_DEF(_name, _write_fa_id, _running_fa_id)       \
	FMNA_UARP_WRITER_DEF(_name,                                             \
			     fmna_uarp_writer_mcuboot_api,                      \
			     (&(struct fmna_uarp_writer_mcuboot_ctx){           \
				.write_fa_id = (_write_fa_id),                  \
				.running_fa_id = (_running_fa_id),              \
				}))

/** MCUboot-specific FMNA UARP payload writer configuration data structure. */
struct fmna_uarp_writer_mcuboot_ctx {
	/** Write flash partition ID. */
	uint8_t write_fa_id;

	/** Currently running flash partition ID. */
	uint8_t running_fa_id;
};

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* FMNA_UARP_WRITER_MCUBOOT_H_ */
