/*
 * Copyright (c) 2025 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/kernel.h>

#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>

#include "app_network_selector.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(app_network_selector, LOG_LEVEL_INF);

#define SETTINGS_NETWORK_SELECTOR_SUBTREE_NAME		"network_selector"
#define SETTINGS_NETWORK_SELECTOR_KEY_NAME		"network"
#define SETTINGS_NAME_SEPARATOR_STR			"/"
#define SETTINGS_NETWORK_SELECTOR_FULL_NAME		\
	(SETTINGS_NETWORK_SELECTOR_SUBTREE_NAME		\
	 SETTINGS_NAME_SEPARATOR_STR			\
	 SETTINGS_NETWORK_SELECTOR_KEY_NAME)

BUILD_ASSERT(SETTINGS_NAME_SEPARATOR == '/');

struct network_desc {
	/** Bits 0–7: current network selection.
	 *  Available values defined in the @ref app_network_selector enum.
	 */
	uint32_t id : 8;

	/** Bits 8–30: reserved bitfield. */
	uint32_t reserved : 23;

	/** Bits 31: flag used to indicate unfinished factory reset procedure. */
	uint32_t reset_in_progress : 1;
};

BUILD_ASSERT(APP_NETWORK_SELECTOR_COUNT <= UINT8_MAX);
BUILD_ASSERT(sizeof(struct network_desc) == sizeof(uint32_t));

static struct network_desc current_network;
static int settings_rc = -ENOENT;
static bool initialized;
static atomic_t settings_loaded = ATOMIC_INIT(false);

static const char *network_name_get(enum app_network_selector network)
{
	switch (network) {
	case APP_NETWORK_SELECTOR_APPLE:
		return "Apple Find My";
	case APP_NETWORK_SELECTOR_GOOGLE:
		return "Google Find My Device";
	case APP_NETWORK_SELECTOR_UNSELECTED:
		return "Unselected";
	default:
		return "Unknown";
	}
}

static int network_settings_load(size_t len, settings_read_cb read_cb, void *cb_arg)
{
	int rc;
	LOG_INF("network_settings_load###");
	if (len != sizeof(current_network)) {
		return -EINVAL;
	}

	rc = read_cb(cb_arg, &current_network, sizeof(current_network));
	if (rc < 0) {
		return rc;
	}

	if (rc != sizeof(current_network)) {
		return -EINVAL;
	}

	return 0;
}

static int network_selector_settings_set(const char *name,
				size_t len,
				settings_read_cb read_cb,
				void *cb_arg)
{
	int err;
	LOG_INF("network_selector_settings_set###");
	LOG_INF("Network Selector: the 'network' node is being set by Settings %s",name);

	if (!strncmp(name,
		     SETTINGS_NETWORK_SELECTOR_KEY_NAME,
		     sizeof(SETTINGS_NETWORK_SELECTOR_KEY_NAME))) {
		err = network_settings_load(len, read_cb, cb_arg);
	} else {
		err = -ENOENT;
	}

	/* The error will be returned when calling app_network_selector_init.
	 * In case of -ENOENT, the module will initialize to APP_NETWORK_SELECTOR_UNSELECTED
	 * on init.
	 */
	
	settings_rc = err;
	LOG_INF("network_selector_settings_set settings_rc %d###",settings_rc);
	return 0;
}

static int network_selector_settings_commit(void)
{
	atomic_set(&settings_loaded, true);

	return 0;
}

static int network_selector_set(enum app_network_selector network, bool reset_in_progress)
{
	int err;
	struct network_desc desc = {0};
	LOG_INF("network_selector_set###");
	desc.id = network;
	desc.reset_in_progress = reset_in_progress;

	err = settings_save_one(SETTINGS_NETWORK_SELECTOR_FULL_NAME,
				&desc,
				sizeof(desc));
	if (err) {
		LOG_ERR("Network Selector: Settings saving failed (err %d)", err);
		return err;
	}

	current_network = desc;

	return 0;
}

static int reset_in_progress_set(bool in_progress)
{
	enum app_network_selector network = in_progress ?
		current_network.id : APP_NETWORK_SELECTOR_UNSELECTED;
	LOG_INF("reset_in_progress_set###");
	return network_selector_set(network, in_progress);
}

struct app_network_selector_desc *network_find(enum app_network_selector network)
{
	STRUCT_SECTION_FOREACH(app_network_selector_desc, desc) {
		LOG_INF("network_find %s###",network_name_get(desc->network));
		if (desc->network == network) {
			return desc;
		}
	}

	return NULL;
}

 void factory_reset_reboot(void)
{
	LOG_INF("Network Selector: Factory reset finalized, "
		"rebooting to the network selector...");
	LOG_INF("factory_reset_reboot###");
	sys_reboot(SYS_REBOOT_COLD);

	/* Should not reach. */
	k_panic();
}

static int factory_reset_run(void)
{
	int err;
	enum app_network_selector network = current_network.id;
	struct app_network_selector_desc *desc = network_find(network);
	LOG_INF("factory_reset_run###");
	__ASSERT_NO_MSG(desc);

	err = reset_in_progress_set(true);
	if (err) {
		LOG_ERR("Network Selector: Factory reset failed (err %d)", err);
		return err;
	}

	err = desc->factory_reset();
	if (err) {
		LOG_ERR("Network Selector: Factory reset failed (err %d)", err);
		return err;
	}

	err = reset_in_progress_set(false);
	if (err) {
		LOG_ERR("Network Selector: Factory reset failed (err %d)", err);
		return err;
	}

	factory_reset_reboot();

	return 0;
}
extern  int factory_reset_perform_apple(void);
int app_network_selector_set(enum app_network_selector network)
{
// add by andrew
#if 1
	__ASSERT_NO_MSG(initialized);
	LOG_INF("app_network_selector_set###");
	if (network >= APP_NETWORK_SELECTOR_COUNT) {
		LOG_ERR("Network Selector: Invalid network value");
		return -EINVAL;
	}

	if (current_network.id != APP_NETWORK_SELECTOR_UNSELECTED) {
		if (network == APP_NETWORK_SELECTOR_UNSELECTED) {
			LOG_INF("Network Selector: Unselecting the network, "
				"performing factory reset");
			return factory_reset_run();
		}

		LOG_ERR("Network Selector: Network already set, "
			"go through factory reset first");
		return -EACCES;
	}

	return network_selector_set(network, false);
#endif
//factory_reset_perform_apple();
//factory_reset_reboot();
return 0;
}
extern void app_network_apple_run(void);
extern void app_network_google_run(void);
void app_network_selector_launch(void)
{
// add by andrew
#if 0
	__ASSERT_NO_MSG(initialized);
	LOG_INF("app_network_selector_launch###");
	const struct app_network_selector_desc *desc = network_find(current_network.id);

	__ASSERT_NO_MSG(desc);

	LOG_INF("Network Selector: Running \"%s\" network", network_name_get(desc->network));

	desc->launch();
#endif
#if 1
	if(APP_NETWORK_SELECTOR_UNSELECTED == current_network.id)
	{
		app_network_apple_run();
		app_network_google_run();
	}
	else if(APP_NETWORK_SELECTOR_APPLE == current_network.id)
	{
		app_network_apple_run();
	}
	else if(APP_NETWORK_SELECTOR_GOOGLE == current_network.id)
	{
		app_network_google_run();
	}
#endif
}

int app_network_selector_init(void)
{
	int err;
// add by andrew
	
	__ASSERT_NO_MSG(!initialized);
	__ASSERT_NO_MSG(atomic_get(&settings_loaded));
	LOG_INF("app_network_selector_init###");
	if ((settings_rc != 0) && (settings_rc != -ENOENT)) {
		LOG_ERR("Network Selector: Settings loading failed (err %d)", settings_rc);
		return settings_rc;
	}

	if (settings_rc == -ENOENT) {
		LOG_WRN("Network Selector: Configuration not found, initializing to UNSELECTED");
		err = network_selector_set(APP_NETWORK_SELECTOR_UNSELECTED, false);
		if (err) {
			LOG_ERR("Network Selector: Set failed (err %d)",
				  err);
			return err;
		}

		initialized = true;

		return 0;
	}
#if 1
	if (current_network.reset_in_progress) {
		LOG_WRN("Network Selector: Factory reset has been interrupted, retrying");
		err = factory_reset_run();
		if (err) {
			LOG_ERR("Network Selector: Factory reset failed (err %d)", err);
			return err;
		}
	}
#endif
	initialized = true;

	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(app_network_selector,
			       SETTINGS_NETWORK_SELECTOR_SUBTREE_NAME,
			       NULL,
			       network_selector_settings_set,
			       network_selector_settings_commit,
			       NULL);
