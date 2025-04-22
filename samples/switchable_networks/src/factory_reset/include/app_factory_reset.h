/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef APP_FACTORY_RESET_H_
#define APP_FACTORY_RESET_H_

#include <stdbool.h>

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Schedule the factory reset action.
 *
 * @param delay Time to wait before the factory reset action is performed.
 */
void app_factory_reset_schedule(k_timeout_t delay);

/** Cancel the scheduled factory reset action. */
void app_factory_reset_cancel(void);

/** Initialize the factory reset module.
 *
 * @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int app_factory_reset_init(bool requested);

#ifdef __cplusplus
}
#endif

#endif /* APP_FACTORY_RESET_H_ */
