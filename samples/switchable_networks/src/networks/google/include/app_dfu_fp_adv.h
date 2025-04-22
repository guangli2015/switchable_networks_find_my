/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef APP_DFU_FP_ADV_H_
#define APP_DFU_FP_ADV_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Manage the DFU advertising using the Fast Pair advertising set.
 *
 *  This function updates the Fast Pair advertising set with the advertising data
 *  related to the DFU.
 *  It injects the SMP UUID to the advertising data when the device is in the DFU mode
 *  using the Bluetooth LE advertising provider module.
 *
 *  It can be used as the adv_manage callback in the struct app_dfu_cb.
 */
void app_dfu_fp_adv_manage(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* APP_DFU_FP_ADV_H_ */
