/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef APP_DFU_BT_ADV_H_
#define APP_DFU_BT_ADV_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Manage the DFU advertising using the separate advertising set.
 *
 *  This function enables or disables the DFU advertising. It handles the lifecycle of
 *  the advertising set and the advertising data.
 *
 *  It uses the Bluetooth identity set using the @ref app_dfu_bt_adv_id_set function.
 *  If the Bluetooth identity was not set explicitly, the BT_ID_DEFAULT identity is used.
 *
 *  It can be used as the adv_manage callback in the struct app_dfu_cb.
 */
void app_dfu_bt_adv_manage(bool enable);

/** Set the Bluetooth identity used by the DFU separate advertising module.
 *
 *  This function shall only be called before the DFU separate advertising module is initialized
 *  using the @ref app_dfu_bt_adv_init function.
 *
 *  @param bt_id Bluetooth identity.
 *
 *  @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int app_dfu_bt_adv_id_set(uint8_t bt_id);

/** Initialize the DFU separate advertising module.
 *
 *  @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int app_dfu_bt_adv_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_DFU_BT_ADV_H_ */
