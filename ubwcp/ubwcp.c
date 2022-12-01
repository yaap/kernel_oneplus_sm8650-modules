// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/dma-buf.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/hashtable.h>
#include <linux/scatterlist.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/genalloc.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/numa.h>
#include <linux/memory_hotplug.h>
#include <asm/page.h>
#include <linux/delay.h>
#include <linux/ubwcp_dma_heap.h>
#include <linux/debugfs.h>

MODULE_IMPORT_NS(DMA_BUF);

#include "ubwcp.h"
#include "ubwcp_hw.h"
#include "include/uapi/ubwcp_ioctl.h"

#define UBWCP_NUM_DEVICES 1
#define UBWCP_DEVICE_NAME "ubwcp"

#define UBWCP_BUFFER_DESC_OFFSET  64
#define UBWCP_BUFFER_DESC_COUNT  256


#define CACHE_ADDR(x) ((x) >> 6)
#define PAGE_ADDR(x)  ((x) >> 12)
#define UBWCP_ALIGN(_x, _y)  ((((_x) + (_y) - 1)/(_y))*(_y))


//#define DBG(fmt, args...)
//#define DBG_BUF_ATTR(fmt, args...)
#define DBG_BUF_ATTR(fmt, args...) do { if (ubwcp_debug_trace_enable) \
					pr_err("ubwcp: %s(): " fmt "\n", __func__, ##args); \
					} while (0)
#define DBG(fmt, args...) do { if (ubwcp_debug_trace_enable) \
				pr_err("ubwcp: %s(): " fmt "\n", __func__, ##args); \
				} while (0)
#define ERR(fmt, args...) pr_err("ubwcp: %s(): ~~~ERROR~~~: " fmt "\n", __func__, ##args)

#define FENTRY() DBG("")


#define META_DATA_PITCH_ALIGN    64
#define META_DATA_HEIGHT_ALIGN   16
#define META_DATA_SIZE_ALIGN  4096
#define PIXEL_DATA_SIZE_ALIGN 4096

struct ubwcp_desc {
	int idx;
	void *ptr;
};

/* TBD: confirm size of width/height */
struct ubwcp_dimension {
	u16 width;
	u16 height;
};

struct ubwcp_plane_info {
	u16 pixel_bytes;
	u16 per_pixel;
	struct ubwcp_dimension tilesize_p;      /* pixels */
	struct ubwcp_dimension macrotilesize_p; /* pixels */
};

struct ubwcp_image_format_info {
	u16 planes;
	struct ubwcp_plane_info p_info[2];
};

enum ubwcp_std_image_format {
	RGBA   = 0,
	NV12   = 1,
	NV124R = 2,
	P010   = 3,
	TP10   = 4,
	P016   = 5,
	INFO_FORMAT_LIST_SIZE,
	STD_IMAGE_FORMAT_INVALID = 0xFF
};

struct ubwcp_driver {
	/* cdev related */
	dev_t devt;
	struct class *dev_class; //sysfs dev class
	struct device *dev_sys; //sysfs dev
	struct cdev cdev; //char dev

	/* debugfs */
	struct dentry *debugfs_root;

	/* ubwcp devices */
	struct device *dev; //ubwcp device
	struct device *dev_desc_cb; //smmu dev for descriptors
	struct device *dev_buf_cb; //smmu dev for ubwcp buffers

	void __iomem *base; //ubwcp base address
	struct regulator *vdd;

	/* interrupts */
	int irq_range_ck_rd;
	int irq_range_ck_wr;
	int irq_encode;
	int irq_decode;

	/* ula address pool */
	u64 ula_pool_base;
	u64 ula_pool_size;
	struct gen_pool *ula_pool;

	configure_mmap mmap_config_fptr;

	/* HW version */
	u32 hw_ver_major;
	u32 hw_ver_minor;

	/* keep track of all buffers. hash table index'ed using dma_buf ptr.
	 * 2**8 = 256 hash values
	 */
	DECLARE_HASHTABLE(buf_table, 8);

	/* buffer descriptor */
	void       *buffer_desc_base;      /* CPU address */
	dma_addr_t buffer_desc_dma_handle; /* dma address */
	size_t buffer_desc_size;
	struct ubwcp_desc desc_list[UBWCP_BUFFER_DESC_COUNT];

	struct ubwcp_image_format_info format_info[INFO_FORMAT_LIST_SIZE];

	struct mutex desc_lock;        /* allocate/free descriptors */
	struct mutex buf_table_lock;   /* add/remove dma_buf into list of managed bufffers */
	struct mutex ula_lock;         /* allocate/free ula */
	struct mutex ubwcp_flush_lock; /* ubwcp flush */
	struct mutex hw_range_ck_lock; /* range ck */
};

struct ubwcp_buf {
	struct hlist_node hnode;
	struct ubwcp_driver *ubwcp;
	struct ubwcp_buffer_attrs buf_attr;
	bool perm;
	struct ubwcp_desc *desc;
	bool buf_attr_set;
	bool locked;
	enum dma_data_direction lock_dir;
	int lock_count;

	/* dma_buf info */
	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;

	/* ula info */
	phys_addr_t ula_pa;
	size_t ula_size;

	/* meta metadata */
	struct ubwcp_hw_meta_metadata mmdata;
	struct mutex lock;
};


static struct ubwcp_driver *me;
static int error_print_count;
u32 ubwcp_debug_trace_enable;

static struct ubwcp_driver *ubwcp_get_driver(void)
{
	if (!me)
		WARN(1, "ubwcp: driver ptr requested but driver not initialized");

	return me;
}

static void image_format_init(struct ubwcp_driver *ubwcp)
{				/* planes, bytes/p,   Tp  ,    MTp    */
	ubwcp->format_info[RGBA]   = (struct ubwcp_image_format_info)
					{1, {{4, 1,  {16, 4}, {64, 16}}}};
	ubwcp->format_info[NV12]   = (struct ubwcp_image_format_info)
					{2, {{1,  1, {32, 8}, {128, 32}},
					{2,  1, {16, 8}, { 64, 32}}}};
	ubwcp->format_info[NV124R] = (struct ubwcp_image_format_info)
					{2, {{1,  1, {64, 4}, {256, 16}},
					{2,  1, {32, 4}, {128, 16}}}};
	ubwcp->format_info[P010]   = (struct ubwcp_image_format_info)
					{2, {{2,  1, {32, 4}, {128, 16}},
					{4,  1, {16, 4}, { 64, 16}}}};
	ubwcp->format_info[TP10]   = (struct ubwcp_image_format_info)
					{2, {{4,  3, {48, 4}, {192, 16}},
					{8,  3, {24, 4}, { 96, 16}}}};
	ubwcp->format_info[P016]   = (struct ubwcp_image_format_info)
					{2, {{2,  1, {32, 4}, {128, 16}},
					{4,  1, {16, 4}, { 64, 16}}}};
}

static void ubwcp_buf_desc_list_init(struct ubwcp_driver *ubwcp)
{
	int idx;
	struct ubwcp_desc *desc_list = ubwcp->desc_list;

	for (idx = 0; idx < UBWCP_BUFFER_DESC_COUNT; idx++) {
		desc_list[idx].idx = -1;
		desc_list[idx].ptr = NULL;
	}
}

/* UBWCP Power control */
static int ubwcp_power(struct ubwcp_driver *ubwcp, bool enable)
{
	int ret = 0;

	if (!ubwcp) {
		ERR("ubwcp ptr is NULL");
		return -1;
	}

	if (!ubwcp->vdd) {
		ERR("vdd is NULL");
		return -1;
	}

	if (enable) {
		ret = regulator_enable(ubwcp->vdd);
		if (ret < 0) {
			ERR("regulator_enable failed: %d", ret);
			ret = -1;
		} else {
			DBG("regulator_enable() success");
		}
	} else {
		ret = regulator_disable(ubwcp->vdd);
		if (ret < 0) {
			ERR("regulator_disable failed: %d", ret);
			ret = -1;
		} else {
			DBG("regulator_disable() success");
		}
	}
	return ret;
}


static int ubwcp_flush(struct ubwcp_driver *ubwcp)
{
	int ret = 0;

	mutex_lock(&ubwcp->ubwcp_flush_lock);
	ret = ubwcp_hw_flush(ubwcp->base);
	mutex_unlock(&ubwcp->ubwcp_flush_lock);
	if (ret != 0)
		WARN(1, "ubwcp_hw_flush() failed!");

	return ret;
}


/* get dma_buf ptr for the given dma_buf fd */
struct dma_buf *ubwcp_dma_buf_fd_to_dma_buf(int dma_buf_fd)
{
	struct dma_buf *dmabuf;

	/* TBD: dma_buf_get() results in taking ref to buf and it won't ever get
	 * free'ed until ref count goes to 0. So we must reduce the ref count
	 * immediately after we find our corresponding ubwcp_buf.
	 */
	dmabuf = dma_buf_get(dma_buf_fd);
	if (IS_ERR(dmabuf)) {
		ERR("dmabuf ptr not found for dma_buf_fd = %d", dma_buf_fd);
		return NULL;
	}

	dma_buf_put(dmabuf);

	return dmabuf;
}
EXPORT_SYMBOL(ubwcp_dma_buf_fd_to_dma_buf);


/* get ubwcp_buf corresponding to the given dma_buf */
static struct ubwcp_buf *dma_buf_to_ubwcp_buf(struct dma_buf *dmabuf)
{
	struct ubwcp_buf *buf = NULL;
	struct ubwcp_driver *ubwcp = ubwcp_get_driver();

	if (!dmabuf || !ubwcp)
		return NULL;

	mutex_lock(&ubwcp->buf_table_lock);
	/* look up ubwcp_buf corresponding to this dma_buf */
	hash_for_each_possible(ubwcp->buf_table, buf, hnode, (u64)dmabuf) {
		if (buf->dma_buf == dmabuf)
			break;
	}
	mutex_unlock(&ubwcp->buf_table_lock);

	return buf;
}


/* return ubwcp hardware version */
int ubwcp_get_hw_version(struct ubwcp_ioctl_hw_version *ver)
{
	struct ubwcp_driver *ubwcp;

	FENTRY();

	if (!ver) {
		ERR("invalid version ptr");
		return -EINVAL;
	}

	ubwcp = ubwcp_get_driver();
	if (!ubwcp)
		return -1;

	ver->major = ubwcp->hw_ver_major;
	ver->minor = ubwcp->hw_ver_minor;
	return 0;
}
EXPORT_SYMBOL(ubwcp_get_hw_version);

/**
 *
 * Initialize ubwcp buffer for the given dma_buf. This
 * initializes ubwcp internal data structures and possibly hw to
 * use ubwcp for this buffer.
 *
 * @param dmabuf : ptr to the buffer to be configured for ubwcp
 *
 * @return int : 0 on success, otherwise error code
 */
static int ubwcp_init_buffer(struct dma_buf *dmabuf)
{
	int ret = 0;
	int nid;
	struct ubwcp_buf *buf;
	struct ubwcp_driver *ubwcp = ubwcp_get_driver();

	FENTRY();

	if (!ubwcp)
		return -1;

	if (!dmabuf) {
		ERR("NULL dmabuf input ptr");
		return -EINVAL;
	}

	if (dma_buf_to_ubwcp_buf(dmabuf)) {
		ERR("dma_buf already initialized for ubwcp");
		return -EEXIST;
	}

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf) {
		ERR("failed to alloc for new ubwcp_buf");
		return -ENOMEM;
	}

	mutex_init(&buf->lock);
	buf->dma_buf = dmabuf;
	buf->ubwcp   = ubwcp;

	mutex_lock(&ubwcp->buf_table_lock);
	if (hash_empty(ubwcp->buf_table)) {

		ret = ubwcp_power(ubwcp, true);
		if (ret)
			goto err_power_on;

		nid = memory_add_physaddr_to_nid(ubwcp->ula_pool_base);
		DBG("calling add_memory()...");
		ret = add_memory(nid, ubwcp->ula_pool_base, ubwcp->ula_pool_size, MHP_NONE);
		if (ret) {
			ERR("add_memory() failed st:0x%lx sz:0x%lx err: %d",
				ubwcp->ula_pool_base,
				ubwcp->ula_pool_size,
				ret);
			goto err_add_memory;
		} else {
			DBG("add_memory() ula_pool_base:0x%llx, size:0x%zx, kernel addr:0x%p",
				ubwcp->ula_pool_base,
				ubwcp->ula_pool_size,
				page_to_virt(pfn_to_page(PFN_DOWN(ubwcp->ula_pool_base))));
		}
	}
	hash_add(ubwcp->buf_table, &buf->hnode, (u64)buf->dma_buf);
	mutex_unlock(&ubwcp->buf_table_lock);
	return ret;

err_add_memory:
	ubwcp_power(ubwcp, false);
err_power_on:
	mutex_unlock(&ubwcp->buf_table_lock);
	kfree(buf);
	if (!ret)
		ret = -1;
	return ret;
}

static void dump_attributes(struct ubwcp_buffer_attrs *attr)
{
	DBG_BUF_ATTR("");
	DBG_BUF_ATTR("image_format: %d", attr->image_format);
	DBG_BUF_ATTR("major_ubwc_ver: %d", attr->major_ubwc_ver);
	DBG_BUF_ATTR("minor_ubwc_ver: %d", attr->minor_ubwc_ver);
	DBG_BUF_ATTR("compression_type: %d", attr->compression_type);
	DBG_BUF_ATTR("lossy_params: %llu", attr->lossy_params);
	DBG_BUF_ATTR("width: %d", attr->width);
	DBG_BUF_ATTR("height: %d", attr->height);
	DBG_BUF_ATTR("stride: %d", attr->stride);
	DBG_BUF_ATTR("scanlines: %d", attr->scanlines);
	DBG_BUF_ATTR("planar_padding: %d", attr->planar_padding);
	DBG_BUF_ATTR("subsample: %d", attr->subsample);
	DBG_BUF_ATTR("sub_system_target: %d", attr->sub_system_target);
	DBG_BUF_ATTR("y_offset: %d", attr->y_offset);
	DBG_BUF_ATTR("batch_size: %d", attr->batch_size);
	DBG_BUF_ATTR("");
}

/* validate buffer attributes */
static bool ubwcp_buf_attrs_valid(struct ubwcp_buffer_attrs *attr)
{
	bool valid_format;

	switch (attr->image_format) {
	case UBWCP_LINEAR:
	case UBWCP_RGBA8888:
	case UBWCP_NV12:
	case UBWCP_NV12_Y:
	case UBWCP_NV12_UV:
	case UBWCP_NV124R:
	case UBWCP_NV124R_Y:
	case UBWCP_NV124R_UV:
	case UBWCP_TP10:
	case UBWCP_TP10_Y:
	case UBWCP_TP10_UV:
	case UBWCP_P010:
	case UBWCP_P010_Y:
	case UBWCP_P010_UV:
	case UBWCP_P016:
	case UBWCP_P016_Y:
	case UBWCP_P016_UV:
		valid_format = true;
		break;
	default:
		valid_format = false;
	}

	if (!valid_format) {
		ERR("invalid image format: %d", attr->image_format);
		goto err;
	}

	if (attr->major_ubwc_ver || attr->minor_ubwc_ver) {
		ERR("major/minor ubwc ver must be 0. major: %d minor: %d",
			attr->major_ubwc_ver, attr->minor_ubwc_ver);
		goto err;
	}

	if (attr->compression_type != UBWCP_COMPRESSION_LOSSLESS) {
		ERR("compression_type is not valid: %d",
			attr->compression_type);
		goto err;
	}

	if (attr->lossy_params != 0) {
		ERR("lossy_params is not valid: %d", attr->lossy_params);
		goto err;
	}

	//TBD: some upper limit for width?
	if (attr->width > 10*1024) {
		ERR("width is invalid (above upper limit): %d", attr->width);
		goto err;
	}

	//TBD: some upper limit for height?
	if (attr->height > 10*1024) {
		ERR("height is invalid (above upper limit): %d", attr->height);
		goto err;
	}


	/* TBD: what's the upper limit for stride? 8K is likely too high. */
	if (!IS_ALIGNED(attr->stride, 64) ||
	    (attr->stride < attr->width) ||
	    (attr->stride > 4*8192)) {
		ERR("stride is not valid (aligned to 64 and <= 8192): %d",
			attr->stride);
		goto err;
	}

	/* TBD: currently assume height + 10. Replace 10 with right num from camera. */
	if ((attr->scanlines < attr->height) ||
	    (attr->scanlines > attr->height + 10)) {
		ERR("scanlines is not valid - height: %d scanlines: %d",
			attr->height, attr->scanlines);
		goto err;
	}

	if (attr->planar_padding > 4096) {
		ERR("planar_padding is not valid. (<= 4096): %d",
			attr->planar_padding);
		goto err;
	}

	if (attr->subsample != UBWCP_SUBSAMPLE_4_2_0)  {
		ERR("subsample is not valid: %d", attr->subsample);
		goto err;
	}

	if (attr->sub_system_target & ~UBWCP_SUBSYSTEM_TARGET_CPU) {
		ERR("sub_system_target other that CPU is not supported: %d",
			attr->sub_system_target);
		goto err;
	}

	if (!(attr->sub_system_target & UBWCP_SUBSYSTEM_TARGET_CPU)) {
		ERR("sub_system_target is not set to CPU: %d",
			attr->sub_system_target);
		goto err;
	}

	if (attr->y_offset != 0) {
		ERR("y_offset is not valid: %d", attr->y_offset);
		goto err;
	}

	if (attr->batch_size != 1) {
		ERR("batch_size is not valid: %d", attr->batch_size);
		goto err;
	}

	dump_attributes(attr);
	return true;
err:
	dump_attributes(attr);
	return false;
}


/* return true if image format has only Y plane*/
bool ubwcp_image_y_only(u16 format)
{
	switch (format) {
	case UBWCP_NV12_Y:
	case UBWCP_NV124R_Y:
	case UBWCP_TP10_Y:
	case UBWCP_P010_Y:
	case UBWCP_P016_Y:
		return true;
	default:
		return false;
	}
}


/* return true if image format has only UV plane*/
bool ubwcp_image_uv_only(u16 format)
{
	switch (format) {
	case UBWCP_NV12_UV:
	case UBWCP_NV124R_UV:
	case UBWCP_TP10_UV:
	case UBWCP_P010_UV:
	case UBWCP_P016_UV:
		return true;
	default:
		return false;
	}
}

/* calculate and return metadata buffer size for a given plane
 * and buffer attributes
 * NOTE: in this function, we will only pass in NV12 format.
 * NOT NV12_Y or NV12_UV etc.
 * the Y or UV information is in the "plane"
 * "format" here purely means "encoding format" and no information
 * if some plane data is missing.
 */
static size_t metadata_buf_sz(struct ubwcp_driver *ubwcp,
				enum ubwcp_std_image_format format,
				u32 width, u32 height, u8 plane)
{
	size_t size;
	u64 pitch;
	u64 lines;
	u64 tile_width;
	u32 tile_height;

	struct ubwcp_image_format_info f_info;
	struct ubwcp_plane_info p_info;

	f_info = ubwcp->format_info[format];

	DBG_BUF_ATTR("");
	DBG_BUF_ATTR("");
	DBG_BUF_ATTR("Calculating metadata buffer size: format = %d, plane = %d", format, plane);

	if (plane >= f_info.planes) {
		ERR("Format does not have requested plane info: format: %d, plane: %d",
			format, plane);
		WARN(1, "Fix this!!!!!");
		return 0;
	}

	p_info = f_info.p_info[plane];

	/* UV plane */
	if (plane == 1) {
		width  = width/2;
		height = height/2;
	}

	tile_width  = p_info.tilesize_p.width;
	tile_height = p_info.tilesize_p.height;

	/* pitch: # of tiles in a row
	 * lines: # of tile rows
	 */
	pitch =  UBWCP_ALIGN((width  + tile_width  - 1)/tile_width,  META_DATA_PITCH_ALIGN);
	lines =  UBWCP_ALIGN((height + tile_height - 1)/tile_height, META_DATA_HEIGHT_ALIGN);

	DBG_BUF_ATTR("image params     : %d x %d (pixels)", width, height);
	DBG_BUF_ATTR("tile  params     : %d x %d (pixels)", tile_width, tile_height);
	DBG_BUF_ATTR("pitch            : %d (%d)", pitch, width/tile_width);
	DBG_BUF_ATTR("lines            : %d (%d)", lines, height);
	DBG_BUF_ATTR("size (p*l*bytes) : %d", pitch*lines*1);

	/* x1 below is only to clarify that we are multiplying by 1 bytes/tile */
	size = UBWCP_ALIGN(pitch*lines*1, META_DATA_SIZE_ALIGN);

	DBG_BUF_ATTR("size (aligned 4K): %zu (0x%zx)", size, size);
	return size;
}


/* calculate and return size of pixel data buffer for a given plane
 * and buffer attributes
 */
static size_t pixeldata_buf_sz(struct ubwcp_driver *ubwcp,
					u16 format, u32 width,
					u32 height, u8 plane)
{
	size_t size;
	u64 pitch;
	u64 lines;
	u16 pixel_bytes;
	u16 per_pixel;
	u64 macro_tile_width_p;
	u64 macro_tile_height_p;

	struct ubwcp_image_format_info f_info;
	struct ubwcp_plane_info p_info;

	f_info = ubwcp->format_info[format];

	DBG_BUF_ATTR("");
	DBG_BUF_ATTR("");
	DBG_BUF_ATTR("Calculating Pixeldata buffer size: format = %d, plane = %d", format, plane);

	if (plane >= f_info.planes) {
		ERR("Format does not have requested plane info: format: %d, plane: %d",
			format, plane);
		WARN(1, "Fix this!!!!!");
		return 0;
	}

	p_info = f_info.p_info[plane];

	pixel_bytes = p_info.pixel_bytes;
	per_pixel   = p_info.per_pixel;

	/* UV plane */
	if (plane == 1) {
		width  = width/2;
		height = height/2;
	}

	macro_tile_width_p  = p_info.macrotilesize_p.width;
	macro_tile_height_p = p_info.macrotilesize_p.height;

	/* align pixel width and height macro tile width and height */
	pitch = UBWCP_ALIGN(width, macro_tile_width_p);
	lines = UBWCP_ALIGN(height, macro_tile_height_p);

	DBG_BUF_ATTR("image params     : %d x %d (pixels)", width, height);
	DBG_BUF_ATTR("macro tile params: %d x %d (pixels)", macro_tile_width_p,
								macro_tile_height_p);
	DBG_BUF_ATTR("bytes_per_pixel  : %d/%d", pixel_bytes, per_pixel);
	DBG_BUF_ATTR("pitch            : %d", pitch);
	DBG_BUF_ATTR("lines            : %d", lines);
	DBG_BUF_ATTR("size (p*l*bytes) : %d", (pitch*lines*pixel_bytes)/per_pixel);

	size  = UBWCP_ALIGN((pitch*lines*pixel_bytes)/per_pixel, PIXEL_DATA_SIZE_ALIGN);

	DBG_BUF_ATTR("size (aligned 4K): %zu (0x%zx)", size, size);

	return size;
}

/*
 * plane: must be 0 or 1 (1st plane == 0, 2nd plane == 1)
 */
static size_t ubwcp_ula_size(struct ubwcp_driver *ubwcp, u16 format,
					u32 stride_b, u32 scanlines, u8 plane)
{
	size_t size;

	DBG_BUF_ATTR("%s(format = %d, plane = %d)", __func__, format, plane);
	/* UV plane */
	if (plane == 1)
		scanlines = scanlines/2;
	size = stride_b*scanlines;
	DBG_BUF_ATTR("Size of plane-%u: (%u * %u) = %zu (0x%zx)",
		plane, stride_b, scanlines, size, size);
	return size;
}

int missing_plane_from_format(u16 ioctl_image_format)
{
	int missing_plane;

	switch (ioctl_image_format) {
	case UBWCP_NV12_Y:
		missing_plane = 2;
		break;
	case UBWCP_NV12_UV:
		missing_plane = 1;
		break;
	case UBWCP_NV124R_Y:
		missing_plane = 2;
		break;
	case UBWCP_NV124R_UV:
		missing_plane = 1;
		break;
	case UBWCP_TP10_Y:
		missing_plane = 2;
		break;
	case UBWCP_TP10_UV:
		missing_plane = 1;
		break;
	case UBWCP_P010_Y:
		missing_plane = 2;
		break;
	case UBWCP_P010_UV:
		missing_plane = 1;
		break;
	case UBWCP_P016_Y:
		missing_plane = 2;
		break;
	case UBWCP_P016_UV:
		missing_plane = 1;
		break;
	default:
		missing_plane = 0;
	}
	return missing_plane;
}

int planes_in_format(enum ubwcp_std_image_format format)
{
	if (format == RGBA)
		return 1;
	else
		return 2;
}

enum ubwcp_std_image_format to_std_format(u16 ioctl_image_format)
{
	switch (ioctl_image_format) {
	case UBWCP_RGBA8888:
		return RGBA;
	case UBWCP_NV12:
	case UBWCP_NV12_Y:
	case UBWCP_NV12_UV:
		return NV12;
	case UBWCP_NV124R:
	case UBWCP_NV124R_Y:
	case UBWCP_NV124R_UV:
		return NV124R;
	case UBWCP_TP10:
	case UBWCP_TP10_Y:
	case UBWCP_TP10_UV:
		return TP10;
	case UBWCP_P010:
	case UBWCP_P010_Y:
	case UBWCP_P010_UV:
		return P010;
	case UBWCP_P016:
	case UBWCP_P016_Y:
	case UBWCP_P016_UV:
		return P016;
	default:
		WARN(1, "Fix this!!!");
		return STD_IMAGE_FORMAT_INVALID;
	}
}

unsigned int ubwcp_get_hw_image_format_value(u16 ioctl_image_format)
{
	enum ubwcp_std_image_format format;

	format = to_std_format(ioctl_image_format);
	switch (format) {
	case RGBA:
		return HW_BUFFER_FORMAT_RGBA;
	case NV12:
		return HW_BUFFER_FORMAT_NV12;
	case NV124R:
		return HW_BUFFER_FORMAT_NV124R;
	case P010:
		return HW_BUFFER_FORMAT_P010;
	case TP10:
		return HW_BUFFER_FORMAT_TP10;
	case P016:
		return HW_BUFFER_FORMAT_P016;
	default:
		WARN(1, "Fix this!!!!!");
		return 0;
	}
}

/* calculate ULA buffer parms
 * TBD: how do we make sure uv_start address (not the offset)
 * is aligned per requirement: cache line
 */
static int ubwcp_calc_ula_params(struct ubwcp_driver *ubwcp,
					struct ubwcp_buffer_attrs *attr,
					size_t *ula_size,
					size_t *uv_start_offset)
{
	size_t size;
	enum ubwcp_std_image_format format;
	int planes;
	int missing_plane;
	u32 stride;
	u32 scanlines;
	u32 planar_padding;

	stride         = attr->stride;
	scanlines      = attr->scanlines;
	planar_padding = attr->planar_padding;

	/* convert ioctl image format to standard image format */
	format = to_std_format(attr->image_format);


	/* Number of "expected" planes in "the standard defined" image format */
	planes = planes_in_format(format);

	/* any plane missing?
	 * valid missing_plane values:
	 *      0 == no plane missing
	 *      1 == 1st plane missing
	 *      2 == 2nd plane missing
	 */
	missing_plane = missing_plane_from_format(attr->image_format);

	DBG_BUF_ATTR("ioctl_image_format : %d, std_format: %d", attr->image_format, format);
	DBG_BUF_ATTR("planes_in_format   : %d", planes);
	DBG_BUF_ATTR("missing_plane      : %d", missing_plane);
	DBG_BUF_ATTR("Planar Padding     : %d", planar_padding);

	if (planes == 1) {
		/* uv_start beyond ULA range */
		size = ubwcp_ula_size(ubwcp, format, stride, scanlines, 0);
		*uv_start_offset = size;
	} else {
		if (!missing_plane) {
			/* size for both planes and padding */
			size = ubwcp_ula_size(ubwcp, format, stride, scanlines, 0);
			size += planar_padding;
			*uv_start_offset = size;
			size += ubwcp_ula_size(ubwcp, format, stride, scanlines, 1);
		} else  {
			if (missing_plane == 2) {
				/* Y-only image, set uv_start beyond ULA range */
				size = ubwcp_ula_size(ubwcp, format, stride, scanlines, 0);
				*uv_start_offset = size;
			} else {
				/* first plane data is not there */
				size = ubwcp_ula_size(ubwcp, format, stride, scanlines, 1);
				*uv_start_offset = 0; /* uv data is at the beginning */
			}
		}
	}

	//TBD: cleanup
	*ula_size = size;
	DBG_BUF_ATTR("Before page align: Total ULA_Size: %d (0x%x) (planes + planar padding)",
									*ula_size, *ula_size);
	*ula_size = UBWCP_ALIGN(size, 4096);
	DBG_BUF_ATTR("After page align : Total ULA_Size: %d (0x%x) (planes + planar padding)",
									*ula_size, *ula_size);
	return 0;
}


/* calculate UBWCP buffer parms */
static int ubwcp_calc_ubwcp_buf_params(struct ubwcp_driver *ubwcp,
					struct ubwcp_buffer_attrs *attr,
					size_t *md_p0, size_t *pd_p0,
					size_t *md_p1, size_t *pd_p1,
					size_t *stride_tp10_b)
{
	int planes;
	int missing_plane;
	enum ubwcp_std_image_format format;
	size_t stride_tp10_p;

	FENTRY();

	/* convert ioctl image format to standard image format */
	format = to_std_format(attr->image_format);
	missing_plane = missing_plane_from_format(attr->image_format);
	planes = planes_in_format(format); //pass in 0 (RGB) should return 1

	DBG_BUF_ATTR("ioctl_image_format : %d, std_format: %d", attr->image_format, format);
	DBG_BUF_ATTR("planes_in_format   : %d", planes);
	DBG_BUF_ATTR("missing_plane      : %d", missing_plane);

	if (!missing_plane) {
		*md_p0 = metadata_buf_sz(ubwcp, format, attr->width, attr->height, 0);
		*pd_p0 = pixeldata_buf_sz(ubwcp, format, attr->width, attr->height, 0);
		if (planes == 2) {
			*md_p1 = metadata_buf_sz(ubwcp, format, attr->width, attr->height, 1);
			*pd_p1 = pixeldata_buf_sz(ubwcp, format, attr->width, attr->height, 1);
		}
	} else {
		if (missing_plane == 1) {
			*md_p0 = 0;
			*pd_p0 = 0;
			*md_p1 = metadata_buf_sz(ubwcp, format, attr->width, attr->height, 1);
			*pd_p1 = pixeldata_buf_sz(ubwcp, format, attr->width, attr->height, 1);
		} else {
			*md_p0 = metadata_buf_sz(ubwcp, format, attr->width, attr->height, 0);
			*pd_p0 = pixeldata_buf_sz(ubwcp, format, attr->width, attr->height, 0);
			*md_p1 = 0;
			*pd_p1 = 0;
		}
	}

	if (format == TP10) {
		stride_tp10_p = UBWCP_ALIGN(attr->width, 192);
		*stride_tp10_b = (stride_tp10_p/3) + stride_tp10_p;
	} else {
		*stride_tp10_b = 0;
	}

	return 0;
}


/* reserve ULA address space of the given size */
static phys_addr_t ubwcp_ula_alloc(struct ubwcp_driver *ubwcp, size_t size)
{
	phys_addr_t pa;

	mutex_lock(&ubwcp->ula_lock);
	pa = gen_pool_alloc(ubwcp->ula_pool, size);
	DBG("addr: %p, size: %zx", pa, size);
	mutex_unlock(&ubwcp->ula_lock);
	return pa;
}


/* free ULA address space of the given address and size */
static void ubwcp_ula_free(struct ubwcp_driver *ubwcp, phys_addr_t pa, size_t size)
{
	mutex_lock(&ubwcp->ula_lock);
	if (!gen_pool_has_addr(ubwcp->ula_pool, pa, size)) {
		ERR("Attempt to free mem not from gen_pool: pa: %p, size: %zx", pa, size);
		goto err;
	}
	DBG("addr: %p, size: %zx", pa, size);
	gen_pool_free(ubwcp->ula_pool, pa, size);
	mutex_unlock(&ubwcp->ula_lock);
	return;

err:
	mutex_unlock(&ubwcp->ula_lock);
}


/* free up or expand current_pa and return the new pa */
static phys_addr_t ubwcp_ula_realloc(struct ubwcp_driver *ubwcp,
					phys_addr_t pa,
					size_t size,
					size_t new_size)
{
	if (size == new_size)
		return pa;

	if (pa)
		ubwcp_ula_free(ubwcp, pa, size);

	return ubwcp_ula_alloc(ubwcp, new_size);
}


/* unmap dma buf */
static void ubwcp_dma_unmap(struct ubwcp_buf *buf)
{
	FENTRY();
	if (buf->dma_buf && buf->attachment) {
		DBG("Calling dma_buf_unmap_attachment()");
		dma_buf_unmap_attachment(buf->attachment, buf->sgt, DMA_BIDIRECTIONAL);
		buf->sgt = NULL;
		dma_buf_detach(buf->dma_buf, buf->attachment);
		buf->attachment = NULL;
	}
}


/* dma map ubwcp buffer */
static int ubwcp_dma_map(struct ubwcp_buf *buf,
				struct device *dev,
				size_t iova_min_size,
				dma_addr_t *iova)
{
	int ret = 0;
	struct dma_buf *dma_buf = buf->dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	size_t dma_len;

	/* Map buffer to SMMU and get IOVA */
	attachment = dma_buf_attach(dma_buf, dev);
	if (IS_ERR(attachment)) {
		ret = PTR_ERR(attachment);
		ERR("dma_buf_attach() failed: %d", ret);
		goto err;
	}

	dma_set_max_seg_size(dev, DMA_BIT_MASK(32));
	dma_set_seg_boundary(dev, (unsigned long)DMA_BIT_MASK(64));

	sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(sgt)) {
		ret = PTR_ERR(sgt);
		ERR("dma_buf_map_attachment() failed: %d", ret);
		goto err_detach;
	}

	if (sgt->nents != 1) {
		ERR("nents = %d", sgt->nents);
		goto err_unmap;
	}

	/* ensure that dma_buf is big enough for the new attrs */
	dma_len = sg_dma_len(sgt->sgl);
	if (dma_len < iova_min_size) {
		ERR("dma len: %d is less than min ubwcp buffer size: %d",
							dma_len, iova_min_size);
		goto err_unmap;
	}

	*iova = sg_dma_address(sgt->sgl);
	buf->attachment = attachment;
	buf->sgt = sgt;
	return ret;

err_unmap:
	dma_buf_unmap_attachment(attachment, sgt, DMA_BIDIRECTIONAL);
err_detach:
	dma_buf_detach(dma_buf, attachment);
err:
	if (!ret)
		ret = -1;
	return ret;
}

static void
ubwcp_pixel_to_bytes(struct ubwcp_driver *ubwcp,
			enum ubwcp_std_image_format format,
			u32 width_p, u32 height_p,
			u32 *width_b, u32 *height_b)
{
	u16 pixel_bytes;
	u16 per_pixel;
	struct ubwcp_image_format_info f_info;
	struct ubwcp_plane_info p_info;

	f_info = ubwcp->format_info[format];
	p_info = f_info.p_info[0];

	pixel_bytes = p_info.pixel_bytes;
	per_pixel   = p_info.per_pixel;

	*width_b  = (width_p*pixel_bytes)/per_pixel;
	*height_b = (height_p*pixel_bytes)/per_pixel;
}

static void reset_buf_attrs(struct ubwcp_buf *buf)
{
	struct ubwcp_hw_meta_metadata *mmdata;
	struct ubwcp_driver *ubwcp;

	ubwcp = buf->ubwcp;
	mmdata = &buf->mmdata;

	ubwcp_dma_unmap(buf);

	/* reset ula params */
	if (buf->ula_size) {
		ubwcp_ula_free(ubwcp, buf->ula_pa, buf->ula_size);
		buf->ula_size = 0;
		buf->ula_pa = 0;
	}
	/* reset ubwcp params */
	memset(mmdata, 0, sizeof(*mmdata));
	buf->buf_attr_set = false;
}

static void print_mmdata_desc(struct ubwcp_hw_meta_metadata *mmdata)
{
	DBG_BUF_ATTR("");
	DBG_BUF_ATTR("--------MM_DATA DESC ---------");
	DBG_BUF_ATTR("uv_start_addr   : 0x%08llx (cache addr) (actual: 0x%llx)",
					mmdata->uv_start_addr, mmdata->uv_start_addr << 6);
	DBG_BUF_ATTR("format          : 0x%08x", mmdata->format);
	DBG_BUF_ATTR("stride          : 0x%08x (cache addr) (actual: 0x%x)",
					mmdata->stride, mmdata->stride << 6);
	DBG_BUF_ATTR("stride_ubwcp    : 0x%08x (cache addr) (actual: 0x%zx)",
					mmdata->stride_ubwcp, mmdata->stride_ubwcp << 6);
	DBG_BUF_ATTR("metadata_base_y : 0x%08x (page addr)  (actual: 0x%llx)",
					mmdata->metadata_base_y,  mmdata->metadata_base_y << 12);
	DBG_BUF_ATTR("metadata_base_uv: 0x%08x (page addr)  (actual: 0x%zx)",
					mmdata->metadata_base_uv, mmdata->metadata_base_uv << 12);
	DBG_BUF_ATTR("buffer_y_offset : 0x%08x (page addr)  (actual: 0x%zx)",
					mmdata->buffer_y_offset,  mmdata->buffer_y_offset << 12);
	DBG_BUF_ATTR("buffer_uv_offset: 0x%08x (page addr)  (actual: 0x%zx)",
					mmdata->buffer_uv_offset, mmdata->buffer_uv_offset << 12);
	DBG_BUF_ATTR("width_height    : 0x%08x (width: 0x%x height: 0x%x)",
		mmdata->width_height, mmdata->width_height >> 16, mmdata->width_height & 0xFFFF);
	DBG_BUF_ATTR("");
}

/* set buffer attributes:
 * Failure:
 * If a call to ubwcp_set_buf_attrs() fails, any attributes set from a previously
 * successful ubwcp_set_buf_attrs() will be also removed. Thus,
 * ubwcp_set_buf_attrs() implicitly does "unset previous attributes" and
 * then "try to set these new attributes".
 *
 * The result of a failed call to ubwcp_set_buf_attrs() will leave the buffer
 * in a linear mode, NOT with attributes from earlier successful call.
 */
int ubwcp_set_buf_attrs(struct dma_buf *dmabuf, struct ubwcp_buffer_attrs *attr)
{
	int ret = 0;
	size_t ula_size = 0;
	size_t uv_start_offset = 0;
	phys_addr_t ula_pa = 0x0;
	struct ubwcp_buf *buf;
	struct ubwcp_driver *ubwcp;

	size_t metadata_p0;
	size_t pixeldata_p0;
	size_t metadata_p1;
	size_t pixeldata_p1;
	size_t iova_min_size;
	size_t stride_tp10_b;
	dma_addr_t iova_base;
	struct ubwcp_hw_meta_metadata *mmdata;
	u64 uv_start;
	u32 stride_b;
	u32 width_b;
	u32 height_b;
	enum ubwcp_std_image_format std_image_format;

	FENTRY();

	if (!dmabuf) {
		ERR("NULL dmabuf input ptr");
		return -EINVAL;
	}

	if (!attr) {
		ERR("NULL attr ptr");
		return -EINVAL;
	}

	buf = dma_buf_to_ubwcp_buf(dmabuf);
	if (!buf) {
		ERR("No corresponding ubwcp_buf for the passed in dma_buf");
		return -EINVAL;
	}

	mutex_lock(&buf->lock);

	if (buf->locked) {
		ERR("Cannot set attr when buffer is locked");
		ret = -EBUSY;
		goto err;
	}

	ubwcp  = buf->ubwcp;
	mmdata = &buf->mmdata;

	//TBD: now that we have single exit point for all errors,
	//we can limit this call to error only?
	//also see if this can be part of reset_buf_attrs()
	DBG_BUF_ATTR("resetting mmap to linear");
	/* remove any earlier dma buf mmap configuration */
	ret = ubwcp->mmap_config_fptr(buf->dma_buf, true, 0, 0);
	if (ret) {
		ERR("dma_buf_mmap_config() failed: %d", ret);
		goto err;
	}

	if (!ubwcp_buf_attrs_valid(attr)) {
		ERR("Invalid buf attrs");
		goto err;
	}

	DBG_BUF_ATTR("valid buf attrs");

	if (attr->image_format == UBWCP_LINEAR) {
		DBG_BUF_ATTR("Linear format requested");
		/* linear format request with permanent range xlation doesn't
		 * make sense. need to define behavior if this happens.
		 * note: with perm set, desc is allocated to this buffer.
		 */
		//TBD: UBWCP_ASSERT(!buf->perm);

		if (buf->buf_attr_set)
			reset_buf_attrs(buf);

		mutex_unlock(&buf->lock);
		return 0;
	}

	std_image_format = to_std_format(attr->image_format);
	if (std_image_format == STD_IMAGE_FORMAT_INVALID) {
		ERR("Unable to map ioctl image format to std image format");
		goto err;
	}

	/* Calculate uncompressed-buffer size. */
	DBG_BUF_ATTR("");
	DBG_BUF_ATTR("");
	DBG_BUF_ATTR("Calculating ula params -->");
	ret = ubwcp_calc_ula_params(ubwcp, attr, &ula_size, &uv_start_offset);
	if (ret) {
		ERR("ubwcp_calc_ula_params() failed: %d", ret);
		goto err;
	}

	DBG_BUF_ATTR("");
	DBG_BUF_ATTR("");
	DBG_BUF_ATTR("Calculating ubwcp params -->");
	ret = ubwcp_calc_ubwcp_buf_params(ubwcp, attr,
						&metadata_p0, &pixeldata_p0,
						&metadata_p1, &pixeldata_p1,
						&stride_tp10_b);
	if (ret) {
		ERR("ubwcp_calc_buf_params() failed: %d", ret);
		goto err;
	}

	iova_min_size = metadata_p0 + pixeldata_p0 + metadata_p1 + pixeldata_p1;

	DBG_BUF_ATTR("");
	DBG_BUF_ATTR("");
	DBG_BUF_ATTR("------Summary ULA  Calculated Params ------");
	DBG_BUF_ATTR("ULA Size        : %8zu (0x%8zx)", ula_size, ula_size);
	DBG_BUF_ATTR("UV Start Offset : %8zu (0x%8zx)", uv_start_offset, uv_start_offset);
	DBG_BUF_ATTR("------Summary UBCP Calculated Params ------");
	DBG_BUF_ATTR("metadata_p0     : %8d (0x%8zx)", metadata_p0, metadata_p0);
	DBG_BUF_ATTR("pixeldata_p0    : %8d (0x%8zx)", pixeldata_p0, pixeldata_p0);
	DBG_BUF_ATTR("metadata_p1     : %8d (0x%8zx)", metadata_p1, metadata_p1);
	DBG_BUF_ATTR("pixeldata_p1    : %8d (0x%8zx)", pixeldata_p1, pixeldata_p1);
	DBG_BUF_ATTR("stride_tp10     : %8d (0x%8zx)", stride_tp10_b, stride_tp10_b);
	DBG_BUF_ATTR("iova_min_size   : %8d (0x%8zx)", iova_min_size, iova_min_size);
	DBG_BUF_ATTR("");

	if (buf->buf_attr_set) {
		/* if buf attr were previously set, these must not be 0 */
		/* TBD: do we need this check in production code? */
		if (!buf->ula_pa) {
			WARN(1, "ula_pa cannot be 0 if buf_attr_set is true!!!");
			goto err;
		}
		if (!buf->ula_size) {
			WARN(1, "ula_size cannot be 0 if buf_attr_set is true!!!");
			goto err;
		}
	}

	/* assign ULA PA with uncompressed-size range */
	ula_pa = ubwcp_ula_realloc(ubwcp, buf->ula_pa, buf->ula_size, ula_size);
	if (!ula_pa) {
		ERR("ubwcp_ula_alloc/realloc() failed. running out of ULA PA space?");
		goto err;
	}

	buf->ula_size = ula_size;
	buf->ula_pa   = ula_pa;
	DBG_BUF_ATTR("Allocated ULA_PA: 0x%p of size: 0x%zx", ula_pa, ula_size);
	DBG_BUF_ATTR("");

	/* inform ULA-PA to dma-heap: needed for dma-heap to do CMOs later on */
	DBG_BUF_ATTR("Calling mmap_config(): ULA_PA: 0x%p size: 0x%zx", ula_pa, ula_size);
	ret = ubwcp->mmap_config_fptr(buf->dma_buf, false, buf->ula_pa,
								buf->ula_size);
	if (ret) {
		ERR("dma_buf_mmap_config() failed: %d", ret);
		goto err;
	}

	/* dma map only the first time attribute is set */
	if (!buf->buf_attr_set) {
		/* linear -> ubwcp. map ubwcp buffer */
		ret = ubwcp_dma_map(buf, ubwcp->dev_buf_cb, iova_min_size, &iova_base);
		if (ret) {
			ERR("ubwcp_dma_map() failed: %d", ret);
			goto err;
		}
		DBG_BUF_ATTR("dma_buf IOVA range: 0x%llx + min_size (0x%zx): 0x%llx",
					iova_base, iova_min_size, iova_base + iova_min_size);
	}

	uv_start = ula_pa + uv_start_offset;
	if (!IS_ALIGNED(uv_start, 64)) {
		ERR("ERROR: uv_start is NOT aligned to cache line");
		goto err;
	}

	/* Convert height and width to bytes for writing to mmdata */
	if (std_image_format != TP10) {
		ubwcp_pixel_to_bytes(ubwcp, std_image_format, attr->width,
					attr->height, &width_b, &height_b);
	} else {
		/* for tp10 image compression, we need to program p010 width/height */
		ubwcp_pixel_to_bytes(ubwcp, P010, attr->width,
					attr->height, &width_b, &height_b);
	}

	stride_b = attr->stride;

	/* create the mmdata descriptor */
	memset(mmdata, 0, sizeof(*mmdata));
	mmdata->uv_start_addr = CACHE_ADDR(uv_start);
	mmdata->format        = ubwcp_get_hw_image_format_value(attr->image_format);

	if (std_image_format != TP10) {
		mmdata->stride       = CACHE_ADDR(stride_b);      /* uncompressed stride */
	} else {
		mmdata->stride       = CACHE_ADDR(stride_tp10_b); /*   compressed stride */
		mmdata->stride_ubwcp = CACHE_ADDR(stride_b);      /* uncompressed stride */
	}

	mmdata->metadata_base_y  = PAGE_ADDR(iova_base);
	mmdata->metadata_base_uv = PAGE_ADDR(iova_base + metadata_p0 + pixeldata_p0);
	mmdata->buffer_y_offset  = PAGE_ADDR(metadata_p0);
	mmdata->buffer_uv_offset = PAGE_ADDR(metadata_p1);

	/* NOTE: For version 1.1, both width & height needs to be in bytes.
	 * For other versions, width in bytes & height in pixels.
	 */
	if ((ubwcp->hw_ver_major == 1) && (ubwcp->hw_ver_minor == 1))
		mmdata->width_height = width_b << 16 | height_b;
	else
		mmdata->width_height = width_b << 16 | attr->height;

	print_mmdata_desc(mmdata);

	buf->buf_attr = *attr;
	buf->buf_attr_set = true;
	//TBD: UBWCP_ASSERT(!buf->perm);
	mutex_unlock(&buf->lock);
	return 0;

err:
	reset_buf_attrs(buf);
	mutex_unlock(&buf->lock);
	if (!ret)
		ret = -1;
	return ret;
}
EXPORT_SYMBOL(ubwcp_set_buf_attrs);


/* Set buffer attributes ioctl */
static int ubwcp_set_buf_attrs_ioctl(struct ubwcp_ioctl_buffer_attrs *attr_ioctl)
{
	struct dma_buf *dmabuf;

	dmabuf = ubwcp_dma_buf_fd_to_dma_buf(attr_ioctl->fd);

	return ubwcp_set_buf_attrs(dmabuf, &attr_ioctl->attr);
}


/* Free up the buffer descriptor */
static void ubwcp_buf_desc_free(struct ubwcp_driver *ubwcp, struct ubwcp_desc *desc)
{
	int idx = desc->idx;
	struct ubwcp_desc *desc_list = ubwcp->desc_list;

	mutex_lock(&ubwcp->desc_lock);
	desc_list[idx].idx = -1;
	desc_list[idx].ptr = NULL;
	DBG("freed descriptor_id: %d", idx);
	mutex_unlock(&ubwcp->desc_lock);
}


/* Allocate next available buffer descriptor. */
static struct ubwcp_desc *ubwcp_buf_desc_allocate(struct ubwcp_driver *ubwcp)
{
	int idx;
	struct ubwcp_desc *desc_list = ubwcp->desc_list;

	mutex_lock(&ubwcp->desc_lock);
	for (idx = 0; idx < UBWCP_BUFFER_DESC_COUNT; idx++) {
		if (desc_list[idx].idx == -1) {
			desc_list[idx].idx = idx;
			desc_list[idx].ptr = ubwcp->buffer_desc_base +
						idx*UBWCP_BUFFER_DESC_OFFSET;
			DBG("allocated descriptor_id: %d", idx);
			mutex_unlock(&ubwcp->desc_lock);
			return &desc_list[idx];
		}
	}
	mutex_unlock(&ubwcp->desc_lock);
	return NULL;
}

#define FLUSH_WA_SIZE		64
#define FLUSH_WA_UDELAY	89
void ubwcp_flush_cache_wa(struct device *dev, phys_addr_t paddr, size_t size)
{
	phys_addr_t cline = paddr;
	int num_line = size / FLUSH_WA_SIZE;
	int i;

	for (i = 0; i < num_line; i++) {
		dma_sync_single_for_cpu(dev, cline, FLUSH_WA_SIZE, 0);
		udelay(FLUSH_WA_UDELAY);
		cline += FLUSH_WA_SIZE;
	}
}

/**
 * Lock buffer for CPU access. This prepares ubwcp hw to allow
 * CPU access to the compressed buffer. It will perform
 * necessary address translation configuration and cache maintenance ops
 * so that CPU can safely access ubwcp buffer, if this call is
 * successful.
 * Allocate descriptor if not already,
 * perform CMO and then enable range check
 *
 * @param dmabuf : ptr to the dma buf
 * @param direction : direction of access
 *
 * @return int : 0 on success, otherwise error code
 */
static int ubwcp_lock(struct dma_buf *dmabuf, enum dma_data_direction dir)
{
	int ret = 0;
	struct ubwcp_buf *buf;
	struct ubwcp_driver *ubwcp;

	FENTRY();

	if (!dmabuf) {
		ERR("NULL dmabuf input ptr");
		return -EINVAL;
	}

	if (!valid_dma_direction(dir)) {
		ERR("invalid direction: %d", dir);
		return -EINVAL;
	}

	buf = dma_buf_to_ubwcp_buf(dmabuf);
	if (!buf) {
		ERR("ubwcp_buf ptr not found");
		return -1;
	}

	mutex_lock(&buf->lock);

	if (!buf->buf_attr_set) {
		ERR("lock() called on buffer, but attr not set");
		goto err;
	}

	if (buf->buf_attr.image_format == UBWCP_LINEAR) {
		ERR("lock() called on linear buffer");
		goto err;
	}

	if (!buf->locked) {
		DBG("first lock on buffer");
		ubwcp = buf->ubwcp;

		/* buf->desc could already be allocated because of perm range xlation */
		if (!buf->desc) {
			/* allocate a buffer descriptor */
			buf->desc = ubwcp_buf_desc_allocate(buf->ubwcp);
			if (!buf->desc) {
				ERR("ubwcp_allocate_buf_desc() failed");
				goto err;
			}

			memcpy(buf->desc->ptr, &buf->mmdata, sizeof(buf->mmdata));

			/* Flushing of updated mmdata:
			 * mmdata is iocoherent and ubwcp will get it from CPU cache -
			 * *as long as* it has not cached that itself during previous
			 * access to the same descriptor.
			 *
			 * During unlock of previous use of this descriptor,
			 * we do hw flush, which will get rid of this mmdata from
			 * ubwcp cache.
			 *
			 * In addition, we also do a hw flush after enable_range_ck().
			 * That will also get rid of any speculative fetch of mmdata
			 * by the ubwcp hw. At this time, the assumption is that ubwcp
			 * will cache mmdata only for active descriptor. But if ubwcp
			 * is speculatively fetching mmdata for all descriptors
			 * (irrespetive of enabled or not), the flush during lock
			 * will be necessary to make sure ubwcp sees updated mmdata
			 * that we just updated
			 */

			/* program ULA range for this buffer */
			DBG("setting range check: descriptor_id: %d, addr: %p, size: %zx",
							buf->desc->idx, buf->ula_pa, buf->ula_size);
			ubwcp_hw_set_range_check(ubwcp->base, buf->desc->idx, buf->ula_pa,
										buf->ula_size);
		}


		/* enable range check */
		DBG("enabling range check, descriptor_id: %d", buf->desc->idx);
		mutex_lock(&ubwcp->hw_range_ck_lock);
		ubwcp_hw_enable_range_check(ubwcp->base, buf->desc->idx);
		mutex_unlock(&ubwcp->hw_range_ck_lock);

		/* Flush/invalidate UBWCP caches */
		/* Why: cpu could have done a speculative fetch before
		 * enable_range_ck() and ubwcp in process of returning "default" data
		 * we don't want that stashing of default data pending.
		 * we force completion of that and then we also cpu invalidate which
		 * will get rid of that line.
		 */
		ubwcp_flush(ubwcp);

		/* Flush/invalidate ULA PA from CPU caches
		 * TBD: if (dir == READ or BIDIRECTION) //NOT for write
		 * -- Confirm with Chris if this can be skipped for write
		 */
		dma_sync_single_for_cpu(ubwcp->dev, buf->ula_pa, buf->ula_size, dir);
		buf->lock_dir = dir;
		buf->locked = true;
	} else {
		DBG("buf already locked");
		/* TBD: what if new buffer direction is not same as previous?
		 * must update the dir.
		 */
	}
	buf->lock_count++;
	DBG("new lock_count: %d", buf->lock_count);
	mutex_unlock(&buf->lock);
	return ret;

err:
	mutex_unlock(&buf->lock);
	if (!ret)
		ret = -1;
	return ret;
}

/* This can be called as a result of external unlock() call or
 * internally if free() is called without unlock().
 * It can fail only for 1 reason: ubwcp_flush fails. currently we are ignoring the flush failure
 * because it is hardware failure and no recovery path is defined.
 */
static int unlock_internal(struct ubwcp_buf *buf, enum dma_data_direction dir, bool free_buffer)
{
	int ret = 0;
	struct ubwcp_driver *ubwcp;

	DBG("current lock_count: %d", buf->lock_count);
	if (free_buffer) {
		buf->lock_count = 0;
		DBG("Forced lock_count: %d", buf->lock_count);
	} else {
		buf->lock_count--;
		DBG("new lock_count: %d", buf->lock_count);
		if (buf->lock_count) {
			DBG("more than 1 lock on buffer. waiting until last unlock");
			return 0;
		}
	}

	ubwcp = buf->ubwcp;

	/* Flush/invalidate ULA PA from CPU caches */
	//TBD: if (dir == WRITE or BIDIRECTION)
	//dma_sync_single_for_device(ubwcp->dev, buf->ula_pa, buf->ula_size, dir);
	/* TODO: Use flush work around, remove when no longer needed */
	ubwcp_flush_cache_wa(ubwcp->dev, buf->ula_pa, buf->ula_size);

	/* disable range check with ubwcp flush */
	DBG("disabling range check");
	//TBD: could combine these 2 locks into a single lock to make it simpler
	mutex_lock(&ubwcp->ubwcp_flush_lock);
	mutex_lock(&ubwcp->hw_range_ck_lock);
	ret = ubwcp_hw_disable_range_check_with_flush(ubwcp->base, buf->desc->idx);
	if (ret)
		ERR("disable_range_check_with_flush() failed: %d", ret);
	mutex_unlock(&ubwcp->hw_range_ck_lock);
	mutex_unlock(&ubwcp->ubwcp_flush_lock);

	/* release descriptor if perm range xlation is not set */
	if (!buf->perm) {
		ubwcp_buf_desc_free(buf->ubwcp, buf->desc);
		buf->desc = NULL;
	}
	buf->locked = false;
	return ret;
}


/**
 * Unlock buffer from CPU access. This prepares ubwcp hw to
 * safely allow for device access to the compressed buffer including any
 * necessary cache maintenance ops. It may also free up certain ubwcp
 * resources that could result in error when accessed by CPU in
 * unlocked state.
 *
 * @param dmabuf : ptr to the dma buf
 * @param direction : direction of access
 *
 * @return int : 0 on success, otherwise error code
 */
static int ubwcp_unlock(struct dma_buf *dmabuf, enum dma_data_direction dir)
{
	struct ubwcp_buf *buf;
	int ret;

	FENTRY();

	if (!dmabuf) {
		ERR("NULL dmabuf input ptr");
		return -EINVAL;
	}

	if (!valid_dma_direction(dir)) {
		ERR("invalid direction: %d", dir);
		return -EINVAL;
	}

	buf = dma_buf_to_ubwcp_buf(dmabuf);
	if (!buf) {
		ERR("ubwcp_buf not found");
		return -1;
	}

	if (!buf->locked) {
		ERR("unlock() called on buffer which not in locked state");
		return -1;
	}

	error_print_count = 0;
	mutex_lock(&buf->lock);
	ret = unlock_internal(buf, dir, false);
	mutex_unlock(&buf->lock);
	return ret;
}


/* Return buffer attributes for the given buffer */
int ubwcp_get_buf_attrs(struct dma_buf *dmabuf, struct ubwcp_buffer_attrs *attr)
{
	int ret = 0;
	struct ubwcp_buf *buf;

	FENTRY();

	if (!dmabuf) {
		ERR("NULL dmabuf input ptr");
		return -EINVAL;
	}

	if (!attr) {
		ERR("NULL attr ptr");
		return -EINVAL;
	}

	buf = dma_buf_to_ubwcp_buf(dmabuf);
	if (!buf) {
		ERR("ubwcp_buf ptr not found");
		return -1;
	}

	mutex_lock(&buf->lock);
	if (!buf->buf_attr_set) {
		ERR("buffer attributes not set");
		mutex_unlock(&buf->lock);
		return -1;
	}

	*attr = buf->buf_attr;

	mutex_unlock(&buf->lock);
	return ret;
}
EXPORT_SYMBOL(ubwcp_get_buf_attrs);


/* Set permanent range translation.
 * enable: Descriptor will be reserved for this buffer until disabled,
 *         making lock/unlock quicker.
 * disable: Descriptor will not be reserved for this buffer. Instead,
 *          descriptor will be allocated and released for each lock/unlock.
 *          If currently allocated but not being used, descriptor will be
 *          released.
 */
int ubwcp_set_perm_range_translation(struct dma_buf *dmabuf, bool enable)
{
	int ret = 0;
	struct ubwcp_buf *buf;

	FENTRY();

	if (!dmabuf) {
		ERR("NULL dmabuf input ptr");
		return -EINVAL;
	}

	buf = dma_buf_to_ubwcp_buf(dmabuf);
	if (!buf) {
		ERR("ubwcp_buf not found");
		return -1;
	}

	/* not implemented */
	if (1) {
		ERR("API not implemented yet");
		return -1;
	}

	/* TBD: make sure we acquire buf lock while setting this so there is
	 * no race condition with attr_set/lock/unlock
	 */
	buf->perm = enable;

	/* if "disable" and we have allocated a desc and it is not being
	 * used currently, release it
	 */
	if (!enable && buf->desc && !buf->locked) {
		ubwcp_buf_desc_free(buf->ubwcp, buf->desc);
		buf->desc = NULL;

		/* Flush/invalidate UBWCP caches */
		//TBD: need to do anything?
	}

	return ret;
}
EXPORT_SYMBOL(ubwcp_set_perm_range_translation);

/**
 * Free up ubwcp resources for this buffer.
 *
 * @param dmabuf : ptr to the dma buf
 *
 * @return int : 0 on success, otherwise error code
 */
static int ubwcp_free_buffer(struct dma_buf *dmabuf)
{
	int ret = 0;
	struct ubwcp_buf *buf;
	struct ubwcp_driver *ubwcp;

	FENTRY();

	if (!dmabuf) {
		ERR("NULL dmabuf input ptr");
		return -EINVAL;
	}

	buf = dma_buf_to_ubwcp_buf(dmabuf);
	if (!buf) {
		ERR("ubwcp_buf ptr not found");
		return -1;
	}

	mutex_lock(&buf->lock);
	ubwcp = buf->ubwcp;

	if (buf->locked) {
		DBG("free() called without unlock. unlock()'ing first...");
		ret = unlock_internal(buf, buf->lock_dir, true);
		if (ret)
			ERR("unlock_internal(): failed : %d, but continuing free()", ret);
	}

	/* if we are still holding a desc, release it. this can happen only if perm == true */
	if (buf->desc) {
		WARN_ON(!buf->perm); /* TBD: change to BUG() later...*/
		ubwcp_buf_desc_free(buf->ubwcp, buf->desc);
		buf->desc = NULL;
	}

	if (buf->buf_attr_set)
		reset_buf_attrs(buf);

	mutex_lock(&ubwcp->buf_table_lock);
	hash_del(&buf->hnode);
	kfree(buf);

	/* If this is the last buffer being freed, power off ubwcp */
	if (hash_empty(ubwcp->buf_table)) {
		DBG("last buffer: ~~~~~~~~~~~");
		/* TBD: If everything is working fine, ubwcp_flush() should not
		 * be needed here. Each buffer free logic should be taking
		 * care of flush. Just a note for now. Might need to add the
		 * flush here for debug purpose.
		 */
		DBG("Calling offline_and_remove_memory() for ULA PA pool");
		ret = offline_and_remove_memory(ubwcp->ula_pool_base,
				ubwcp->ula_pool_size);
		if (ret) {
			ERR("offline_and_remove_memory failed st:0x%lx sz:0x%lx err: %d",
				ubwcp->ula_pool_base,
				ubwcp->ula_pool_size, ret);
			goto err_remove_mem;
		} else {
			DBG("DONE: calling offline_and_remove_memory() for ULA PA pool");
		}
		DBG("Don't Call power OFF ...");
	}
	mutex_unlock(&ubwcp->buf_table_lock);
	return ret;

err_remove_mem:
	mutex_unlock(&ubwcp->buf_table_lock);
	if (!ret)
		ret = -1;
	DBG("returning error: %d", ret);
	return ret;
}


/* file open: TBD: increment ref count? */
static int ubwcp_open(struct inode *i, struct file *f)
{
	return 0;
}


/* file open: TBD: decrement ref count? */
static int ubwcp_close(struct inode *i, struct file *f)
{
	return 0;
}


/* handle IOCTLs */
static long ubwcp_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
	struct ubwcp_ioctl_buffer_attrs buf_attr_ioctl;
	struct ubwcp_ioctl_hw_version hw_ver;

	switch (ioctl_num) {
	case UBWCP_IOCTL_SET_BUF_ATTR:
		if (copy_from_user(&buf_attr_ioctl, (const void __user *) ioctl_param,
				   sizeof(buf_attr_ioctl))) {
			ERR("ERROR: copy_from_user() failed");
			return -EFAULT;
		}
		DBG("IOCTL : SET_BUF_ATTR: fd = %d", buf_attr_ioctl.fd);
		return ubwcp_set_buf_attrs_ioctl(&buf_attr_ioctl);

	case UBWCP_IOCTL_GET_HW_VER:
		DBG("IOCTL : GET_HW_VER");
		ubwcp_get_hw_version(&hw_ver);
		if (copy_to_user((void __user *)ioctl_param, &hw_ver, sizeof(hw_ver))) {
			ERR("ERROR: copy_to_user() failed");
			return -EFAULT;
		}
		break;

	default:
		ERR("Invalid ioctl_num = %d", ioctl_num);
		return -EINVAL;
	}
	return 0;
}


static const struct file_operations ubwcp_fops = {
	.owner = THIS_MODULE,
	.open           = ubwcp_open,
	.release        = ubwcp_close,
	.unlocked_ioctl = ubwcp_ioctl,
};


static int ubwcp_debugfs_init(struct ubwcp_driver *ubwcp)
{
	struct dentry *debugfs_root;

	debugfs_root = debugfs_create_dir("ubwcp", NULL);
	if (!debugfs_root) {
		pr_warn("Failed to create debugfs for ubwcp\n");
		return -1;
	}

	debugfs_create_u32("debug_trace_enable", 0644, debugfs_root, &ubwcp_debug_trace_enable);

	ubwcp->debugfs_root = debugfs_root;
	return 0;
}

static void ubwcp_debugfs_deinit(struct ubwcp_driver *ubwcp)
{
	debugfs_remove_recursive(ubwcp->debugfs_root);
}

/* ubwcp char device initialization */
static int ubwcp_cdev_init(struct ubwcp_driver *ubwcp)
{
	int ret;
	dev_t devt;
	struct class *dev_class;
	struct device *dev_sys;

	/* allocate major device number (/proc/devices -> major_num ubwcp) */
	ret = alloc_chrdev_region(&devt, 0, UBWCP_NUM_DEVICES, UBWCP_DEVICE_NAME);
	if (ret) {
		ERR("alloc_chrdev_region() failed: %d", ret);
		return ret;
	}

	/* create device class  (/sys/class/ubwcp_class) */
	dev_class = class_create(THIS_MODULE, "ubwcp_class");
	if (IS_ERR(dev_class)) {
		ERR("class_create() failed");
		return -1;
	}

	/* Create device and register with sysfs
	 * (/sys/class/ubwcp_class/ubwcp/... -> dev/power/subsystem/uevent)
	 */
	dev_sys = device_create(dev_class, NULL, devt, NULL,
			UBWCP_DEVICE_NAME);
	if (IS_ERR(dev_sys)) {
		ERR("device_create() failed");
		return -1;
	}

	/* register file operations and get cdev */
	cdev_init(&ubwcp->cdev, &ubwcp_fops);

	/* associate cdev and device major/minor with file system
	 * can do file ops on /dev/ubwcp after this
	 */
	ret = cdev_add(&ubwcp->cdev, devt, 1);
	if (ret) {
		ERR("cdev_add() failed");
		return -1;
	}

	ubwcp->devt = devt;
	ubwcp->dev_class = dev_class;
	ubwcp->dev_sys = dev_sys;
	return 0;
}

static void ubwcp_cdev_deinit(struct ubwcp_driver *ubwcp)
{
	device_destroy(ubwcp->dev_class, ubwcp->devt);
	class_destroy(ubwcp->dev_class);
	cdev_del(&ubwcp->cdev);
	unregister_chrdev_region(ubwcp->devt, UBWCP_NUM_DEVICES);
}


#define ERR_PRINT_COUNT_MAX 21
/* TBD: use proper rate limit for debug prints */
irqreturn_t ubwcp_irq_handler(int irq, void *ptr)
{
	struct ubwcp_driver *ubwcp;
	void __iomem *base;
	u64 src;

	error_print_count++;

	ubwcp = (struct ubwcp_driver *) ptr;
	base = ubwcp->base;

	if (irq == ubwcp->irq_range_ck_rd) {
		if (error_print_count < ERR_PRINT_COUNT_MAX) {
			src = ubwcp_hw_interrupt_src_address(base, 0);
			ERR("check range read error: src: 0x%llx", src << 6);
		}
		ubwcp_hw_interrupt_clear(ubwcp->base, 0);
	} else if (irq == ubwcp->irq_range_ck_wr) {
		if (error_print_count < ERR_PRINT_COUNT_MAX) {
			src = ubwcp_hw_interrupt_src_address(base, 1);
			ERR("check range write error: src: 0x%llx", src << 6);
		}
		ubwcp_hw_interrupt_clear(ubwcp->base, 1);
	} else if (irq == ubwcp->irq_encode) {
		if (error_print_count < ERR_PRINT_COUNT_MAX) {
			src = ubwcp_hw_interrupt_src_address(base, 3);
			ERR("encode error: src: 0x%llx", src << 6);
		}
		ubwcp_hw_interrupt_clear(ubwcp->base, 3); //TBD: encode is bit-3 instead of bit-2
	} else if (irq == ubwcp->irq_decode) {
		if (error_print_count < ERR_PRINT_COUNT_MAX) {
			src = ubwcp_hw_interrupt_src_address(base, 2);
			ERR("decode error: src: 0x%llx", src << 6);
		}
		ubwcp_hw_interrupt_clear(ubwcp->base, 2); //TBD: decode is bit-2 instead of bit-3
	} else {
		ERR("unknown irq: %d", irq);
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

static int ubwcp_interrupt_register(struct platform_device *pdev, struct ubwcp_driver *ubwcp)
{
	int ret = 0;
	struct device *dev = &pdev->dev;

	FENTRY();

	ubwcp->irq_range_ck_rd = platform_get_irq(pdev, 0);
	if (ubwcp->irq_range_ck_rd < 0)
		return ubwcp->irq_range_ck_rd;

	ubwcp->irq_range_ck_wr = platform_get_irq(pdev, 1);
	if (ubwcp->irq_range_ck_wr < 0)
		return ubwcp->irq_range_ck_wr;

	ubwcp->irq_encode = platform_get_irq(pdev, 2);
	if (ubwcp->irq_encode < 0)
		return ubwcp->irq_encode;

	ubwcp->irq_decode = platform_get_irq(pdev, 3);
	if (ubwcp->irq_decode < 0)
		return ubwcp->irq_decode;

	DBG("got irqs: %d %d %d %d", ubwcp->irq_range_ck_rd,
					ubwcp->irq_range_ck_wr,
					ubwcp->irq_encode,
					ubwcp->irq_decode);

	ret = devm_request_irq(dev, ubwcp->irq_range_ck_rd, ubwcp_irq_handler, 0, "ubwcp", ubwcp);
	if (ret) {
		ERR("request_irq() failed. irq: %d ret: %d",
						ubwcp->irq_range_ck_rd, ret);
		return ret;
	}

	ret = devm_request_irq(dev, ubwcp->irq_range_ck_wr, ubwcp_irq_handler, 0, "ubwcp", ubwcp);
	if (ret) {
		ERR("request_irq() failed. irq: %d ret: %d",
						ubwcp->irq_range_ck_wr, ret);
		return ret;
	}

	ret = devm_request_irq(dev, ubwcp->irq_encode, ubwcp_irq_handler, 0, "ubwcp", ubwcp);
	if (ret) {
		ERR("request_irq() failed. irq: %d ret: %d",
							ubwcp->irq_encode, ret);
		return ret;
	}

	ret = devm_request_irq(dev, ubwcp->irq_decode, ubwcp_irq_handler, 0, "ubwcp", ubwcp);
	if (ret) {
		ERR("request_irq() failed. irq: %d ret: %d",
							ubwcp->irq_decode, ret);
		return ret;
	}

	return ret;
}

/* ubwcp device probe */
static int qcom_ubwcp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct ubwcp_driver *ubwcp;
	struct device *ubwcp_dev = &pdev->dev;

	FENTRY();

	ubwcp = devm_kzalloc(ubwcp_dev, sizeof(*ubwcp), GFP_KERNEL);
	if (!ubwcp) {
		ERR("devm_kzalloc() failed");
		return -ENOMEM;
	}

	ubwcp->dev = &pdev->dev;

	ret = dma_set_mask_and_coherent(ubwcp->dev, DMA_BIT_MASK(64));

#ifdef UBWCP_USE_SMC
	{
		struct resource res;

		of_address_to_resource(ubwcp_dev->of_node, 0, &res);
		ubwcp->base = (void __iomem *) res.start;
		DBG("Using SMC calls. base: %p", ubwcp->base);
	}
#else
	ubwcp->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ubwcp->base)) {
		ERR("devm ioremap() failed: %d", PTR_ERR(ubwcp->base));
		return PTR_ERR(ubwcp->base);
	}
	DBG("ubwcp->base: %p", ubwcp->base);
#endif

	ret = of_property_read_u64_index(ubwcp_dev->of_node, "ula_range", 0, &ubwcp->ula_pool_base);
	if (ret) {
		ERR("failed reading ula_range (base): %d", ret);
		return ret;
	}
	DBG("ubwcp: ula_range: base = 0x%lx", ubwcp->ula_pool_base);

	ret = of_property_read_u64_index(ubwcp_dev->of_node, "ula_range", 1, &ubwcp->ula_pool_size);
	if (ret) {
		ERR("failed reading ula_range (size): %d", ret);
		return ret;
	}
	DBG("ubwcp: ula_range: size = 0x%lx", ubwcp->ula_pool_size);

	/*TBD: remove later. reducing size for quick testing...*/
	ubwcp->ula_pool_size = 0x20000000; //500MB instead of 8GB

	if (ubwcp_interrupt_register(pdev, ubwcp))
		return -1;

	/* Regulator */
	ubwcp->vdd = devm_regulator_get(ubwcp_dev, "vdd");
	if (IS_ERR_OR_NULL(ubwcp->vdd)) {
		ret = PTR_ERR(ubwcp->vdd);
		ERR("devm_regulator_get() failed: %d", ret);
		return -1;
	}

	mutex_init(&ubwcp->desc_lock);
	mutex_init(&ubwcp->buf_table_lock);
	mutex_init(&ubwcp->ula_lock);
	mutex_init(&ubwcp->ubwcp_flush_lock);
	mutex_init(&ubwcp->hw_range_ck_lock);


	if (ubwcp_power(ubwcp, true))
		return -1;

	if (ubwcp_cdev_init(ubwcp))
		return -1;

	if (ubwcp_debugfs_init(ubwcp))
		return -1;

	/* create ULA pool */
	ubwcp->ula_pool = gen_pool_create(12, -1);
	if (!ubwcp->ula_pool) {
		ERR("failed gen_pool_create()");
		ret = -1;
		goto err_pool_create;
	}

	ret = gen_pool_add(ubwcp->ula_pool, ubwcp->ula_pool_base, ubwcp->ula_pool_size, -1);
	if (ret) {
		ERR("failed gen_pool_add(): %d", ret);
		ret = -1;
		goto err_pool_add;
	}

	/* register the default config mmap function. */
	ubwcp->mmap_config_fptr = msm_ubwcp_dma_buf_configure_mmap;

	hash_init(ubwcp->buf_table);
	ubwcp_buf_desc_list_init(ubwcp);
	image_format_init(ubwcp);

	/* one time hw init */
	ubwcp_hw_one_time_init(ubwcp->base);
	ubwcp_hw_version(ubwcp->base, &ubwcp->hw_ver_major, &ubwcp->hw_ver_minor);
	pr_err("ubwcp: hw version: major %d, minor %d\n", ubwcp->hw_ver_major, ubwcp->hw_ver_minor);
	if (ubwcp->hw_ver_major == 0) {
		ERR("Failed to read HW version");
		ret = -1;
		goto err_pool_add;
	}

	/* set pdev->dev->driver_data = ubwcp */
	platform_set_drvdata(pdev, ubwcp);

	/* enable all 4 interrupts */
	ubwcp_hw_interrupt_enable(ubwcp->base, INTERRUPT_READ_ERROR,   true);
	ubwcp_hw_interrupt_enable(ubwcp->base, INTERRUPT_WRITE_ERROR,  true);
	ubwcp_hw_interrupt_enable(ubwcp->base, INTERRUPT_ENCODE_ERROR, true);
	ubwcp_hw_interrupt_enable(ubwcp->base, INTERRUPT_DECODE_ERROR, true);

	/* Turn OFF until buffers are allocated */
	if (ubwcp_power(ubwcp, false)) {
		ret = -1;
		goto err_power_off;
	}

	ret = msm_ubwcp_set_ops(ubwcp_init_buffer, ubwcp_free_buffer, ubwcp_lock, ubwcp_unlock);
	if (ret) {
		ERR("msm_ubwcp_set_ops() failed: %d, but IGNORED", ret);
		/* TBD: ignore return error during testing phase.
		 * This allows us to rmmod/insmod for faster dev cycle.
		 * In final version: return error and de-register driver if set_ops fails.
		 */
		ret = 0;
		//goto err_power_off;
	} else {
		DBG("msm_ubwcp_set_ops(): success"); }

	me = ubwcp;
	return ret;

err_power_off:
	ubwcp_hw_interrupt_enable(ubwcp->base, INTERRUPT_READ_ERROR,   false);
	ubwcp_hw_interrupt_enable(ubwcp->base, INTERRUPT_WRITE_ERROR,  false);
	ubwcp_hw_interrupt_enable(ubwcp->base, INTERRUPT_ENCODE_ERROR, false);
	ubwcp_hw_interrupt_enable(ubwcp->base, INTERRUPT_DECODE_ERROR, false);
err_pool_add:
	gen_pool_destroy(ubwcp->ula_pool);
err_pool_create:
	ubwcp_cdev_deinit(ubwcp);
	return ret;
}


/* buffer context bank device probe */
static int ubwcp_probe_cb_buf(struct platform_device *pdev)
{
	struct ubwcp_driver *ubwcp;

	FENTRY();

	ubwcp = dev_get_drvdata(pdev->dev.parent);
	if (!ubwcp) {
		ERR("failed to get ubwcp ptr");
		return -EINVAL;
	}

	/* save the buffer cb device */
	ubwcp->dev_buf_cb = &pdev->dev;
	return 0;
}

/* descriptor context bank device probe */
static int ubwcp_probe_cb_desc(struct platform_device *pdev)
{
	int ret = 0;
	struct ubwcp_driver *ubwcp;

	FENTRY();

	ubwcp = dev_get_drvdata(pdev->dev.parent);
	if (!ubwcp) {
		ERR("failed to get ubwcp ptr");
		return -EINVAL;
	}

	ubwcp->buffer_desc_size = UBWCP_BUFFER_DESC_OFFSET *
					UBWCP_BUFFER_DESC_COUNT;

	ubwcp->dev_desc_cb = &pdev->dev;

	dma_set_max_seg_size(ubwcp->dev_desc_cb, DMA_BIT_MASK(32));
	dma_set_seg_boundary(ubwcp->dev_desc_cb, (unsigned long)DMA_BIT_MASK(64));

	/* Allocate buffer descriptors. UBWCP is iocoherent device.
	 * Thus we don't need to flush after updates to buffer descriptors.
	 */
	ubwcp->buffer_desc_base = dma_alloc_coherent(ubwcp->dev_desc_cb,
					ubwcp->buffer_desc_size,
					&ubwcp->buffer_desc_dma_handle,
					GFP_KERNEL);
	if (!ubwcp->buffer_desc_base) {
		ERR("failed to allocate desc buffer");
		return -ENOMEM;
	}

	DBG("desc_base = %p size = %zu", ubwcp->buffer_desc_base,
						ubwcp->buffer_desc_size);

	ret = ubwcp_power(ubwcp, true);
	if (ret) {
		ERR("failed to power on");
		goto err;
	}
	ubwcp_hw_set_buf_desc(ubwcp->base, (u64) ubwcp->buffer_desc_dma_handle,
						UBWCP_BUFFER_DESC_OFFSET);

	ret = ubwcp_power(ubwcp, false);
	if (ret) {
		ERR("failed to power off");
		goto err;
	}

	return ret;

err:
	dma_free_coherent(ubwcp->dev_desc_cb,
				ubwcp->buffer_desc_size,
				ubwcp->buffer_desc_base,
				ubwcp->buffer_desc_dma_handle);
	ubwcp->buffer_desc_base = NULL;
	ubwcp->buffer_desc_dma_handle = 0;
	ubwcp->dev_desc_cb = NULL;
	return -1;
}

/* buffer context bank device remove */
static int ubwcp_remove_cb_buf(struct platform_device *pdev)
{
	struct ubwcp_driver *ubwcp;

	FENTRY();

	ubwcp = dev_get_drvdata(pdev->dev.parent);
	if (!ubwcp) {
		ERR("failed to get ubwcp ptr");
		return -EINVAL;
	}

	/* remove buf_cb reference */
	ubwcp->dev_buf_cb = NULL;
	return 0;
}

/* descriptor context bank device remove */
static int ubwcp_remove_cb_desc(struct platform_device *pdev)
{
	struct ubwcp_driver *ubwcp;

	FENTRY();

	ubwcp = dev_get_drvdata(pdev->dev.parent);
	if (!ubwcp) {
		ERR("failed to get ubwcp ptr");
		return -EINVAL;
	}

	if (!ubwcp->dev_desc_cb) {
		ERR("ubwcp->dev_desc_cb == NULL");
		return -1;
	}

	ubwcp_power(ubwcp, true);
	ubwcp_hw_set_buf_desc(ubwcp->base, 0x0, 0x0);
	ubwcp_power(ubwcp, false);

	dma_free_coherent(ubwcp->dev_desc_cb,
				ubwcp->buffer_desc_size,
				ubwcp->buffer_desc_base,
				ubwcp->buffer_desc_dma_handle);
	ubwcp->buffer_desc_base = NULL;
	ubwcp->buffer_desc_dma_handle = 0;
	return 0;
}

/* ubwcp device remove */
static int qcom_ubwcp_remove(struct platform_device *pdev)
{
	size_t avail;
	size_t psize;
	struct ubwcp_driver *ubwcp;

	FENTRY();

	/* get pdev->dev->driver_data = ubwcp */
	ubwcp = platform_get_drvdata(pdev);
	if (!ubwcp) {
		ERR("ubwcp == NULL");
		return -1;
	}

	ubwcp_power(ubwcp, true);
	ubwcp_hw_interrupt_enable(ubwcp->base, INTERRUPT_READ_ERROR,   false);
	ubwcp_hw_interrupt_enable(ubwcp->base, INTERRUPT_WRITE_ERROR,  false);
	ubwcp_hw_interrupt_enable(ubwcp->base, INTERRUPT_ENCODE_ERROR, false);
	ubwcp_hw_interrupt_enable(ubwcp->base, INTERRUPT_DECODE_ERROR, false);
	ubwcp_power(ubwcp, false);

	/* before destroying, make sure pool is empty. otherwise pool_destroy() panics.
	 * TBD: remove this check for production code and let it panic
	 */
	avail = gen_pool_avail(ubwcp->ula_pool);
	psize = gen_pool_size(ubwcp->ula_pool);
	if (psize != avail) {
		ERR("gen_pool is not empty! avail: %zx size: %zx", avail, psize);
		ERR("skipping pool destroy....cause it will PANIC. Fix this!!!!");
		WARN(1, "Fix this!");
	} else {
		gen_pool_destroy(ubwcp->ula_pool);
	}
	ubwcp_debugfs_deinit(ubwcp);
	ubwcp_cdev_deinit(ubwcp);

	return 0;
}


/* top level ubwcp device probe function */
static int ubwcp_probe(struct platform_device *pdev)
{
	const char *compatible = "";

	FENTRY();

	if (of_device_is_compatible(pdev->dev.of_node, "qcom,ubwcp"))
		return qcom_ubwcp_probe(pdev);
	else if (of_device_is_compatible(pdev->dev.of_node, "qcom,ubwcp-context-bank-desc"))
		return ubwcp_probe_cb_desc(pdev);
	else if (of_device_is_compatible(pdev->dev.of_node, "qcom,ubwcp-context-bank-buf"))
		return ubwcp_probe_cb_buf(pdev);

	of_property_read_string(pdev->dev.of_node, "compatible", &compatible);
	ERR("unknown device: %s", compatible);

	WARN_ON(1);
	return -EINVAL;
}

/* top level ubwcp device remove function */
static int ubwcp_remove(struct platform_device *pdev)
{
	const char *compatible = "";

	FENTRY();

	/* TBD: what if buffers are still allocated? locked? etc.
	 *  also should turn off power?
	 */

	if (of_device_is_compatible(pdev->dev.of_node, "qcom,ubwcp"))
		return qcom_ubwcp_remove(pdev);
	else if (of_device_is_compatible(pdev->dev.of_node, "qcom,ubwcp-context-bank-desc"))
		return ubwcp_remove_cb_desc(pdev);
	else if (of_device_is_compatible(pdev->dev.of_node, "qcom,ubwcp-context-bank-buf"))
		return ubwcp_remove_cb_buf(pdev);

	of_property_read_string(pdev->dev.of_node, "compatible", &compatible);
	ERR("unknown device: %s", compatible);

	WARN_ON(1);
	return -EINVAL;
}


static const struct of_device_id ubwcp_dt_match[] = {
	{.compatible = "qcom,ubwcp"},
	{.compatible = "qcom,ubwcp-context-bank-desc"},
	{.compatible = "qcom,ubwcp-context-bank-buf"},
	{}
};

struct platform_driver ubwcp_platform_driver = {
	.probe = ubwcp_probe,
	.remove = ubwcp_remove,
	.driver = {
		.name = "qcom,ubwcp",
		.of_match_table = ubwcp_dt_match,
	},
};

int ubwcp_init(void)
{
	int ret = 0;

	DBG("+++++++++++");

	ret = platform_driver_register(&ubwcp_platform_driver);
	if (ret)
		ERR("platform_driver_register() failed: %d", ret);

	return ret;
}

void ubwcp_exit(void)
{
	platform_driver_unregister(&ubwcp_platform_driver);

	DBG("-----------");
}

module_init(ubwcp_init);
module_exit(ubwcp_exit);

MODULE_LICENSE("GPL");
