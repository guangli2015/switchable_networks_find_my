/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef APP_DFU_H_
#define APP_DFU_H_

#include <stdint.h>
#include <stdbool.h>

#include <zephyr/bluetooth/uuid.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Check if the GATT operations on the DFU GATT service are allowed.
 *
 *  When the DFU Kconfig is enabled, the sample statically registers the proprietary
 *  DFU GATT service and characteristics (SMP).
 *  Access to this service is allowed only when the device is in DFU mode.
 *
 *  The DFU mode timeout will be refreshed each time the device receives an
 *  authorized DFU GATT service operation.
 *
 *  @param uuid GATT characteristic UUID
 *  @return true if allowed, otherwise false
 */
bool app_dfu_bt_gatt_operation_allow(const struct bt_uuid *uuid);

/** Set the Bluetooth identity used by the DFU module.
 *
 *  This function shall only be called before the DFU module is initialized using the
 *  @ref app_dfu_bt_adv_init function.
 *
 *  @param bt_id Bluetooth identity.
 *
 *  @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int app_dfu_bt_id_set(uint8_t bt_id);

/** Enter the DFU mode.
 *
 *  The DFU mode can be entered in the persistent mode. In this mode, the device will
 *  remain in the DFU mode until the @ref app_dfu_mode_exit function is called.
 *  Otherwise, the device will exit the DFU mode after the timeout (5 minutes) expires.
 *  The timeout will be refreshed each time the device receives an authorized DFU GATT
 *  service operation or the @ref app_dfu_mode_enter function is called.
 *
 *  @param persistent_mode If true, the DFU mode will be entered in the persistent mode.
 */
void app_dfu_mode_enter(bool persistent_mode);

/** Exit the DFU mode.
 *
 *  The device will exit the DFU mode.
 */
void app_dfu_mode_exit(void);

/** Check if the currently booted image is confirmed.
 *
 *  @return true if the image is confirmed, otherwise false.
 */
bool app_dfu_is_confirmed(void);

/** Callbacks for the DFU module. */
struct app_dfu_cb {
	/** Notify the user that the DFU state has changed.
	 *
	 *  This function can be used to trigger the UI notification when the DFU mode
	 *  is being enabled or disabled.
	 *
	 *  @param enabled If true, the DFU mode is enabled. Otherwise, it is disabled.
	 */
	void (*state_changed)(bool enabled);

	/** Notify the user that the currently booted image has been confirmed. */
	void (*image_confirmed)(void);
};

/** Register the DFU callbacks.
 *
 *  The registered instance needs to persist in the memory after this function
 *  exits, as it is used directly without the copy operation. It is possible to
 *  register only one instance of callbacks with this API.
 *
 *  @param cbs Callbacks for the DFU module
 *
 *  @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int app_dfu_cb_register(const struct app_dfu_cb *cb);

/** Initialize the DFU module.
 *
 *  @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int app_dfu_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_DFU_H_ */
