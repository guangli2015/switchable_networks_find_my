/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef APP_NETWORK_SELECTOR_H_
#define APP_NETWORK_SELECTOR_H_

#include <stdint.h>

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Available networks to be selected.
 *  The value of the each enum should not be changed to keep the backwards compatibility,
 *  as it is saved in the settings.
 *  The new networks can be added at the end of the list before @ref APP_NETWORK_SELECTOR_COUNT.
 *  Max number of networks is limited to 255.
 */
enum app_network_selector {
	/** Selector for the unselected network. */
	APP_NETWORK_SELECTOR_UNSELECTED,

	/** Selector for the Apple Find My network. */
	APP_NETWORK_SELECTOR_APPLE,

	/** Selector for the Google Find My Device network. */
	APP_NETWORK_SELECTOR_GOOGLE,

	/* New networks can be added here. */

	/** Number of available networks. */
	APP_NETWORK_SELECTOR_COUNT,
};

/** Network descriptor structure. */
struct app_network_selector_desc {
	/** Described network. See the @ref app_network_selector identifiers for more details. */
	enum app_network_selector network;

	/** Callback used to launch the selected network. */
	void (*launch)(void);

	/** Callback used to perform the factory reset action.
	 *
	 *  @return 0 on success, negative error code on failure.
	 */
	int (*factory_reset)(void);
};

/** Register the network descriptor.
 *
 *  @param _name The name of the network descriptor.
 *  @param _network The network to register, see @ref app_network_selector.
 *  @param _launch The launch callback.
 *  @param _factory_reset The factory reset callback.
 */
#define APP_NETWORK_SELECTOR_DESC_REGISTER(_name, _network, _launch, _factory_reset)	\
	BUILD_ASSERT(_network < APP_NETWORK_SELECTOR_COUNT);				\
	BUILD_ASSERT(_launch != NULL);							\
	BUILD_ASSERT(_factory_reset != NULL);						\
	static const STRUCT_SECTION_ITERABLE(app_network_selector_desc, _name) = {	\
		.network = _network,							\
		.launch = _launch,							\
		.factory_reset = _factory_reset,					\
	}

/** Set the current network.
 *
 *  If the network is currently set to @ref APP_NETWORK_SELECTOR_UNSELECTED, this function
 *  sets the new network value and saves it to the settings.
 *
 *  If the network is already set to any different value, the only permitted value
 *  is @ref APP_NETWORK_SELECTOR_UNSELECTED, which performs the factory reset action.
 *  It uses the factory_reset callback of the current network descriptor to
 *  perform the factory reset action. After the factory reset is completed without
 *  errors, the @ref APP_NETWORK_SELECTOR_UNSELECTED network is set as current network,
 *  it is saved to settings and the device is rebooted. If the factory reset fails, it
 *  is up to the user to handle the error.
 *
 *  @param[in] network The network to set.
 *
 *  @return 0 on success, negative error code on failure.
 */
int app_network_selector_set(enum app_network_selector network);

/** Launch the selected network.
 *
 *  This function launches the selected network.
 *  It uses the launch callback of the current network descriptor.
 */
void app_network_selector_launch(void);

/** Initialize the network selector module.
 *
 *  This function shall only be used after calling the @ref settings_load function.
 *
 *  @return 0 on success, negative error code on failure.
 */
int app_network_selector_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_NETWORK_SELECTOR_H_ */
