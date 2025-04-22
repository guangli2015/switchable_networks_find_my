/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef APP_RING_H_
#define APP_RING_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the ringing module.
 *
 * @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int app_ring_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_RING_H_ */
