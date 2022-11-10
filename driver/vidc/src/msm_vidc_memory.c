// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/dma-mapping.h>

#include "msm_vidc_memory.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_driver.h"
#include "msm_vidc_core.h"
#include "msm_vidc_events.h"
#include "msm_vidc_platform.h"
#include "venus_hfi.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,16,0))
	MODULE_IMPORT_NS(DMA_BUF);
#endif

struct msm_vidc_type_size_name {
	enum msm_memory_pool_type type;
	u32                       size;
	char                     *name;
};

static const struct msm_vidc_type_size_name buftype_size_name_arr[] = {
	{MSM_MEM_POOL_BUFFER,     sizeof(struct msm_vidc_buffer),     "MSM_MEM_POOL_BUFFER"     },
	{MSM_MEM_POOL_MAP,        sizeof(struct msm_vidc_map),        "MSM_MEM_POOL_MAP"        },
	{MSM_MEM_POOL_ALLOC,      sizeof(struct msm_vidc_alloc),      "MSM_MEM_POOL_ALLOC"      },
	{MSM_MEM_POOL_TIMESTAMP,  sizeof(struct msm_vidc_timestamp),  "MSM_MEM_POOL_TIMESTAMP"  },
	{MSM_MEM_POOL_DMABUF,     sizeof(struct msm_memory_dmabuf),   "MSM_MEM_POOL_DMABUF"     },
	{MSM_MEM_POOL_PACKET,     sizeof(struct hfi_pending_packet) + MSM_MEM_POOL_PACKET_SIZE,
		"MSM_MEM_POOL_PACKET"},
	{MSM_MEM_POOL_BUF_TIMER,  sizeof(struct msm_vidc_input_timer), "MSM_MEM_POOL_BUF_TIMER" },
	{MSM_MEM_POOL_BUF_STATS,  sizeof(struct msm_vidc_buffer_stats), "MSM_MEM_POOL_BUF_STATS"},
};

int msm_vidc_vmem_alloc(unsigned long size, void **mem, const char *msg)
{
	int rc = 0;

	if (*mem) {
		d_vpr_e("%s: error: double alloc\n", msg);
		rc = -EINVAL;
	}

	*mem = vzalloc(size);
	if (!*mem) {
		d_vpr_e("allocation failed for %s\n", msg);
		rc = -ENOMEM;
	}

	return rc;
}

void msm_vidc_vmem_free(void **addr)
{
	if (addr && *addr) {
		vfree(*addr);
		*addr = NULL;
	}
}

void *msm_vidc_pool_alloc(struct msm_vidc_inst *inst, enum msm_memory_pool_type type)
{
	struct msm_memory_alloc_header *hdr = NULL;
	struct msm_memory_pool *pool;

	if (!inst || type < 0 || type >= MSM_MEM_POOL_MAX) {
		d_vpr_e("%s: Invalid params\n", __func__);
		return NULL;
	}
	pool = &inst->pool[type];

	if (!list_empty(&pool->free_pool)) {
		/* get 1st node from free pool */
		hdr = list_first_entry(&pool->free_pool,
			struct msm_memory_alloc_header, list);

		/* move node from free pool to busy pool */
		list_move_tail(&hdr->list, &pool->busy_pool);

		/* reset existing data */
		memset((char *)hdr->buf, 0, pool->size);

		/* set busy flag to true. This is to catch double free request */
		hdr->busy = true;

		return hdr->buf;
	}

	if (msm_vidc_vmem_alloc(pool->size + sizeof(struct msm_memory_alloc_header),
			(void **)&hdr, __func__))
		return NULL;

	INIT_LIST_HEAD(&hdr->list);
	hdr->type = type;
	hdr->busy = true;
	hdr->buf = (void *)(hdr + 1);
	list_add_tail(&hdr->list, &pool->busy_pool);

	return hdr->buf;
}

void msm_vidc_pool_free(struct msm_vidc_inst *inst, void *vidc_buf)
{
	struct msm_memory_alloc_header *hdr;
	struct msm_memory_pool *pool;

	if (!inst || !vidc_buf) {
		d_vpr_e("%s: Invalid params\n", __func__);
		return;
	}
	hdr = (struct msm_memory_alloc_header *)vidc_buf - 1;

	/* sanitize buffer addr */
	if (hdr->buf != vidc_buf) {
		i_vpr_e(inst, "%s: invalid buf addr %p\n", __func__, vidc_buf);
		return;
	}

	/* sanitize pool type */
	if (hdr->type < 0 || hdr->type >= MSM_MEM_POOL_MAX) {
		i_vpr_e(inst, "%s: invalid pool type %#x\n", __func__, hdr->type);
		return;
	}
	pool = &inst->pool[hdr->type];

	/* catch double-free request */
	if (!hdr->busy) {
		i_vpr_e(inst, "%s: double free request. type %s, addr %p\n", __func__,
			pool->name, vidc_buf);
		return;
	}
	hdr->busy = false;

	/* move node from busy pool to free pool */
	list_move_tail(&hdr->list, &pool->free_pool);
}

static void msm_vidc_destroy_pool_buffers(struct msm_vidc_inst *inst,
	enum msm_memory_pool_type type)
{
	struct msm_memory_alloc_header *hdr, *dummy;
	struct msm_memory_pool *pool;
	u32 fcount = 0, bcount = 0;

	if (!inst || type < 0 || type >= MSM_MEM_POOL_MAX) {
		d_vpr_e("%s: Invalid params\n", __func__);
		return;
	}
	pool = &inst->pool[type];

	/* detect memleak: busy pool is expected to be empty here */
	if (!list_empty(&pool->busy_pool))
		i_vpr_e(inst, "%s: destroy request on active buffer. type %s\n",
			__func__, pool->name);

	/* destroy all free buffers */
	list_for_each_entry_safe(hdr, dummy, &pool->free_pool, list) {
		list_del(&hdr->list);
		msm_vidc_vmem_free((void **)&hdr);
		fcount++;
	}

	/* destroy all busy buffers */
	list_for_each_entry_safe(hdr, dummy, &pool->busy_pool, list) {
		list_del(&hdr->list);
		msm_vidc_vmem_free((void **)&hdr);
		bcount++;
	}

	i_vpr_h(inst, "%s: type: %23s, count: free %2u, busy %2u\n",
		__func__, pool->name, fcount, bcount);
}

int msm_vidc_pools_init(struct msm_vidc_inst *inst)
{
	u32 i;

	if (!inst) {
		d_vpr_e("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	if (ARRAY_SIZE(buftype_size_name_arr) != MSM_MEM_POOL_MAX) {
		i_vpr_e(inst, "%s: num elements mismatch %lu %u\n", __func__,
			ARRAY_SIZE(buftype_size_name_arr), MSM_MEM_POOL_MAX);
		return -EINVAL;
	}

	for (i = 0; i < MSM_MEM_POOL_MAX; i++) {
		if (i != buftype_size_name_arr[i].type) {
			i_vpr_e(inst, "%s: type mismatch %u %u\n", __func__,
				i, buftype_size_name_arr[i].type);
			return -EINVAL;
		}
		inst->pool[i].size = buftype_size_name_arr[i].size;
		inst->pool[i].name = buftype_size_name_arr[i].name;
		INIT_LIST_HEAD(&inst->pool[i].free_pool);
		INIT_LIST_HEAD(&inst->pool[i].busy_pool);
	}

	return 0;
}

void msm_vidc_pools_deinit(struct msm_vidc_inst *inst)
{
	u32 i = 0;

	if (!inst) {
		d_vpr_e("%s: Invalid params\n", __func__);
		return;
	}

	/* destroy all buffers from all pool types */
	for (i = 0; i < MSM_MEM_POOL_MAX; i++)
		msm_vidc_destroy_pool_buffers(inst, i);
}

static struct dma_buf *msm_vidc_dma_buf_get(struct msm_vidc_inst *inst, int fd)
{
	struct msm_memory_dmabuf *buf = NULL;
	struct dma_buf *dmabuf = NULL;
	bool found = false;

	if (!inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		return NULL;
	}

	/* get local dmabuf ref for tracking */
	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		d_vpr_e("Failed to get dmabuf for %d, error %ld\n",
				fd, PTR_ERR(dmabuf));
		return NULL;
	}

	/* track dmabuf - inc refcount if already present */
	list_for_each_entry(buf, &inst->dmabuf_tracker, list) {
		if (buf->dmabuf == dmabuf) {
			buf->refcount++;
			found = true;
			break;
		}
	}
	if (found) {
		/* put local dmabuf ref */
		dma_buf_put(dmabuf);
		return dmabuf;
	}

	/* get tracker instance from pool */
	buf = msm_vidc_pool_alloc(inst, MSM_MEM_POOL_DMABUF);
	if (!buf) {
		i_vpr_e(inst, "%s: dmabuf alloc failed\n", __func__);
		dma_buf_put(dmabuf);
		return NULL;
	}
	/* hold dmabuf strong ref in tracker */
	buf->dmabuf = dmabuf;
	buf->refcount = 1;
	INIT_LIST_HEAD(&buf->list);

	/* add new dmabuf entry to tracker */
	list_add_tail(&buf->list, &inst->dmabuf_tracker);

	return dmabuf;
}

static void msm_vidc_dma_buf_put(struct msm_vidc_inst *inst, struct dma_buf *dmabuf)
{
	struct msm_memory_dmabuf *buf = NULL;
	bool found = false;

	if (!inst || !dmabuf) {
		d_vpr_e("%s: invalid params\n", __func__);
		return;
	}

	/* track dmabuf - dec refcount if already present */
	list_for_each_entry(buf, &inst->dmabuf_tracker, list) {
		if (buf->dmabuf == dmabuf) {
			buf->refcount--;
			found = true;
			break;
		}
	}
	if (!found) {
		i_vpr_e(inst, "%s: invalid dmabuf %p\n", __func__, dmabuf);
		return;
	}

	/* non-zero refcount - do nothing */
	if (buf->refcount)
		return;

	/* remove dmabuf entry from tracker */
	list_del(&buf->list);

	/* release dmabuf strong ref from tracker */
	dma_buf_put(buf->dmabuf);

	/* put tracker instance back to pool */
	msm_vidc_pool_free(inst, buf);
}

static void msm_vidc_dma_buf_put_completely(struct msm_vidc_inst *inst,
	struct msm_memory_dmabuf *buf)
{
	if (!inst || !buf) {
		d_vpr_e("%s: invalid params\n", __func__);
		return;
	}

	while (buf->refcount) {
		buf->refcount--;
		if (!buf->refcount) {
			/* remove dmabuf entry from tracker */
			list_del(&buf->list);

			/* release dmabuf strong ref from tracker */
			dma_buf_put(buf->dmabuf);

			/* put tracker instance back to pool */
			msm_vidc_pool_free(inst, buf);
			break;
		}
	}
}

static struct dma_buf_attachment *msm_vidc_dma_buf_attach(struct msm_vidc_core *core,
	struct dma_buf *dbuf, struct device *dev)
{
	int rc = 0;
	struct dma_buf_attachment *attach = NULL;

	if (!core || !dbuf || !dev) {
		d_vpr_e("%s: invalid params\n", __func__);
		return NULL;
	}

	attach = dma_buf_attach(dbuf, dev);
	if (IS_ERR_OR_NULL(attach)) {
		rc = PTR_ERR(attach) ? PTR_ERR(attach) : -1;
		d_vpr_e("Failed to attach dmabuf, error %d\n", rc);
		return NULL;
	}

	return attach;
}

static int msm_vidc_dma_buf_detach(struct msm_vidc_core *core,
	struct dma_buf *dbuf, struct dma_buf_attachment *attach)
{
	int rc = 0;

	if (!dbuf || !attach) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	dma_buf_detach(dbuf, attach);

	return rc;
}

static int msm_vidc_dma_buf_unmap_attachment(struct msm_vidc_core *core,
	struct dma_buf_attachment *attach, struct sg_table *table)
{
	int rc = 0;

	if (!attach || !table) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);

	return rc;
}

static struct sg_table *msm_vidc_dma_buf_map_attachment(
	struct msm_vidc_core *core, struct dma_buf_attachment *attach)
{
	int rc = 0;
	struct sg_table *table = NULL;

	if (!attach) {
		d_vpr_e("%s: invalid params\n", __func__);
		return NULL;
	}

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(table)) {
		rc = PTR_ERR(table) ? PTR_ERR(table) : -1;
		d_vpr_e("Failed to map table, error %d\n", rc);
		return NULL;
	}
	if (!table->sgl) {
		d_vpr_e("%s: sgl is NULL\n", __func__);
		msm_vidc_dma_buf_unmap_attachment(core, attach, table);
		return NULL;
	}

	return table;
}

static int msm_vidc_memory_map(struct msm_vidc_core *core, struct msm_vidc_map *map)
{
	int rc = 0;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *table = NULL;
	struct context_bank_info *cb = NULL;

	if (!core || !map) {
		d_vpr_e("%s: invalid params\n",	__func__);
		return -EINVAL;
	}

	if (map->refcount) {
		map->refcount++;
		goto exit;
	}

	cb = msm_vidc_get_context_bank_for_region(core, map->region);
	if (!cb) {
		d_vpr_e("%s: Failed to get context bank device\n",
			 __func__);
		rc = -EIO;
		goto error_cb;
	}

	/* Prepare a dma buf for dma on the given device */
	attach = msm_vidc_dma_buf_attach(core, map->dmabuf, cb->dev);
	if (IS_ERR_OR_NULL(attach)) {
		rc = PTR_ERR(attach) ? PTR_ERR(attach) : -ENOMEM;
		d_vpr_e("Failed to attach dmabuf\n");
		goto error_attach;
	}

	table = msm_vidc_dma_buf_map_attachment(core, attach);
	if (IS_ERR_OR_NULL(table)) {
		rc = PTR_ERR(table) ? PTR_ERR(table) : -ENOMEM;
		d_vpr_e("Failed to map table\n");
		goto error_table;
	}

	map->device_addr = sg_dma_address(table->sgl);
	map->table = table;
	map->attach = attach;
	map->refcount++;

exit:
	d_vpr_l(
		"%s: type %11s, device_addr %#llx, refcount %d, region %d\n",
		__func__, buf_name(map->type), map->device_addr, map->refcount, map->region);

	return 0;

error_table:
	msm_vidc_dma_buf_detach(core, map->dmabuf, attach);
error_attach:
error_cb:
	return rc;
}

static int msm_vidc_memory_unmap(struct msm_vidc_core *core,
	struct msm_vidc_map *map)
{
	int rc = 0;

	if (!core || !map) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (map->refcount) {
		map->refcount--;
	} else {
		d_vpr_e("unmap called while refcount is zero already\n");
		return -EINVAL;
	}

	d_vpr_l(
		"%s: type %11s, device_addr %#llx, refcount %d, region %d\n",
		__func__, buf_name(map->type), map->device_addr, map->refcount, map->region);

	if (map->refcount)
		goto exit;

	msm_vidc_dma_buf_unmap_attachment(core, map->attach, map->table);
	msm_vidc_dma_buf_detach(core, map->dmabuf, map->attach);

	map->device_addr = 0x0;
	map->attach = NULL;
	map->table = NULL;

exit:
	return rc;
}

static int msm_vidc_memory_alloc(struct msm_vidc_core *core, struct msm_vidc_alloc *mem)
{
	d_vpr_e("%s: unsupported\n", __func__);
	return -EINVAL;
}

static int msm_vidc_memory_free(struct msm_vidc_core *core, struct msm_vidc_alloc *mem)
{
	d_vpr_e("%s: unsupported\n", __func__);
	return -EINVAL;
}

static u32 msm_vidc_buffer_region(struct msm_vidc_inst *inst,
	enum msm_vidc_buffer_type buffer_type)
{
	return MSM_VIDC_NON_SECURE;
}

static struct msm_vidc_memory_ops msm_mem_ops = {
	.dma_buf_get                    = msm_vidc_dma_buf_get,
	.dma_buf_put                    = msm_vidc_dma_buf_put,
	.dma_buf_put_completely         = msm_vidc_dma_buf_put_completely,
	.dma_buf_attach                 = msm_vidc_dma_buf_attach,
	.dma_buf_detach                 = msm_vidc_dma_buf_detach,
	.dma_buf_map_attachment         = msm_vidc_dma_buf_map_attachment,
	.dma_buf_unmap_attachment       = msm_vidc_dma_buf_unmap_attachment,
	.memory_map                     = msm_vidc_memory_map,
	.memory_unmap                   = msm_vidc_memory_unmap,
	.memory_alloc                   = msm_vidc_memory_alloc,
	.memory_free                    = msm_vidc_memory_free,
	.buffer_region                  = msm_vidc_buffer_region,
};

struct msm_vidc_memory_ops *get_mem_ops(void)
{
	return &msm_mem_ops;
}
