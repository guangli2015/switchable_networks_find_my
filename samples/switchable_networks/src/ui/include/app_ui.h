/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef APP_UI_H_
#define APP_UI_H_

#include <stdint.h>

#include "app_ui_selected.h"
#include "app_ui_unselected.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Available UI modes.
 *
 *  Each mode corresponds to the available networks.
 *  Application can be in only one mode at a time.
 */
enum app_ui_mode {
	/** Unselected mode.
	 *
	 *  Used by the unselected network.
	 */
	APP_UI_MODE_UNSELECTED,

	/** Selected mode.
	 *
	 *  Used by the following networks:
	 *  - Apple Find My
	 *  - Google Find My Device
	 */
	APP_UI_MODE_SELECTED_APPLE,
	APP_UI_MODE_SELECTED_GOOGLE,

	/** Number of available UI modes. */
	APP_UI_MODE_COUNT,
};

/** Set the UI mode.
 *
 *  This function transitions from one mode to another.
 *  Changing the mode allows to handle different UI requests by activating proper listeners.
 *
 * @param mode Mode to be set.
 *
 * @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int app_ui_mode_set(enum app_ui_mode mode);

/** Application states passed to the UI module.
 *
 *  These states are independent of each other and the application can be
 *  in several of them at the same time.
 *
 *  Only the field corresponding to the current mode should be set.
 */
union app_ui_state {
	/** Valid in modes selected modes:
	 *  - @ref APP_UI_MODE_SELECTED_APPLE
	 *  - @ref APP_UI_MODE_SELECTED_GOOGLE
	 */
	enum app_ui_selected_state selected;

	/** Valid in mode @ref APP_UI_MODE_UNSELECTED */
	enum app_ui_unselected_state unselected;
};

/** Indicate the current application state to the UI module.
 *
 *  This function shall only be used after the @ref app_ui_mode_set function is called.
 *
 *  The UI module reacts to the current application state by properly driving the
 *  dependent peripherals, such as LEDs or speakers.
 *  This function should be implemented depending on the chosen platform and peripherals.
 *
 * @param state Current application state to be handled by the UI module
 * @param active Marks if the application enters ( @ref true ) or leaves ( @ref false )
 *               the current state.
 *
 * @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int app_ui_state_change_indicate(union app_ui_state state, bool active);

/** Available UI module requests to be handled by the application.
 *
 *  Only the field corresponding to the current mode should be set.
 */
union app_ui_request {
	/** Valid in modes selected modes:
	 *  - @ref APP_UI_MODE_SELECTED_APPLE
	 *  - @ref APP_UI_MODE_SELECTED_GOOGLE
	 */
	enum app_ui_selected_request selected;

	/** Valid in mode @ref APP_UI_MODE_UNSELECTED */
	enum app_ui_unselected_request unselected;
};

/** Listener for the UI module requests. */
struct app_ui_request_listener {
	/** Mode in which the handler is active. */
	enum app_ui_mode mode;

	/** UI module request handler.
	 *
	 * @param request UI module generated request to be handled by the application.
	 */
	void (*handler)(union app_ui_request request);
};

/** Register a listener for the UI module requests.
 *
 * @param _name UI module request listener name.
 * @param _mode Mode in which the handler is active.
 * @param _handler UI module request handler.
 */
#define APP_UI_REQUEST_LISTENER_REGISTER(_name, _mode, _handler)			\
	BUILD_ASSERT(_handler != NULL);							\
	static const STRUCT_SECTION_ITERABLE(app_ui_request_listener, _name) = {	\
		.mode = _mode,								\
		.handler = _handler,							\
	}

/** Initialize the UI module.
 *
 *  This function should be implemented depending on the chosen platform and peripherals.
 *
 * @return 0 if the operation was successful. Otherwise, a (negative) error code is returned.
 */
int app_ui_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_UI_H_ */
