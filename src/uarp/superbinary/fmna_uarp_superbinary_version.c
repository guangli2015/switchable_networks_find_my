/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "fmna_version.h"

#include <zephyr/dfu/mcuboot.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

#include <pm_config.h>

#define IMAGE0_ID PM_MCUBOOT_PRIMARY_ID

int fmna_version_fw_get(struct fmna_version *ver)
{
	int err;
	struct mcuboot_img_header header;

	if (!ver) {
		return -EINVAL;
	}

	/* Assume that SuperBinary version is equal to the FW version of the running image. */
	err = boot_read_bank_header(IMAGE0_ID, &header, sizeof(header));
	if (err) {
		LOG_ERR("fmna_version: boot_read_bank_header returned error: %d", err);
		memset(ver, 0, sizeof(*ver));
		return err;
	}

	ver->major = header.h.v1.sem_ver.major;
	ver->minor = header.h.v1.sem_ver.minor;
	ver->revision = header.h.v1.sem_ver.revision;
	ver->build_num = header.h.v1.sem_ver.build_num;

	return 0;
}
