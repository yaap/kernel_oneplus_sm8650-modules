// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "ipa_rtp_genl.h"
#include "ipa_i.h"
#include <net/sock.h>
#include <linux/skbuff.h>
#include <uapi/linux/in.h>

#define MAX_OPEN_FRAMES 3
/* Single-NAL:0, FU-A Type: 1 */
#define MAX_STREAM_TYPES 2
#define MAX_IP_TYPES 2

#define IPA_RTP_GENL_OP(_cmd, _func)			\
	{						\
		.cmd	= _cmd,				\
		.doit	= _func,			\
		.dumpit	= NULL,				\
		.flags	= 0,				\
	}

static u8 si[MAX_STREAMS];

static struct nla_policy ipa_rtp_genl_attr_policy[IPA_RTP_GENL_ATTR_MAX + 1] = {
	[IPA_RTP_GENL_ATTR_STR]  = { .type = NLA_NUL_STRING, .len = IPA_RTP_GENL_MAX_STR_LEN },
	[IPA_RTP_GENL_ATTR_INT]  = { .type = NLA_S32 },
	[IPA_RTP_GENL_ATTR_TUPLE_INFO] = NLA_POLICY_EXACT_LEN(sizeof(struct traffic_tuple_info)),
	[IPA_RTP_GENL_ATTR_ASSIGN_STREAM_ID] =
				NLA_POLICY_EXACT_LEN(sizeof(struct assign_stream_id)),
	[IPA_RTP_GENL_ATTR_ADD_BITSTREAM_BUFF] =
				 NLA_POLICY_EXACT_LEN(sizeof(struct bitstream_buffers)),
	[IPA_RTP_GENL_ATTR_SMMU_MAP_BUFF] = NLA_POLICY_EXACT_LEN(sizeof(struct map_buffer)),
	[IPA_RTP_GENL_ATTR_SMMU_UNMAP_BUFF] = NLA_POLICY_EXACT_LEN(sizeof(struct unmap_buffer)),
	[IPA_RTP_GENL_ATTR_REMOVE_STREAM_ID] =
				 NLA_POLICY_EXACT_LEN(sizeof(struct remove_bitstream_buffers)),
};

static const struct genl_ops ipa_rtp_genl_ops[] = {
	IPA_RTP_GENL_OP(IPA_RTP_GENL_CMD_TUPLE_INFO,
			ipa_rtp_tuple_info_req_hdlr),
	IPA_RTP_GENL_OP(IPA_RTP_GENL_CMD_ADD_BITSTREAM_BUFF,
			ipa_rtp_add_bitstream_buff_req_hdlr),
	IPA_RTP_GENL_OP(IPA_RTP_GENL_CMD_SMMU_MAP_BUFF,
			ipa_rtp_smmu_map_buff_req_hdlr),
	IPA_RTP_GENL_OP(IPA_RTP_GENL_CMD_SMMU_UNMAP_BUFF,
			ipa_rtp_smmu_unmap_buff_req_hdlr),
	IPA_RTP_GENL_OP(IPA_RTP_GENL_CMD_REMOVE_STREAM_ID,
			ipa_rtp_rmv_stream_id_req_hdlr),
};

struct genl_family ipa_rtp_genl_family = {
	.id = 0,
	.hdrsize = 0,
	.name    = IPA_RTP_GENL_FAMILY_NAME,
	.version = IPA_RTP_GENL_VERSION,
	.maxattr = IPA_RTP_GENL_ATTR_MAX,
	.policy  = ipa_rtp_genl_attr_policy,
	.ops     = ipa_rtp_genl_ops,
	.n_ops   = ARRAY_SIZE(ipa_rtp_genl_ops),
};

int ipa_rtp_send_tuple_info_resp(struct genl_info *info,
			      struct assign_stream_id *tuple_info_resp)
{
	struct sk_buff *skb;
	void *msg_head;
	int rc = -1;

	IPADBG_LOW("Entry\n");

	if (!info || !tuple_info_resp) {
		IPAERR("Invalid params\n");
		return rc;
	}

	skb = genlmsg_new(sizeof(struct assign_stream_id), GFP_KERNEL);
	if (!skb) {
		IPAERR("failed to alloc genmsg_new\n");
		return rc;
	}

	msg_head = genlmsg_put(skb, 0, info->snd_seq + 1,
			       &ipa_rtp_genl_family,
			       0, IPA_RTP_GENL_CMD_ASSIGN_STREAM_ID);
	if (!msg_head) {
		IPAERR("failed at genlmsg_put\n");
		goto free_skb;
	}

	rc = nla_put(skb, IPA_RTP_GENL_ATTR_ASSIGN_STREAM_ID,
		     sizeof(struct assign_stream_id),
		     tuple_info_resp);
	if (rc != 0) {
		IPAERR("failed at nla_put skb\n");
		goto free_skb;
	}

	genlmsg_end(skb, msg_head);

	rc = genlmsg_unicast(genl_info_net(info), skb, info->snd_portid);
	if (rc != 0) {
		IPAERR("failed in doing genlmsg_unicast\n");
		goto free_skb;
	}

	ipa3_ctx->rtp_stream_id_cnt++;
	IPADBG("assigned stream-id is %u\n", tuple_info_resp->stream_id);
	IPADBG_LOW("Exit\n");

free_skb:
	kfree(skb);
	return rc;
}

int ipa_rtp_tuple_info_req_hdlr(struct sk_buff *skb_2,
				     struct genl_info *info)
{
	struct nlattr *na;
	struct traffic_tuple_info tuple_info_req;
	struct assign_stream_id tuple_info_resp;
	int is_req_valid = 0, i = 0;
	int stream_id_available = 0, rc = -1;

	IPADBG("Entry\n");

	if (!info) {
		IPAERR("error genl info is null\n");
		return rc;
	}

	na = info->attrs[IPA_RTP_GENL_ATTR_TUPLE_INFO];
	if (na) {
		if (nla_memcpy(&tuple_info_req, na,
			       sizeof(tuple_info_req)) > 0) {
			is_req_valid = 1;
		} else {
			IPAERR("nla_memcpy failed %d\n",
			       IPA_RTP_GENL_ATTR_TUPLE_INFO);
			return rc;
		}
	} else {
		IPAERR("no info->attrs %d\n",
		       IPA_RTP_GENL_ATTR_TUPLE_INFO);
		return rc;
	}

	if (tuple_info_req.ts_info.no_of_openframe <= 0 ||
		tuple_info_req.ts_info.no_of_openframe > MAX_OPEN_FRAMES ||
		tuple_info_req.ts_info.stream_type >= MAX_STREAM_TYPES ||
		!tuple_info_req.ts_info.max_pkt_frame ||
		tuple_info_req.ip_type >= MAX_IP_TYPES) {
		IPAERR("invalid no-of-open-frames %u or stream_type %u\n",
				tuple_info_req.ts_info.no_of_openframe,
				tuple_info_req.ts_info.stream_type);
		IPAERR("or max_pkt_frames %u or ip_type %u params\n",
				tuple_info_req.ts_info.max_pkt_frame,
				tuple_info_req.ip_type);
		return rc;
	}

	/* IPv4 Type */
	if (!tuple_info_req.ip_type) {
		if (tuple_info_req.ip_info.ipv4.protocol != IPPROTO_UDP ||
			!tuple_info_req.ip_info.ipv4.src_ip ||
			!tuple_info_req.ip_info.ipv4.dst_ip) {
			IPAERR("invalid src_ip %u or dst_ip %u or protocol %u params\n",
			tuple_info_req.ip_info.ipv4.src_ip, tuple_info_req.ip_info.ipv4.dst_ip,
			tuple_info_req.ip_info.ipv4.protocol);
			return rc;
		}
	} else {
		if (tuple_info_req.ip_info.ipv6.protocol != IPPROTO_UDP) {
			IPAERR("invalid ipv6 protocol %u params\n",
				tuple_info_req.ip_info.ipv6.protocol);
			return rc;
		}
	}

	IPADBG_LOW("no_of_openframes are %u\n", tuple_info_req.ts_info.no_of_openframe);
	IPADBG_LOW("max_pkt_frame is %u\n", tuple_info_req.ts_info.max_pkt_frame);
	IPADBG_LOW("stream_type is %u\n", tuple_info_req.ts_info.stream_type);
	IPADBG_LOW("reorder_timeout is %u\n", tuple_info_req.ts_info.reorder_timeout);
	IPADBG_LOW("num_slices_per_frame are %u\n", tuple_info_req.ts_info.num_slices_per_frame);
	IPADBG_LOW("ip_type is %u\n", tuple_info_req.ip_type);
	IPADBG_LOW("src_port_number is %u\n", tuple_info_req.ip_info.ipv4.src_port_number);
	IPADBG_LOW("dst_port_number is %u\n", tuple_info_req.ip_info.ipv4.dst_port_number);
	IPADBG_LOW("src_ip is %u\n", tuple_info_req.ip_info.ipv4.src_ip);
	IPADBG_LOW("dst_ip is %u\n", tuple_info_req.ip_info.ipv4.dst_ip);
	IPADBG_LOW("protocol is %u\n", tuple_info_req.ip_info.ipv4.protocol);

	/* Call IPA driver/uC tuple info API's here */
	memset(&tuple_info_resp, 0, sizeof(tuple_info_resp));

	for (i = 0; i < MAX_STREAMS; i++) {
		if (si[i] == 0) {
			tuple_info_resp.stream_id = i;
			si[i] = 1;
			stream_id_available = 1;
			break;
		}
	}

	if (!stream_id_available) {
		IPAERR("max stream-ids supported are four only\n");
		return rc;
	}

	if (is_req_valid &&
		ipa_rtp_send_tuple_info_resp(info, &tuple_info_resp))
		si[tuple_info_resp.stream_id] = 0;
	else
		rc = 0;

	IPADBG("Exit\n");
	return rc;
}

int ipa_rtp_smmu_map_buff_req_hdlr(struct sk_buff *skb_2,
				     struct genl_info *info)
{
	struct nlattr *na;
	struct map_buffer map_buffer_req;
	int i = 0, is_req_valid = 0;
	int rc = -1;

	IPADBG("Entry\n");

	if (!info) {
		IPAERR("error genl info is null\n");
		return rc;
	}

	na = info->attrs[IPA_RTP_GENL_ATTR_SMMU_MAP_BUFF];
	if (na) {
		if (nla_memcpy(&map_buffer_req, na,
			       sizeof(map_buffer_req)) > 0) {
			is_req_valid = 1;
		} else {
			IPAERR("nla_memcpy failed %d\n",
			       IPA_RTP_GENL_ATTR_SMMU_MAP_BUFF);
			return rc;
		}
	} else {
		IPAERR("no info->attrs %d\n",
		       IPA_RTP_GENL_ATTR_SMMU_MAP_BUFF);
		return rc;
	}

	if (map_buffer_req.nfd <= 0 || map_buffer_req.nfd > MAX_FDS
			 || map_buffer_req.stream_id > MAX_STREAMS) {
		IPAERR("invalid nfd %u or stream_id %u params\n",
					map_buffer_req.nfd, map_buffer_req.stream_id);
		return rc;
	}

	IPADBG_LOW("number of fd's are %u\n", map_buffer_req.nfd);
	IPADBG_LOW("stream_id is %u\n", map_buffer_req.stream_id);

	/* If IPA C2 component is providing two fd's for meta fd and bitstream buff fd then
	 * sizes need to be filled. If it is a single fd for both meta data and bitstream buff
	 * then meta_buff_fd and bitstream_buffer_fd will be the same. And they need to fill
	 * bitstream_buffer_size as actual size and meta_buff_size to zero.
	 */

	for (i = 0; i < map_buffer_req.nfd; i++) {
		if (map_buffer_req.buff_info[i].bitstream_buffer_fd ==
			map_buffer_req.buff_info[i].meta_buff_fd) {
			if (!map_buffer_req.buff_info[i].bitstream_buffer_size ||
				map_buffer_req.buff_info[i].meta_buff_size) {
				IPAERR("invalid bitstream_buff_size %u\n",
					map_buffer_req.buff_info[i].bitstream_buffer_size);
				IPAERR("or meta_buff_size %u params\n",
					map_buffer_req.buff_info[i].meta_buff_size);
				return rc;
			}
		} else {
			if (!map_buffer_req.buff_info[i].bitstream_buffer_size ||
				!map_buffer_req.buff_info[i].meta_buff_size) {
				IPAERR("invalid bitstream_buff_size %u\n",
					map_buffer_req.buff_info[i].bitstream_buffer_size);
				IPAERR("or meta_buff_size %u params\n",
					map_buffer_req.buff_info[i].meta_buff_size);
				return rc;
			}
		}

		IPADBG_LOW("bitstream_buffer_fd is %u\n",
			map_buffer_req.buff_info[i].bitstream_buffer_fd);
		IPADBG_LOW("meta_buff_fd is %u\n",
			map_buffer_req.buff_info[i].meta_buff_fd);
		IPADBG_LOW("bitstream_buffer_size is %u\n",
			map_buffer_req.buff_info[i].bitstream_buffer_size);
		IPADBG_LOW("meta_buff_size is %u\n",
			map_buffer_req.buff_info[i].meta_buff_size);
	}

	/* Call IPA driver/uC API's here */
	if (is_req_valid)
		rc = ipa3_map_buff_to_device_addr(&map_buffer_req);

	IPADBG("Exit\n");
	return rc;
}

int ipa_rtp_smmu_unmap_buff_req_hdlr(struct sk_buff *skb_2,
				     struct genl_info *info)
{
	struct nlattr *na;
	struct unmap_buffer unmap_buffer_req;
	int i = 0, is_req_valid = 0, rc = -1;

	IPADBG("Entry\n");

	if (!info) {
		IPAERR("error genl info is null\n");
		return rc;
	}

	na = info->attrs[IPA_RTP_GENL_ATTR_SMMU_UNMAP_BUFF];
	if (na) {
		if (nla_memcpy(&unmap_buffer_req, na,
			       sizeof(unmap_buffer_req)) > 0) {
			is_req_valid = 1;
		} else {
			IPAERR("nla_memcpy failed %d\n",
			       IPA_RTP_GENL_ATTR_SMMU_UNMAP_BUFF);
			return rc;
		}
	} else {
		IPAERR("no info->attrs %d\n",
		       IPA_RTP_GENL_ATTR_SMMU_UNMAP_BUFF);
		return rc;
	}

	if (unmap_buffer_req.nfd <= 0 || unmap_buffer_req.nfd > MAX_FDS
				 || unmap_buffer_req.stream_id > MAX_STREAMS) {
		IPAERR("invalid nfd %u or stream_id %u params\n",
					unmap_buffer_req.nfd, unmap_buffer_req.stream_id);
		return rc;
	}

	IPADBG_LOW("number of fd's are %u\n", unmap_buffer_req.nfd);
	IPADBG_LOW("stream_id is %u\n", unmap_buffer_req.stream_id);

	/* If IPA C2 component is providing two fd's for meta fd and bitstream buff fd then
	 * sizes need to be filled. If it is a single fd for both meta data and bitstream buff
	 * then meta_buff_fd and bitstream_buffer_fd will be the same. And they need to fill
	 * bitstream_buffer_size as actual size and meta_buff_size to zero.
	 */

	for (i = 0; i < unmap_buffer_req.nfd; i++) {
		if (unmap_buffer_req.buff_info[i].bitstream_buffer_fd ==
			unmap_buffer_req.buff_info[i].meta_buff_fd) {
			if (!unmap_buffer_req.buff_info[i].bitstream_buffer_size ||
				unmap_buffer_req.buff_info[i].meta_buff_size) {
				IPAERR("invalid bitstream_buff_size %u\n",
					unmap_buffer_req.buff_info[i].bitstream_buffer_size);
				IPAERR("or meta_buff_size %u params\n",
					unmap_buffer_req.buff_info[i].meta_buff_size);
				return rc;
			}
		} else {
			if (!unmap_buffer_req.buff_info[i].bitstream_buffer_size ||
				!unmap_buffer_req.buff_info[i].meta_buff_size) {
				IPAERR("invalid bitstream_buff_size %u\n",
					unmap_buffer_req.buff_info[i].bitstream_buffer_size);
				IPAERR("or meta_buff_size %u params\n",
					unmap_buffer_req.buff_info[i].meta_buff_size);
				return rc;
			}
		}

		IPADBG_LOW("bitstream_buffer_fd is %u\n",
			unmap_buffer_req.buff_info[i].bitstream_buffer_fd);
		IPADBG_LOW("meta_buff_fd is %u\n",
			unmap_buffer_req.buff_info[i].meta_buff_fd);
		IPADBG_LOW("bitstream_buffer_size is %u\n",
			unmap_buffer_req.buff_info[i].bitstream_buffer_size);
		IPADBG_LOW("meta_buff_size is %u\n",
			unmap_buffer_req.buff_info[i].meta_buff_size);
	}

	/* Call IPA driver/uC tuple info API's here */
	if (is_req_valid)
		rc = ipa3_unmap_buff_from_device_addr(&unmap_buffer_req);

	IPADBG("Exit\n");
	return rc;
}

int ipa_rtp_add_bitstream_buff_req_hdlr(struct sk_buff *skb_2,
				     struct genl_info *info)
{
	struct nlattr *na;
	struct bitstream_buffers bs_buffer_req;
	int i = 0, is_req_valid = 0, rc = -1;

	IPADBG("Entry\n");

	if (!info) {
		IPAERR("error genl info is null\n");
		return rc;
	}

	na = info->attrs[IPA_RTP_GENL_ATTR_ADD_BITSTREAM_BUFF];
	if (na) {
		if (nla_memcpy(&bs_buffer_req, na,
			       sizeof(bs_buffer_req)) > 0) {
			is_req_valid = 1;
		} else {
			IPAERR("nla_memcpy failed %d\n",
			       IPA_RTP_GENL_ATTR_ADD_BITSTREAM_BUFF);
			return rc;
		}
	} else {
		IPAERR("no info->attrs %d\n",
		       IPA_RTP_GENL_ATTR_ADD_BITSTREAM_BUFF);
		return rc;
	}

	if (bs_buffer_req.buff_cnt <= 0 || bs_buffer_req.buff_cnt > MAX_BUFF ||
				bs_buffer_req.cookie != IPA_BS_BUFF_COOKIE) {
		IPAERR("invalid buff_cnt %u or buff_cookie 0x%x params\n",
					bs_buffer_req.buff_cnt, bs_buffer_req.cookie);
		return rc;
	}

	IPADBG_LOW("buff_cnt is %u\n", bs_buffer_req.buff_cnt);
	IPADBG_LOW("cookie is 0x%x\n", bs_buffer_req.cookie);

	/* If IPA C2 component is providing two buffers for meta data and bitstream buff,
	 * they need to fill meta_buff_offset and buff_offset as zero.
	 * If it is a single buffer for meta data and bitstream buff, then meta_buff_fd
	 * and buff_fd will be the same. And they need to fill meta_buff_offset as zero
	 * and fill the bitstream buff offset in buff_offset and it should be 4 byte aligned.
	 */

	for (i = 0; i < bs_buffer_req.buff_cnt; i++) {
		if (bs_buffer_req.bs_info[i].stream_id >= MAX_STREAMS) {
			IPAERR("invalid stream_id in buffer %u params\n",
					bs_buffer_req.bs_info[i].stream_id);
			return rc;
		}

		if (bs_buffer_req.bs_info[i].meta_buff_fd == bs_buffer_req.bs_info[i].buff_fd) {
			if (bs_buffer_req.bs_info[i].meta_buff_offset ||
				!bs_buffer_req.bs_info[i].buff_offset ||
				bs_buffer_req.bs_info[i].meta_buff_size ||
				!bs_buffer_req.bs_info[i].buff_size) {
				IPAERR("invalid meta_buff_offset %u or bs_buff_offset %u\n",
						bs_buffer_req.bs_info[i].meta_buff_offset,
						bs_buffer_req.bs_info[i].buff_offset);
				IPAERR("or meta_buff_size %u or bs_buff_size %u params\n",
						bs_buffer_req.bs_info[i].meta_buff_size,
						bs_buffer_req.bs_info[i].buff_size);
				return rc;
			}
		} else {
			if (bs_buffer_req.bs_info[i].meta_buff_offset ||
				bs_buffer_req.bs_info[i].buff_offset ||
				!bs_buffer_req.bs_info[i].meta_buff_size ||
				!bs_buffer_req.bs_info[i].buff_size) {
				IPAERR("invalid meta_buff_offset %u or bs_buff_offset %u\n",
						bs_buffer_req.bs_info[i].meta_buff_offset,
						bs_buffer_req.bs_info[i].buff_offset);
				IPAERR("or meta_buff_size %u or bs_buff_size %u params\n",
						bs_buffer_req.bs_info[i].meta_buff_size,
						bs_buffer_req.bs_info[i].buff_size);
				return rc;
			}
		}

		IPADBG_LOW("stream_id is %u\n", bs_buffer_req.bs_info[i].stream_id);
		IPADBG_LOW("fence_id is %u\n", bs_buffer_req.bs_info[i].fence_id);
		IPADBG_LOW("buff_offset is %u\n", bs_buffer_req.bs_info[i].buff_offset);
		IPADBG_LOW("buff_fd is %u\n", bs_buffer_req.bs_info[i].buff_fd);
		IPADBG_LOW("buff_size is %u\n", bs_buffer_req.bs_info[i].buff_size);
		IPADBG_LOW("meta_buff_offset is %u\n", bs_buffer_req.bs_info[i].meta_buff_offset);
		IPADBG_LOW("meta_buff_fd is %u\n", bs_buffer_req.bs_info[i].meta_buff_fd);
		IPADBG_LOW("meta_buff_size is %u\n", bs_buffer_req.bs_info[i].meta_buff_size);
	}

	/* Call IPA driver/uC API's here */
	if (is_req_valid)
		rc = ipa3_send_bitstream_buff_info(&bs_buffer_req);

	IPADBG("Exit\n");
	return rc;
}

int ipa_rtp_rmv_stream_id_req_hdlr(struct sk_buff *skb_2,
				     struct genl_info *info)
{
	struct nlattr *na;
	struct remove_bitstream_buffers rmv_sid_req;
	int is_req_valid = 0, rc = -1;

	IPADBG("Entry\n");

	if (!info) {
		IPAERR("error genl info is null\n");
		return rc;
	}

	na = info->attrs[IPA_RTP_GENL_CMD_REMOVE_STREAM_ID];
	if (na) {
		if (nla_memcpy(&rmv_sid_req, na,
			sizeof(rmv_sid_req)) > 0) {
			is_req_valid = 1;
		} else {
			IPAERR("nla_memcpy failed %d\n",
			       IPA_RTP_GENL_CMD_REMOVE_STREAM_ID);
			return rc;
		}
	} else {
		IPAERR("no info->attrs %d\n",
		       IPA_RTP_GENL_CMD_REMOVE_STREAM_ID);
		return rc;
	}

	if (rmv_sid_req.stream_id >= MAX_STREAMS) {
		IPAERR("invalid stream_id %u params\n", rmv_sid_req.stream_id);
		return rc;
	}

	/* Call IPA driver/uC tuple info API's here */
	if (is_req_valid)
		rc = ipa3_uc_send_remove_stream_cmd(&rmv_sid_req);

	si[rmv_sid_req.stream_id] = 0;
	ipa3_ctx->rtp_stream_id_cnt--;

	IPADBG("Exit\n");
	return rc;
}

/* register ipa rtp driver family with generic netlink */
int ipa_rtp_genl_init(void)
{
	int rc = 0;

	rc = genl_register_family(&ipa_rtp_genl_family);
	if (rc != 0) {
		IPAERR("ipa_rtp genl register family failed: %d", rc);
		genl_unregister_family(&ipa_rtp_genl_family);
		return rc;
	}

	IPAERR("successfully registered ipa_rtp genl family: %s",
	       IPA_RTP_GENL_FAMILY_NAME);
	return rc;
}

/* Unregister the generic netlink family */
int ipa_rtp_genl_deinit(void)
{
	int rc = 0;

	rc = genl_unregister_family(&ipa_rtp_genl_family);
	if (rc != 0)
		IPAERR("unregister ipa_rtp genl family failed: %d", rc);
	return rc;
}

