/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef APP_UI_SELECTED_H_
#define APP_UI_SELECTED_H_

#ifdef __cplusplus
extern "C" {
#endif

/** Selected mode application states passed to the UI module. */
enum app_ui_selected_state {
	/** Indicates that the application is running correctly. */
	APP_UI_SELECTED_STATE_APP_RUNNING,

	/** Indicates that the device is ringing. */
	APP_UI_SELECTED_STATE_RINGING,

	/** Indicated the identification mode, which depending on network corresponds to:
	 *  - Apple: Serial Number lookup
	 *  - Google: DULT identification mode
	 */
	APP_UI_SELECTED_STATE_ID_MODE,

	/** Indicates that the device is provisioned. */
	APP_UI_SELECTED_STATE_PROVISIONED,

	/** Indicates that the device is advertising, depending on network:
	 *  - Apple: Find My pairing advertising
	 *  - Google: Fast Pair advertising
	 */
	APP_UI_SELECTED_STATE_ADVERTISING,

	/** Indicates that the DFU mode is active. */
	APP_UI_SELECTED_STATE_DFU_MODE,

	/** Number of selected mode states. */
	APP_UI_SELECTED_STATE_COUNT,
};

/** Selected mode UI requests to be handled by the application. */
enum app_ui_selected_request {
	/** Indicates that ringing should be stopped. */
	APP_UI_SELECTED_REQUEST_RINGING_STOP,

	/** Indicates that the identification mode should be entered.
	 *  Depending on the network it corresponds to:
	 *  - Apple: Serial Number lookup
	 *  - Google: DULT identification mode
	 */
	APP_UI_SELECTED_REQUEST_ID_MODE_ENTER,

	/** Indicates that the advertising mode should be changed.
	 *  Depending on the network it corresponds to:
	 *  - Apple: Find My pairing advertising
	 *  - Google: Fast Pair advertising
	 */
	APP_UI_SELECTED_REQUEST_ADVERTISING_MODE_CHANGE,

	/** Indicates that the factory reset should be performed. */
	APP_UI_SELECTED_REQUEST_FACTORY_RESET,

	/** Indicates that the DFU mode should be entered. */
	APP_UI_SELECTED_REQUEST_DFU_MODE_ENTER,

	/** Number of selected mode requests. */
	APP_UI_SELECTED_REQUEST_COUNT,
};

#ifdef __cplusplus
}
#endif

#endif /* APP_UI_SELECTED_H_ */
