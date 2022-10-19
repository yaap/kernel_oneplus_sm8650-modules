/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MSM_VIDC_MEMORY_EXT_H_
#define _MSM_VIDC_MEMORY_EXT_H_

#include "msm_vidc_memory.h"

struct dma_buf_attachment *msm_vidc_dma_buf_attach_ext(struct msm_vidc_core *core,
    struct dma_buf *dbuf, struct device *dev);
int msm_vidc_memory_alloc_ext(struct msm_vidc_core *core,
	struct msm_vidc_alloc *alloc);
int msm_vidc_memory_free_ext(struct msm_vidc_core *core, struct msm_vidc_alloc *mem);
int msm_vidc_memory_map_ext(struct msm_vidc_core *core,
	struct msm_vidc_map *map);
u32 msm_vidc_buffer_region_ext(struct msm_vidc_inst *inst,
	enum msm_vidc_buffer_type buffer_type);

#endif // _MSM_VIDC_MEMORY_EXT_H_