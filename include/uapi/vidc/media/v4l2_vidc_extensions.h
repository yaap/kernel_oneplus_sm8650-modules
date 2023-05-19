/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __V4l2_VIDC_EXTENSIONS_H__
#define __V4l2_VIDC_EXTENSIONS_H__

#include <linux/types.h>
#include <linux/v4l2-controls.h>

/* AV1 */
#ifndef V4L2_PIX_FMT_AV1
#define V4L2_PIX_FMT_AV1                        v4l2_fourcc('A', 'V', '1', '0')
#endif

#ifndef V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10_STILL_PICTURE
#define V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10_STILL_PICTURE    (3)
#endif

/* vendor controls start */
#ifdef V4L2_CTRL_CLASS_CODEC
#define V4L2_CID_MPEG_VIDC_BASE (V4L2_CTRL_CLASS_CODEC | 0x2000)
#else
#define V4L2_CID_MPEG_VIDC_BASE (V4L2_CTRL_CLASS_MPEG | 0x2000)
#endif

#define V4L2_MPEG_MSM_VIDC_DISABLE 0
#define V4L2_MPEG_MSM_VIDC_ENABLE 1

#define V4L2_CID_MPEG_VIDC_SECURE               (V4L2_CID_MPEG_VIDC_BASE + 0x1)
#define V4L2_CID_MPEG_VIDC_LOWLATENCY_REQUEST   (V4L2_CID_MPEG_VIDC_BASE + 0x3)
#define V4L2_CID_MPEG_VIDC_TIME_DELTA_BASED_RC  (V4L2_CID_MPEG_VIDC_BASE + 0xD)
#define V4L2_CID_MPEG_VIDC_PRIORITY             (V4L2_CID_MPEG_VIDC_BASE + 0x2A)

/* Encoder Complexity control */
#define V4L2_CID_MPEG_VIDC_VENC_COMPLEXITY                                   \
    (V4L2_CID_MPEG_VIDC_BASE + 0x2F)

/* Decoder Max Number of Reorder Frames */
#define V4L2_CID_MPEG_VIDC_METADATA_MAX_NUM_REORDER_FRAMES                   \
    (V4L2_CID_MPEG_VIDC_BASE + 0x30)

#define V4L2_CID_MPEG_VIDC_VUI_TIMING_INFO                                    \
    (V4L2_CID_MPEG_VIDC_BASE + 0x43)

#endif
