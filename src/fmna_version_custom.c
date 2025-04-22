/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "fmna_version.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

int fmna_version_fw_get(struct fmna_version *ver)
{
	if (!ver) {
		return -EINVAL;
	}

	ver->major = CONFIG_FMNA_FIRMWARE_VERSION_MAJOR;
	ver->minor = CONFIG_FMNA_FIRMWARE_VERSION_MINOR;
	ver->revision = CONFIG_FMNA_FIRMWARE_VERSION_REVISION;
	ver->build_num = 0;

	return 0;
}
