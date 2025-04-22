/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef APP_UI_UNSELECTED_H_
#define APP_UI_UNSELECTED_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Unselected mode application states passed to the UI module. */
enum app_ui_unselected_state {
	/** Indicates that the network selection menu is active. */
	APP_UI_UNSELECTED_STATE_SELECTION_MENU,

	/** Number of unselected mode states. */
	APP_UI_UNSELECTED_STATE_COUNT,
};

/** Unselected mode UI requests to be handled by the application. */
enum app_ui_unselected_request {
	/** Indicates that the Apple Find My network should be selected. */
	APP_UI_UNSELECTED_REQUEST_NETWORK_APPLE,

	/** Indicates that the Google Find My Device network should be selected. */
	APP_UI_UNSELECTED_REQUEST_NETWORK_GOOGLE,

	/** Number of unselected mode requests. */
	APP_UI_UNSELECTED_REQUEST_COUNT,
};

/** Present available networks to be selected.
 *
 *  In the unselected state, there is a selection menu that allows the user to choose
 *  the locator tag network to launch. This function presents the available networks
 *  to the user by logging the selection method and the name of the network.
 *
 * @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
void app_ui_unselected_network_choice_present(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_UI_UNSELECTED_H_ */
