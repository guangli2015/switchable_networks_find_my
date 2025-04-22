/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef CRYPTO_LOG_H_
#define CRYPTO_LOG_H_

#include <zephyr/logging/log.h>

#if CONFIG_FMN_CRYPTO_DBG_ENABLED
#define LOG_LEVEL LOG_LEVEL_DBG
#else
#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#endif

LOG_MODULE_REGISTER(LOG_MODULE_NAME, LOG_LEVEL);

#endif /* CRYPTO_LOG_H_ */
