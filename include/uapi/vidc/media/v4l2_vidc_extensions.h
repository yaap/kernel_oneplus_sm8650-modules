/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __V4l2_VIDC_EXTENSIONS_H__
#define __V4l2_VIDC_EXTENSIONS_H__

#include <linux/types.h>
#include <linux/v4l2-controls.h>

/* 32  RGBA-8-8-8-8 compressed */
#define V4L2_PIX_FMT_VIDC_ARGB32C               v4l2_fourcc('Q', '2', '4', 'C')
#define V4L2_META_FMT_VIDC                      v4l2_fourcc('Q', 'M', 'E', 'T')
/* HEIC encoder and decoder */
#define V4L2_PIX_FMT_HEIC                       v4l2_fourcc('H', 'E', 'I', 'C')
/* AV1 */
#define V4L2_PIX_FMT_AV1                        v4l2_fourcc('A', 'V', '1', '0')
/* start of vidc specific colorspace definitions */
/*
 * V4L2_COLORSPACE_VIDC_START, V4L2_XFER_FUNC_VIDC_START
 * and V4L2_YCBCR_VIDC_START are introduced because
 * V4L2_COLORSPACE_LAST, V4L2_XFER_FUNC_LAST, and
 * V4L2_YCBCR_ENC_LAST respectively are not accessible
 * in userspace. These values are needed in userspace
 * to check if the colorspace info is private.
 */
#define V4L2_COLORSPACE_VIDC_START           100
#define V4L2_COLORSPACE_VIDC_GENERIC_FILM    101
#define V4L2_COLORSPACE_VIDC_EG431           102
#define V4L2_COLORSPACE_VIDC_EBU_TECH        103

#define V4L2_XFER_FUNC_VIDC_START            200
#define V4L2_XFER_FUNC_VIDC_BT470_SYSTEM_M   201
#define V4L2_XFER_FUNC_VIDC_BT470_SYSTEM_BG  202
#define V4L2_XFER_FUNC_VIDC_BT601_525_OR_625 203
#define V4L2_XFER_FUNC_VIDC_LINEAR           204
#define V4L2_XFER_FUNC_VIDC_XVYCC            205
#define V4L2_XFER_FUNC_VIDC_BT1361           206
#define V4L2_XFER_FUNC_VIDC_BT2020           207
#define V4L2_XFER_FUNC_VIDC_ST428            208
#define V4L2_XFER_FUNC_VIDC_HLG              209

/* should be 255 or below due to u8 limitation */
#define V4L2_YCBCR_VIDC_START                240
#define V4L2_YCBCR_VIDC_SRGB_OR_SMPTE_ST428  241
#define V4L2_YCBCR_VIDC_FCC47_73_682         242

/* end of vidc specific colorspace definitions */
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
/* FIXme: */
#define V4L2_CID_MPEG_VIDC_CODEC_CONFIG         (V4L2_CID_MPEG_VIDC_BASE + 0x4)
#define V4L2_CID_MPEG_VIDC_FRAME_RATE           (V4L2_CID_MPEG_VIDC_BASE + 0x5)
#define V4L2_CID_MPEG_VIDC_OPERATING_RATE       (V4L2_CID_MPEG_VIDC_BASE + 0x6)

#define V4L2_CID_MPEG_VIDC_TIME_DELTA_BASED_RC  (V4L2_CID_MPEG_VIDC_BASE + 0xD)
/* Encoder quality controls */
#define V4L2_CID_MPEG_VIDC_CONTENT_ADAPTIVE_CODING                            \
	(V4L2_CID_MPEG_VIDC_BASE + 0xE)
#define V4L2_CID_MPEG_VIDC_QUALITY_BITRATE_BOOST                              \
	(V4L2_CID_MPEG_VIDC_BASE + 0xF)
#define V4L2_CID_MPEG_VIDC_VIDEO_BLUR_TYPES                                   \
	(V4L2_CID_MPEG_VIDC_BASE + 0x10)
enum v4l2_mpeg_vidc_blur_types {
	VIDC_BLUR_NONE               = 0x0,
	VIDC_BLUR_EXTERNAL           = 0x1,
	VIDC_BLUR_ADAPTIVE           = 0x2,
};
/* (blur width) << 16 | (blur height) */
#define V4L2_CID_MPEG_VIDC_VIDEO_BLUR_RESOLUTION                              \
	(V4L2_CID_MPEG_VIDC_BASE + 0x11)
/* TODO: jdas: compound control for matrix */
#define V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_CUSTOM_MATRIX                        \
	(V4L2_CID_MPEG_VIDC_BASE + 0x12)
#define V4L2_CID_MPEG_VIDC_METADATA_LTR_MARK_USE_DETAILS                      \
	(V4L2_CID_MPEG_VIDC_BASE + 0x13)
#define V4L2_CID_MPEG_VIDC_METADATA_SEQ_HEADER_NAL                            \
	(V4L2_CID_MPEG_VIDC_BASE + 0x14)
#define V4L2_CID_MPEG_VIDC_METADATA_DPB_LUMA_CHROMA_MISR                      \
	(V4L2_CID_MPEG_VIDC_BASE + 0x15)
#define V4L2_CID_MPEG_VIDC_METADATA_OPB_LUMA_CHROMA_MISR                      \
	(V4L2_CID_MPEG_VIDC_BASE + 0x16)
#define V4L2_CID_MPEG_VIDC_METADATA_INTERLACE                                 \
	(V4L2_CID_MPEG_VIDC_BASE + 0x17)
#define V4L2_CID_MPEG_VIDC_METADATA_CONCEALED_MB_COUNT                        \
	(V4L2_CID_MPEG_VIDC_BASE + 0x18)
#define V4L2_CID_MPEG_VIDC_METADATA_HISTOGRAM_INFO                            \
	(V4L2_CID_MPEG_VIDC_BASE + 0x19)
#define V4L2_CID_MPEG_VIDC_METADATA_SEI_MASTERING_DISPLAY_COLOUR              \
	(V4L2_CID_MPEG_VIDC_BASE + 0x1A)
#define V4L2_CID_MPEG_VIDC_METADATA_SEI_CONTENT_LIGHT_LEVEL                   \
	(V4L2_CID_MPEG_VIDC_BASE + 0x1B)
#define V4L2_CID_MPEG_VIDC_METADATA_HDR10PLUS                                 \
	(V4L2_CID_MPEG_VIDC_BASE + 0x1C)
#define V4L2_CID_MPEG_VIDC_METADATA_EVA_STATS                                 \
	(V4L2_CID_MPEG_VIDC_BASE + 0x1D)
#define V4L2_CID_MPEG_VIDC_METADATA_BUFFER_TAG                                \
	(V4L2_CID_MPEG_VIDC_BASE + 0x1E)
#define V4L2_CID_MPEG_VIDC_METADATA_SUBFRAME_OUTPUT                           \
	(V4L2_CID_MPEG_VIDC_BASE + 0x1F)
#define V4L2_CID_MPEG_VIDC_METADATA_ROI_INFO                                  \
	(V4L2_CID_MPEG_VIDC_BASE + 0x20)
#define V4L2_CID_MPEG_VIDC_METADATA_TIMESTAMP                                 \
	(V4L2_CID_MPEG_VIDC_BASE + 0x21)
#define V4L2_CID_MPEG_VIDC_METADATA_ENC_QP_METADATA                           \
	(V4L2_CID_MPEG_VIDC_BASE + 0x22)
#define V4L2_CID_MPEG_VIDC_MIN_BITSTREAM_SIZE_OVERWRITE                       \
	(V4L2_CID_MPEG_VIDC_BASE + 0x23)
#define V4L2_CID_MPEG_VIDC_METADATA_BITSTREAM_RESOLUTION                      \
	(V4L2_CID_MPEG_VIDC_BASE + 0x24)
#define V4L2_CID_MPEG_VIDC_METADATA_CROP_OFFSETS                              \
	(V4L2_CID_MPEG_VIDC_BASE + 0x25)
#define V4L2_CID_MPEG_VIDC_METADATA_SALIENCY_INFO                             \
	(V4L2_CID_MPEG_VIDC_BASE + 0x26)
#define V4L2_CID_MPEG_VIDC_METADATA_TRANSCODE_STAT_INFO                       \
	(V4L2_CID_MPEG_VIDC_BASE + 0x27)

/* Encoder Super frame control */
#define V4L2_CID_MPEG_VIDC_SUPERFRAME           (V4L2_CID_MPEG_VIDC_BASE + 0x28)
/* Thumbnail Mode control */
#define V4L2_CID_MPEG_VIDC_THUMBNAIL_MODE       (V4L2_CID_MPEG_VIDC_BASE + 0x29)
/* Priority control */
#define V4L2_CID_MPEG_VIDC_PRIORITY             (V4L2_CID_MPEG_VIDC_BASE + 0x2A)
/* Metadata DPB Tag List*/
#define V4L2_CID_MPEG_VIDC_METADATA_DPB_TAG_LIST                             \
	(V4L2_CID_MPEG_VIDC_BASE + 0x2B)
/* Encoder Input Compression Ratio control */
#define V4L2_CID_MPEG_VIDC_ENC_INPUT_COMPRESSION_RATIO                       \
	(V4L2_CID_MPEG_VIDC_BASE + 0x2C)
#define V4L2_CID_MPEG_VIDC_METADATA_DEC_QP_METADATA                           \
	(V4L2_CID_MPEG_VIDC_BASE + 0x2E)
/* Encoder Complexity control */
#define V4L2_CID_MPEG_VIDC_VENC_COMPLEXITY                                   \
	(V4L2_CID_MPEG_VIDC_BASE + 0x2F)
/* Decoder Max Number of Reorder Frames */
#define V4L2_CID_MPEG_VIDC_METADATA_MAX_NUM_REORDER_FRAMES                   \
	(V4L2_CID_MPEG_VIDC_BASE + 0x30)
/* Control IDs for AV1 */
#define V4L2_CID_MPEG_VIDEO_AV1_PROFILE        (V4L2_CID_MPEG_VIDC_BASE + 0x31)
enum v4l2_mpeg_video_av1_profile {
	V4L2_MPEG_VIDEO_AV1_PROFILE_MAIN            = 0,
	V4L2_MPEG_VIDEO_AV1_PROFILE_HIGH            = 1,
	V4L2_MPEG_VIDEO_AV1_PROFILE_PROFESSIONAL    = 2,
};

#define V4L2_CID_MPEG_VIDEO_AV1_LEVEL           (V4L2_CID_MPEG_VIDC_BASE + 0x32)
enum v4l2_mpeg_video_av1_level {
	V4L2_MPEG_VIDEO_AV1_LEVEL_2_0  = 0,
	V4L2_MPEG_VIDEO_AV1_LEVEL_2_1  = 1,
	V4L2_MPEG_VIDEO_AV1_LEVEL_2_2  = 2,
	V4L2_MPEG_VIDEO_AV1_LEVEL_2_3  = 3,
	V4L2_MPEG_VIDEO_AV1_LEVEL_3_0  = 4,
	V4L2_MPEG_VIDEO_AV1_LEVEL_3_1  = 5,
	V4L2_MPEG_VIDEO_AV1_LEVEL_3_2  = 6,
	V4L2_MPEG_VIDEO_AV1_LEVEL_3_3  = 7,
	V4L2_MPEG_VIDEO_AV1_LEVEL_4_0  = 8,
	V4L2_MPEG_VIDEO_AV1_LEVEL_4_1  = 9,
	V4L2_MPEG_VIDEO_AV1_LEVEL_4_2  = 10,
	V4L2_MPEG_VIDEO_AV1_LEVEL_4_3  = 11,
	V4L2_MPEG_VIDEO_AV1_LEVEL_5_0  = 12,
	V4L2_MPEG_VIDEO_AV1_LEVEL_5_1  = 13,
	V4L2_MPEG_VIDEO_AV1_LEVEL_5_2  = 14,
	V4L2_MPEG_VIDEO_AV1_LEVEL_5_3  = 15,
	V4L2_MPEG_VIDEO_AV1_LEVEL_6_0  = 16,
	V4L2_MPEG_VIDEO_AV1_LEVEL_6_1  = 17,
	V4L2_MPEG_VIDEO_AV1_LEVEL_6_2  = 18,
	V4L2_MPEG_VIDEO_AV1_LEVEL_6_3  = 19,
	V4L2_MPEG_VIDEO_AV1_LEVEL_7_0  = 20,
	V4L2_MPEG_VIDEO_AV1_LEVEL_7_1  = 21,
	V4L2_MPEG_VIDEO_AV1_LEVEL_7_2  = 22,
	V4L2_MPEG_VIDEO_AV1_LEVEL_7_3  = 23,
};

#define V4L2_CID_MPEG_VIDEO_AV1_TIER        (V4L2_CID_MPEG_VIDC_BASE + 0x33)
enum v4l2_mpeg_video_av1_tier {
	V4L2_MPEG_VIDEO_AV1_TIER_MAIN  = 0,
	V4L2_MPEG_VIDEO_AV1_TIER_HIGH  = 1,
};
/* Decoder Timestamp Reorder control */
#define V4L2_CID_MPEG_VIDC_TS_REORDER           (V4L2_CID_MPEG_VIDC_BASE + 0x34)
/* AV1 Decoder Film Grain */
#define V4L2_CID_MPEG_VIDC_AV1D_FILM_GRAIN_PRESENT                           \
	(V4L2_CID_MPEG_VIDC_BASE + 0x35)
/* Enables Output buffer fence id via input metadata */
#define V4L2_CID_MPEG_VIDC_METADATA_OUTBUF_FENCE                             \
	(V4L2_CID_MPEG_VIDC_BASE + 0x38)
/* Control to set fence id to driver in order get corresponding fence fd */
#define V4L2_CID_MPEG_VIDC_SW_FENCE_ID                                       \
	(V4L2_CID_MPEG_VIDC_BASE + 0x39)
/*
 * Control to get fence fd from driver for the fence id
 * set via V4L2_CID_MPEG_VIDC_SW_FENCE_ID
 */
#define V4L2_CID_MPEG_VIDC_SW_FENCE_FD                                       \
	(V4L2_CID_MPEG_VIDC_BASE + 0x3A)
#define V4L2_CID_MPEG_VIDC_METADATA_PICTURE_TYPE                             \
	(V4L2_CID_MPEG_VIDC_BASE + 0x3B)

/* Encoder Slice Delivery Mode
 * set format has a dependency on this control
 * and gets invoked when this control is updated.
 */
#define V4L2_CID_MPEG_VIDC_HEVC_ENCODE_DELIVERY_MODE                          \
	(V4L2_CID_MPEG_VIDC_BASE + 0x3C)

#define V4L2_CID_MPEG_VIDC_H264_ENCODE_DELIVERY_MODE                          \
	(V4L2_CID_MPEG_VIDC_BASE + 0x3D)

#define V4L2_CID_MPEG_VIDC_CRITICAL_PRIORITY                                 \
	(V4L2_CID_MPEG_VIDC_BASE + 0x3E)
#define V4L2_CID_MPEG_VIDC_RESERVE_DURATION                                  \
	(V4L2_CID_MPEG_VIDC_BASE + 0x3F)

#define V4L2_CID_MPEG_VIDC_METADATA_DOLBY_RPU                                 \
	(V4L2_CID_MPEG_VIDC_BASE + 0x40)

#define V4L2_CID_MPEG_VIDC_CLIENT_ID                                          \
	(V4L2_CID_MPEG_VIDC_BASE + 0x41)

#define V4L2_CID_MPEG_VIDC_LAST_FLAG_EVENT_ENABLE                             \
	(V4L2_CID_MPEG_VIDC_BASE + 0x42)

#define V4L2_CID_MPEG_VIDC_VUI_TIMING_INFO                                    \
	(V4L2_CID_MPEG_VIDC_BASE + 0x43)

#define V4L2_CID_MPEG_VIDC_EARLY_NOTIFY_ENABLE                                \
	(V4L2_CID_MPEG_VIDC_BASE + 0x44)

#define V4L2_CID_MPEG_VIDC_EARLY_NOTIFY_LINE_COUNT                            \
	(V4L2_CID_MPEG_VIDC_BASE + 0x45)

/*
 * This control is introduced to overcome v4l2 limitation
 * of allowing only standard colorspace info via s_fmt.
 * v4l_sanitize_colorspace() is introduced in s_fmt ioctl
 * to reject private colorspace. Through this control, client
 * can set private colorspace info and/or use this control
 * to set colorspace dynamically.
 * The control value is 32 bits packed as:
 *      [ 0 -  7] : matrix coefficients
 *      [ 8 - 15] : transfer characteristics
 *      [16 - 23] : colour primaries
 *      [24 - 31] : range
 * This control is only for encoder.
 * Currently g_fmt in v4l2 does not santize colorspace,
 * hence this control is not introduced for decoder.
 */
#define V4L2_CID_MPEG_VIDC_SIGNAL_COLOR_INFO                                  \
	(V4L2_CID_MPEG_VIDC_BASE + 0x46)

/* control to enable csc */
#define V4L2_CID_MPEG_VIDC_CSC                                                \
	(V4L2_CID_MPEG_VIDC_BASE + 0x47)

#define V4L2_CID_MPEG_VIDC_DRIVER_VERSION                                     \
	(V4L2_CID_MPEG_VIDC_BASE + 0x48)

/* vendor controls end */

/* vendor events start */

/*
 * Vendor event structure looks like below (reference videodev2.h)
 * struct v4l2_event {
 *      __u32                             type;
 *      union {
 *              struct v4l2_event_src_change    src_change;
 *              ...
 *              / ********** vendor event structure ******** /
 *              __u8                            data[64];
 *      } u;
 *      __u32                             pending;
 *      ...
 *  }
 */

/* vendor events end */

#endif
