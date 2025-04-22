/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(fmna_uarp_payload);

#include "fmna_uarp_payload.h"
#include "fmna_uarp_writer.h"
#include "fmna_uarp_writer_mcuboot.h"

#include <pm_config.h>

/** In swap mode, we are writing primary slot candidate image to the second slot.
 *  MCUboot bootloader will then move it to the primary slot.
 */
#define TARGET_RUNNING_FA_ID	PM_MCUBOOT_PRIMARY_ID
#define TARGET_WRITE_FA_ID	PM_MCUBOOT_SECONDARY_ID
#define TARGET_4CC_TAG		CONFIG_FMNA_UARP_PAYLOAD_MCUBOOT_APP_S0_4CC_TAG

static bool accept(const struct fmna_uarp_payload_header *curr_header)
{
	ARG_UNUSED(curr_header);

	LOG_INF("Accepting MCUboot primary slot image payload with tag: \"%s\","
		" version: %d.%d.%d+%d",
		curr_header->tag_4cc,
		curr_header->version.major,
		curr_header->version.minor,
		curr_header->version.release,
		curr_header->version.build);

	return true;
}

static const struct fmna_uarp_payload_cb cbs = {
	.accept = accept,
};

FMNA_UARP_WRITER_MCUBOOT_DEF(payload_app_mcuboot_writer, TARGET_WRITE_FA_ID, TARGET_RUNNING_FA_ID);
FMNA_UARP_PAYLOAD_REGISTER(payload_app_primary_slot,
			   TARGET_4CC_TAG,
			   &payload_app_mcuboot_writer,
			   &cbs);
