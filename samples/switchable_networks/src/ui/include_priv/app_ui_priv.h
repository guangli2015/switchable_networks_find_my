/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef APP_UI_PRIV_H_
#define APP_UI_PRIV_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Broadcast the UI module request to all registered listeners.
 *
 * This function should be called on the asynchronous events that happened,
 * for example, if there should be some action after a button press, this information
 * should be broadcasted to all the listeners as one of them might implement it.
 *
 * @param request UI module generated request to be handled by the application.
 */
void app_ui_request_broadcast(union app_ui_request request, uint32_t mode_bitmask);

/** Get the current UI mode.
 *
 * @return Current UI mode.
 */
enum app_ui_mode app_ui_mode_get(void);

struct app_ui_cb {
	/** Initialize the UI module.
	 *
	 *  Executed in the @ref app_ui_mode_set function to initialize the selected UI module.
	 *
	 * @return 0 if the operation was successful. Otherwise, a (negative) error code is
	 *	   returned.
	 */
	int (*init)(void);

	/** Indicate the current application state to the UI module.
	 *
	 *  Executed in the @ref app_ui_state_change_indicate function.
	 *
	 *  The UI module reacts to the current application state by properly driving the
	 *  dependent peripherals, such as LEDs or speakers.
	 *  This function should be implemented depending on the chosen platform and peripherals.
	 *
	 * @param state Current application state to be handled by the UI module
	 * @param active Marks if the application enters ( @ref true ) or leaves ( @ref false )
	 *               the current state.
	 */
	int (*change_indicate)(union app_ui_state state, bool active);

	/** Uninitialize the UI module.
	 *
	 *  Executed in the @ref app_ui_mode_set function to uninitialize the current UI module
	 *  before the new one is initialized.
	 *
	 * @return 0 if the operation was successful. Otherwise, a (negative) error code is
	 *	   returned.
	 */
	int (*uninit)(void);
};

/** Register the UI handlers for specific modes.
 *
 *  Handlers for all modes defined by the @ref app_ui_mode enum should be registered
 *  before the UI module is initialized using the @ref app_ui_init function.
 *
 *  All callbacks inside the @ref app_ui_cb structure must be implemented.
 *  The registered instance needs to persist in the memory after this function
 *  exits, as it is used directly without the copy operation. It is possible to
 *  register only one instance of callbacks per mode with this API.
 *
 *  @param mode_bm Bitmask of the modes for which the handlers should be registered.
 *  @param ui_cb UI handlers.
 */
void app_ui_register(uint32_t mode_bm, struct app_ui_cb *ui_cb);

#ifdef __cplusplus
}
#endif

#endif /* APP_UI_PRIV_H_ */
