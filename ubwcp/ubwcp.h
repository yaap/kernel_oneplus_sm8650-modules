/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __UBWCP_H_
#define __UBWCP_H_

#include <linux/types.h>
#include <linux/dma-buf.h>

#include "include/uapi/ubwcp_ioctl.h"


typedef int (*configure_mmap)(struct dma_buf *dmabuf, bool linear, phys_addr_t ula_pa_addr,
			      size_t ula_pa_size);

/**
 * Get UBWCP hardware version
 *
 * @param ver : ptr to ver struct where hw version will be
 *            copied
 *
 * @return int : 0 on success, otherwise error code
 */
int ubwcp_get_hw_version(struct ubwcp_ioctl_hw_version *ver);

/**
 * Configures ubwcp buffer with the provided buffer image
 * attributes. This call must be done at least once before
 * ubwcp_lock(). Attributes can be configured multiple times,
 * but only during unlocked state.
 *
 * @param dmabuf : ptr to the dma buf
 * @param attr   : buffer attributes to set
 *
 * @return int : 0 on success, otherwise error code
 */
int ubwcp_set_buf_attrs(struct dma_buf *dmabuf, struct ubwcp_buffer_attrs *attr);

/**
 * Get the currently configured attributes for the buffer
 *
 * @param dmabuf : ptr to the dma buf
 * @param attr   : pointer to location where image attributes
 *		   for this buffer will be copied to.
 *
 * @return int : 0 on success, otherwise error code
 */
int ubwcp_get_buf_attrs(struct dma_buf *dmabuf, struct ubwcp_buffer_attrs *attr);

/**
 * Set permanent range translation for the buffer. This reserves
 * ubwcp address translation resources for the buffer until free
 * is called. This may speed up lock()/unlock() calls as they
 * don't need to configure address translations for the buffer.
 *
 * @param dmabuf : ptr to the dma buf
 * @param enable : true == enable, false == disable
 *
 * @return int : 0 on success, otherwise error code
 */
int ubwcp_set_perm_range_translation(struct dma_buf *dmabuf, bool enable);

#endif /* __UBWCP_H_ */
