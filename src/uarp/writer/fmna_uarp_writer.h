/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef FMNA_UARP_WRITER_H_
#define FMNA_UARP_WRITER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>

/**
 * @defgroup fmna_uarp_writer FMNA UARP payload writer API
 * @brief FMNA UARP payload writer API
 *
 * @{
 */

/** Helper macro to define the writer API structure.
 *
 * @param _name FMNA UARP payload writer API structure name.
 * @param _transfer_start FMNA UARP payload writer API @ref transfer_start function implementation.
 * @param _transfer_write FMNA UARP payload writer API @ref transfer_write function implementation.
 * @param _transfer_finish FMNA UARP payload writer API @ref transfer_finish function
 * 			   implementation.
 * @param _image_confirm FMNA UARP payload writer API @ref image_confirm function implementation.
 */
#define FMNA_UARP_WRITER_API_DEF(_name, \
				 _transfer_start, 	\
				 _transfer_write, 	\
				 _transfer_finish, 	\
				 _image_confirm)	\
	BUILD_ASSERT(_transfer_start != NULL);		\
	BUILD_ASSERT(_transfer_write != NULL);		\
	BUILD_ASSERT(_transfer_finish != NULL);		\
	BUILD_ASSERT(_image_confirm != NULL);		\
	const struct fmna_uarp_writer_api _name = {	\
		.transfer_start = _transfer_start,	\
		.transfer_write = _transfer_write,	\
		.transfer_finish = _transfer_finish,	\
		.image_confirm = _image_confirm,	\
	}

/** Helper macro to define the writer structure.
 *
 * The @ref _api_name must match the name of the previously defined
 * writer API using the @ref FMNA_UARP_WRITER_API_DEF macro.
 *
 * This macro can be used to define other writer-specific define macro.
 *
 * @param _name FMNA UARP payload writer structure name.
 * @param _api_name FMNA UARP payload writer API structure name.
 * @param _ctx Pointer to the FMNA UARP payload writer-specific context.
 */
#define FMNA_UARP_WRITER_DEF(_name, _api_name, _ctx)		\
	extern const struct fmna_uarp_writer_api _api_name;	\
	static const struct fmna_uarp_writer _name = {		\
		.api = &_api_name,				\
		.ctx = _ctx,					\
	}

/** @brief UARP payload writer API structure. */
struct fmna_uarp_writer_api {
	/**
	 * @brief Prepare the writer before writing the first byte of the UARP payload.
	 *
	 * @param ctx Payload writer-specific context.
	 * @param payload_size Payload size.
	 *
	 * @return 0 on success, otherwise negative error code.
	 */
	int (*transfer_start)(void *ctx, size_t payload_size);

	/**
	 * @brief Write a subsequent chunk of the UARP payload.
	 *
	 * @param ctx Payload writer-specific context.
	 * @param chunk Pointer to the chunk's data.
	 * @param chunk_size Size of the chunk.
	 *
	 * @return 0 on success, otherwise negative error code.
	 */
	int (*transfer_write)(void *ctx, const uint8_t *chunk, size_t chunk_size);

	/**
	 * @brief Complete processing the UARP payload.
	 *
	 * If UARP payload processing failed, the writer should invalidate
	 * or remove the payload from the device.
	 *
	 * @param ctx Payload writer-specific context.
	 * @param success Indicates that the UARP payload has been successfully processed.
	 *
	 * @return 0 on success, otherwise negative error code.
	 */
	int (*transfer_finish)(void *ctx, bool success);

	/**
	 * @brief Confirm the UARP payload.
	 *
	 * @param ctx Payload writer-specific context.
	 *
	 * @return 0 on success, otherwise negative error code.
	 */
	int (*image_confirm)(void *ctx);
};

/** @brief UARP payload writer structure. */
struct fmna_uarp_writer {
	/** UARP payload writer API structure. */
	const struct fmna_uarp_writer_api *api;

	/** UARP payload writer-specific context. */
	void * const ctx;
};

/** Prepare the writer before writing the first byte of the UARP payload.
 *
 * @param writer UARP payload writer structure.
 * @param payload_size Payload size.
 *
 * @return 0 on success, otherwise negative error code.
 */
static inline int fmna_uarp_writer_transfer_start(const struct fmna_uarp_writer *writer,
						  size_t payload_size)
{
	const struct fmna_uarp_writer_api *api = writer->api;

	return api->transfer_start(writer->ctx, payload_size);
}

/** Write a subsequent chunk of the UARP payload.
 *
 * @param writer UARP payload writer structure.
 * @param chunk Pointer to the chunk's data.
 * @param chunk_size Size of the chunk.
 *
 * @return 0 on success, otherwise negative error code.
 */
static inline int fmna_uarp_writer_transfer_write(const struct fmna_uarp_writer *writer,
						  const uint8_t *chunk,
						  size_t chunk_size)
{
	const struct fmna_uarp_writer_api *api = writer->api;

	return api->transfer_write(writer->ctx, chunk, chunk_size);
}

/** Complete processing the UARP payload.
 *
 * If UARP payload processing failed, the writer should invalidate
 * or remove the payload from the device.
 *
 * @param writer UARP payload writer structure.
 * @param success Indicates that the UARP payload has been successfully processed.
 *
 * @return 0 on success, otherwise negative error code.
 */
static inline int fmna_uarp_writer_transfer_finish(const struct fmna_uarp_writer *writer,
						   bool success)
{
	const struct fmna_uarp_writer_api *api = writer->api;

	return api->transfer_finish(writer->ctx, success);
}

/** Confirm the UARP payload.
 *
 * @param writer UARP payload writer structure.
 *
 * @return 0 on success, otherwise negative error code.
 */
static inline int fmna_uarp_writer_image_confirm(const struct fmna_uarp_writer *writer)
{
	const struct fmna_uarp_writer_api *api = writer->api;

	return api->image_confirm(writer->ctx);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* FMNA_UARP_WRITER_H_ */
