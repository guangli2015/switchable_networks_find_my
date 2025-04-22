/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef FMNA_UARP_PAYLOAD_H_
#define FMNA_UARP_PAYLOAD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>

#include "fmna_uarp_writer.h"

/**
 * @defgroup fmna_uarp_payload FMNA UARP payload API
 * @brief FMNA UARP payload API
 *
 * @{
 */

/** UARP payload identifier name, 4 characters + '\0'. */
#define FMNA_UARP_PAYLOAD_4CC_SIZE	(4 + 1)

/** @brief Selected UARP payload header data received from the SuperBinary structure
 *         @ref struct UARPPayloadHeader.
 */
struct fmna_uarp_payload_header {
	/** UARP payload identifier name. */
	char tag_4cc[FMNA_UARP_PAYLOAD_4CC_SIZE];

	/** UARP payload version. */
	struct {
		uint32_t major;
		uint32_t minor;
		uint32_t release;
		uint32_t build;
	} version;
};

/** @brief Callback structure for managing the UARP payload. */
struct fmna_uarp_payload_cb {
	/**
	 * @brief User-specific UARP payload accept function.
	 *
	 * The device can accept or deny processing the described UARP payload based on the
	 * additional user-specific run-time logic.
	 * This callback is mandatory to implement.
	 *
	 * @param curr_header Current UARP payload header data.
	 *
	 * @return 0 on success, otherwise negative error code.
	 */
	bool (*accept)(const struct fmna_uarp_payload_header *curr_header);
};

/** @brief UARP payload descriptor structure. */
struct fmna_uarp_payload {
	/** Payload identifier. */
	char tag_4cc[FMNA_UARP_PAYLOAD_4CC_SIZE];

	/** Writer context used to store the UARP payload in the memory. */
	const struct fmna_uarp_writer *writer;

	/** Payload management callbacks. */
	const struct fmna_uarp_payload_cb *callbacks;
};

/** @brief Register UARP payload which will be processed during the UARP procedure.
 *
 * UARP payload are processed sequentially, thus UARP payload writers can
 * be safely used for multiple payloads and it's internal state won't be
 * overwritten between starting and ending processing the specific payload.
 *
 * @param _name FMNA UARP payload structure name.
 * @param _tag_4cc FMNA UARP payload 4CC identifier.
 * @param _writer Pointer to the FMNA UARP payload writer structure instance.
 * @param _callbacks Pointer to the user-specific FMNA UARP payload callbacks structure instance.
 */
#define FMNA_UARP_PAYLOAD_REGISTER(_name, _tag_4cc, _writer, _callbacks)	\
	BUILD_ASSERT(sizeof(_tag_4cc) == FMNA_UARP_PAYLOAD_4CC_SIZE);		\
	BUILD_ASSERT(_writer != NULL);						\
	BUILD_ASSERT(_callbacks != NULL);					\
	static const STRUCT_SECTION_ITERABLE(fmna_uarp_payload, _name) = {	\
		.tag_4cc = _tag_4cc,						\
		.writer = _writer,						\
		.callbacks = _callbacks,					\
	}

/** @brief Find UARP payload descriptor using the 4CC tag payload identifier.
 *
 *  @param tag_4cc UARP payload 4CC tag.
 *  @param payload UARP payload descriptor.
 *
 *  @return Pointer to the UARP payload descriptor if found, otherwise NULL.
 */
const struct fmna_uarp_payload *fmna_uarp_payload_find(
	const char tag_4cc[FMNA_UARP_PAYLOAD_4CC_SIZE]);

/** @brief Iterate over all registered UARP payloads.
 *
 * @param cb Callback function to be called for each UARP payload.
 * @param user_data User data to be passed to the callback function.
 *
 * @return 0 on success, otherwise negative error code.
 */
int fmna_uarp_payload_foreach(int (*cb)(const struct fmna_uarp_payload *payload, void *user_data),
			       void *user_data);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* FMNA_UARP_PAYLOAD_H_ */
