// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __LINUX_BTFM_CODEC_PKT_H
#define __LINUX_BTFM_CODEC_PKT_H

typedef uint16_t btm_opcode;

struct btm_req {
	btm_opcode opcode;
	uint8_t len;
	uint8_t *data;
};

struct btm_rsp {
	btm_opcode opcode;
	uint8_t status;
};

struct btm_ind {
	btm_opcode opcode;
	uint8_t len;
	uint8_t *data;
};

#define	BTM_BTFMCODEC_PREPARE_AUDIO_BEARER_SWITCH_REQ		0x5000
#define BTM_BTFMCODEC_PREPARE_AUDIO_BEARER_SWITCH_RSP 		0x5001
#define BTM_BTFMCODEC_MASTER_CONFIG_REQ			            0x5002
#define BTM_BTFMCODEC_MASTER_CONFIG_RSP			            0x5003
#define BTM_BTFMCODEC_MASTER_SHUTDOWN_REQ			        0x5004
#define BTM_BTFMCODEC_CTRL_MASTER_SHUTDOWN_RSP			    0x5005
#define BTM_BTFMCODEC_BEARER_SWITCH_IND			            0x50C8
#define BTM_BTFMCODEC_TRANSPORT_SWITCH_FAILED_IND		    0x50C9

struct btm_master_config_req {
	btm_opcode opcode;
	uint8_t len;
	uint8_t stream_identifier;
	uint32_t device_identifier;
	uint32_t sampling_rate;
	uint8_t bit_width;
	uint8_t num_of_channels;
	uint8_t channel_num;
	uint8_t codec_id;
};

#endif /* __LINUX_BTFM_CODEC_PKT_H*/
