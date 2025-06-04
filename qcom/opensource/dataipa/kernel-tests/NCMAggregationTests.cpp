/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2017,2020 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *  * Neither the name of The Linux Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * ​​​​​Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include "hton.h" // for htonl
#include "NCMAggregationTestFixture.h"
#include "Constants.h"
#include "TestsUtils.h"
#include "linux/msm_ipa.h"

#define AGGREGATION_LOOP 4
#define IPV4_DST_ADDR_OFFSET (16)
#define IPV4_DST_ADDR_OFFSET_IN_ETH \
		(16 /* IP */ + 14 /* ethernet */)

/////////////////////////////////////////////////////////////////////////////////
//							NCM Aggregation scenarios                         //
/////////////////////////////////////////////////////////////////////////////////

class NCMAggregationScenarios {
public:
	//NCM Aggregation test - sends 5 packets and receives 1 aggregated packet
	static bool NCMAggregationTest(Pipe* input, Pipe* output, enum ipa_ip_type m_eIP);
	//NCM Deaggregation test - sends an aggregated packet made from 5 packets
	//and receives 5 packets
	static bool NCMDeaggregationTest(Pipe* input,
			Pipe* output, enum ipa_ip_type m_eIP);
	//NCM Deaggregation one packet test - sends an aggregated packet made from
	//1 packet and receives 1 packet
	static bool NCMDeaggregationOnePacketTest(Pipe* input, Pipe* output, enum ipa_ip_type m_eIP);
	//NCM Deaggregation and Aggregation test - sends an aggregated packet made
	//from 5 packets and receives the same aggregated packet
	static bool NCMDeaggregationAndAggregationTest(Pipe* input, Pipe* output, enum ipa_ip_type m_eIP);
	//NCM multiple Deaggregation and Aggregation test - sends 5 aggregated
	//packets each one made of 1 packet and receives an aggregated packet made
	//of the 5 packets
	static bool NCMMultipleDeaggregationAndAggregationTest(
			Pipe* input, Pipe* output,
			enum ipa_ip_type m_eIP);
	//NCM Aggregation Loop test - sends 5 packets and expects to receive 1
	//aggregated packet a few times
	static bool NCMAggregationLoopTest(Pipe* input,
			Pipe* output, enum ipa_ip_type m_eIP);
	//NCM Aggregation time limit test - sends 1 small packet smaller than the
	//byte limit and receives 1 aggregated packet
	static bool NCMAggregationTimeLimitTest(Pipe* input, Pipe* output, enum ipa_ip_type m_eIP);
	//NCM Aggregation byte limit test - sends 2 packets that together are
	//larger than the byte limit
	static bool NCMAggregationByteLimitTest(Pipe* input, Pipe* output, enum ipa_ip_type m_eIP);
	static bool NCMAggregationByteLimitTestFC(Pipe* input, Pipe* output, enum ipa_ip_type m_eIP);
	static bool NCMAggregationDualDpTestFC(Pipe* input, Pipe* output1, Pipe* output2, enum ipa_ip_type m_eIP);
	//NCM Deaggregation multiple NDP test - sends an aggregated packet made
	//from 5 packets and 2 NDPs and receives 5 packets
	static bool NCMDeaggregationMultipleNDPTest(Pipe* input, Pipe* output, enum ipa_ip_type m_eIP);
	//NCM Aggregation time limit loop test - sends 5 small packet smaller than
	//the byte limit and receives 5 aggregated packet
	static bool NCMAggregationTimeLimitLoopTest(Pipe* input, Pipe* output, enum ipa_ip_type m_eIP);
	//NCM Aggregation 0 limits test - sends 5 packets and expects to get each
	//packet back aggregated (both size and time limits are 0)
	static bool NCMAggregation0LimitsTest(Pipe* input, Pipe* output, enum ipa_ip_type m_eIP);

private:
	//This method will deaggregate an aggregated packet and compare the packets
	//to the expected packets
	static bool DeaggragateAndComparePackets(
			Byte pAggregatedPacket[MAX_PACKET_SIZE],
			Byte pExpectedPackets[MAX_PACKETS_IN_NCM_TESTS][MAX_PACKET_SIZE],
			int pPacketsSizes[MAX_PACKETS_IN_NCM_TESTS], int nNumPackets,
			int nAggregatedPacketSize);
	//This method will aggregate packets
	static void AggregatePackets(
			Byte pAggregatedPacket[MAX_PACKET_SIZE]/*output*/,
			Byte pPackets[NUM_PACKETS][MAX_PACKET_SIZE],
			int pPacketsSizes[NUM_PACKETS], int nNumPackets,
			int nAggregatedPacketSize);
	//This method will aggregate packets and take into consideration their
	//stream id to separate them into different NDPs
	static void AggregatePacketsWithStreamId(
			Byte pAggregatedPacket[MAX_PACKET_SIZE]/*output*/,
			Byte pPackets[NUM_PACKETS][MAX_PACKET_SIZE],
			int pPacketsSizes[NUM_PACKETS], int nNumPackets,
			int nAggregatedPacketSize, Byte pPacketsStreamId[NUM_PACKETS]);
	//This method will deaggregate an aggregated packet made of one packet and
	//compare the packet to the expected packet
	static bool DeaggragateAndCompareOnePacket(
			Byte pAggregatedPacket[MAX_PACKET_SIZE],
			Byte pExpectedPacket[MAX_PACKET_SIZE], int nPacketsSize,
			int nAggregatedPacketSize);
	//This method will deaggregate an aggregated packet and compare the packets
	//to the expected packets
	static bool DeaggragateAndComparePacketsWithStreamId(
			Byte pAggregatedPacket[MAX_PACKET_SIZE],
			Byte pExpectedPackets[][MAX_PACKET_SIZE], int pPacketsSizes[],
			int nNumPackets, int nAggregatedPacketSize,
			Byte pPacketsStreamId[NUM_PACKETS]);
};

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

bool NCMAggregationScenarios::NCMAggregationTest(
		Pipe* input, Pipe* output, enum ipa_ip_type m_eIP)
{
	//The packets that will be sent
	Byte pPackets[NUM_PACKETS][MAX_PACKET_SIZE];
	//The real sizes of the packets that will be sent
	int pPacketsSizes[NUM_PACKETS];
	//Buffer for the packet that will be received
	Byte pReceivedPacket[2*MAX_PACKET_SIZE];
	//Total size of all sent packets (this is the max size of the aggregated
	//packet minus the size of the header and the NDP)
	int nTotalPacketsSize = MAX_PACKET_SIZE - (4 * NUM_PACKETS) - 24;
	uint32_t nIPv4DSTAddr;
	size_t pIpPacketsSizes[NUM_PACKETS];

	//initialize the packets
	for (int i = 0; i < NUM_PACKETS; i++)
	{
		if (NUM_PACKETS - 1 == i)
			pPacketsSizes[i] = nTotalPacketsSize;
		else
			pPacketsSizes[i] = nTotalPacketsSize / NUM_PACKETS;
		while (0 != pPacketsSizes[i] % 4)
		{
			pPacketsSizes[i]++;
		}
		nTotalPacketsSize -= pPacketsSizes[i];

		// Load input data (Eth packet) from file
		pIpPacketsSizes[i] = MAX_PACKET_SIZE;
		if (!LoadEtherPacket(m_eIP, pPackets[i], pIpPacketsSizes[i]))
		{
			LOG_MSG_ERROR("Failed default Packet");
			return false;
		}
		nIPv4DSTAddr = ntohl(0x7F000001);
		memcpy (&pPackets[i][IPV4_DST_ADDR_OFFSET_IN_ETH],&nIPv4DSTAddr,
				sizeof(nIPv4DSTAddr));
		int size = pIpPacketsSizes[i];
		LOG_MSG_DEBUG("pIpPacketsSizes value of %d element is %d\n",i,pIpPacketsSizes[i]);
		LOG_MSG_DEBUG("pPacketsSizes value of %d element is %d\n",i,pPacketsSizes[i]);
		while (size < pPacketsSizes[i])
		{
			pPackets[i][size] = i;
			size++;
		}

	}

	//send the packets
	for (int i = 0; i < NUM_PACKETS; i++)
	{
		LOG_MSG_DEBUG("Sending packet %d into the USB pipe(%d bytes)\n", i,
					pPacketsSizes[i]);
		int nBytesSent = input->Send(pPackets[i], pPacketsSizes[i]);
		if (pPacketsSizes[i] != nBytesSent)
		{
			LOG_MSG_DEBUG("Sending packet %d into the USB pipe(%d bytes) "
					"failed!\n", i, pPacketsSizes[i]);
			LOG_MSG_DEBUG("nBytesSent value is %d \n",nBytesSent);
			return false;
		}
	}

	//receive the aggregated packet
	LOG_MSG_DEBUG("Reading packet from the USB pipe(%d bytes should be there)"
			"\n", MAX_PACKET_SIZE);
	int nBytesReceived = output->Receive(pReceivedPacket, MAX_PACKET_SIZE);
	if (MAX_PACKET_SIZE != nBytesReceived)
	{
		LOG_MSG_DEBUG("Receiving aggregated packet from the USB pipe(%d bytes) "
				"failed!\n", MAX_PACKET_SIZE);
		LOG_MSG_DEBUG("nBytesReceived value is %d \n",nBytesReceived);
		print_buff(pReceivedPacket, nBytesReceived);
		return false;
	}
	//deaggregating the aggregated packet
	return DeaggragateAndComparePackets(pReceivedPacket, pPackets,
			pPacketsSizes, NUM_PACKETS, nBytesReceived);
}

/////////////////////////////////////////////////////////////////////////////////

bool NCMAggregationScenarios::NCMDeaggregationTest(
		Pipe* input, Pipe* output, enum ipa_ip_type m_eIP)
{
	bool bTestResult = true;
	//The packets that the aggregated packet will be made of
	Byte pExpectedPackets[NUM_PACKETS][MAX_PACKET_SIZE];
	//The real sizes of the packets that the aggregated packet will be made of
	int pPacketsSizes[NUM_PACKETS];
	//Buffers for the packets that will be received
	Byte pReceivedPackets[NUM_PACKETS][MAX_PACKET_SIZE];
	//Total size of all the packets that the aggregated packet will be made of
	//(this is the max size of the aggregated packet
	//minus the size of the header and the NDP)
	int nTotalPacketsSize = MAX_PACKET_SIZE - (4 * NUM_PACKETS) - 24;
	//The aggregated packet that will be sent
	Byte pAggregatedPacket[MAX_PACKET_SIZE] = {0};

	uint32_t nIPv4DSTAddr;
	size_t pIpPacketsSizes[NUM_PACKETS];

	//initialize the packets
	for (int i = 0; i < NUM_PACKETS; i++)
	{
		if (NUM_PACKETS - 1 == i)
			pPacketsSizes[i] = nTotalPacketsSize;
		else
			pPacketsSizes[i] = nTotalPacketsSize / NUM_PACKETS;
		while (0 != pPacketsSizes[i] % 4)
		{
			pPacketsSizes[i]++;
		}
		nTotalPacketsSize -= pPacketsSizes[i];
		// Load input data (Eth packet) from file
		pIpPacketsSizes[i] = MAX_PACKET_SIZE;
		if (!LoadEtherPacket(m_eIP, pExpectedPackets[i],
				pIpPacketsSizes[i]))
		{
			LOG_MSG_ERROR("Failed default Packet");
			return false;
		}
		nIPv4DSTAddr = ntohl(0x7F000001);
		memcpy (&pExpectedPackets[i][IPV4_DST_ADDR_OFFSET_IN_ETH],&nIPv4DSTAddr,
				sizeof(nIPv4DSTAddr));
		int size = pIpPacketsSizes[i];
		while (size < pPacketsSizes[i])
		{
			pExpectedPackets[i][size] = i;
			size++;
		}
	}

	//initializing the aggregated packet
	AggregatePackets(pAggregatedPacket, pExpectedPackets, pPacketsSizes,
			NUM_PACKETS, MAX_PACKET_SIZE);

	//send the aggregated packet
	LOG_MSG_DEBUG("Sending aggregated packet into the USB pipe(%d bytes)\n",
			sizeof(pAggregatedPacket));
	int nBytesSent = input->Send(pAggregatedPacket, sizeof(pAggregatedPacket));
	if (sizeof(pAggregatedPacket) != nBytesSent)
	{
		LOG_MSG_DEBUG("Sending aggregated packet into the USB pipe(%d bytes) "
				"failed!\n", sizeof(pAggregatedPacket));
		return false;
	}

	//receive the packets
	for (int i = 0; i < NUM_PACKETS; i++)
	{
		LOG_MSG_DEBUG("Reading packet %d from the USB pipe(%d bytes should be "
				"there)\n", i, pPacketsSizes[i]);
		int nBytesReceived = output->Receive(pReceivedPackets[i],
				pPacketsSizes[i]);
		if (pPacketsSizes[i] != nBytesReceived)
		{
			LOG_MSG_DEBUG("Receiving packet %d from the USB pipe(%d bytes) "
					"failed!\n", i, pPacketsSizes[i]);
			print_buff(pReceivedPackets[i], nBytesReceived);
			return false;
		}
	}

	//comparing the received packet to the aggregated packet
	LOG_MSG_DEBUG("Checking sent.vs.received packet\n");
	for (int i = 0; i < NUM_PACKETS; i++)
		bTestResult &= !memcmp(pExpectedPackets[i], pReceivedPackets[i],
				pPacketsSizes[i]);

	return bTestResult;
}

/////////////////////////////////////////////////////////////////////////////////

bool NCMAggregationScenarios::NCMDeaggregationOnePacketTest(
		Pipe* input, Pipe* output,
		enum ipa_ip_type m_eIP)
{
	bool bTestResult = true;
	//The packets that the aggregated packet will be made of
	Byte pExpectedPackets[1][MAX_PACKET_SIZE];
	//The real sizes of the packets that the aggregated packet will be made of
	int pPacketsSizes[1] = {100};
	//Buffers for the packets that will be received
	Byte pReceivedPackets[1][MAX_PACKET_SIZE];
	//Total size of the aggregated packet
	//(this is the max size of the aggregated packet
	//minus the size of the header and the NDP)
	int nTotalAggregatedPacketSize = 100 + 12 + 16;
	//The aggregated packet that will be sent
	Byte pAggregatedPacket[MAX_PACKET_SIZE] = {0};

	uint32_t nIPv4DSTAddr;
	size_t pIpPacketsSizes[1];

	// Load input data (Eth packet) from file
	pIpPacketsSizes[0] = 100;
	if (!LoadEtherPacket(m_eIP, pExpectedPackets[0], pIpPacketsSizes[0]))
	{
		LOG_MSG_ERROR("Failed default Packet");
		return false;
	}
	nIPv4DSTAddr = ntohl(0x7F000001);
	memcpy (&pExpectedPackets[0][IPV4_DST_ADDR_OFFSET_IN_ETH],&nIPv4DSTAddr,
			sizeof(nIPv4DSTAddr));
	int size = pIpPacketsSizes[0];
	while (size < pPacketsSizes[0])
	{
		pExpectedPackets[0][size] = 0;
		size++;
	}


	//initializing the aggregated packet
	AggregatePackets(pAggregatedPacket, pExpectedPackets, pPacketsSizes, 1,
			nTotalAggregatedPacketSize);

	//send the aggregated packet
	LOG_MSG_DEBUG("Sending aggregated packet into the USB pipe(%d bytes)\n",
			nTotalAggregatedPacketSize);
	int nBytesSent = input->Send(pAggregatedPacket, nTotalAggregatedPacketSize);
	if (nTotalAggregatedPacketSize != nBytesSent)
	{
		LOG_MSG_DEBUG("Sending aggregated packet into the USB pipe(%d bytes) "
				"failed!\n", nTotalAggregatedPacketSize);
		return false;
	}

	//receive the packet
	for (int i = 0; i < 1; i++)
	{
		LOG_MSG_DEBUG("Reading packet %d from the USB pipe(%d bytes should be "
				"there)\n", i, pPacketsSizes[i]);
		int nBytesReceived = output->Receive(pReceivedPackets[i],
				pPacketsSizes[i]);
		if (pPacketsSizes[i] != nBytesReceived)
		{
			LOG_MSG_DEBUG("Receiving packet %d from the USB pipe(%d bytes) "
					"failed!\n", i, pPacketsSizes[i]);
			print_buff(pReceivedPackets[i], nBytesReceived);
			return false;
		}
	}

	//comparing the received packet to the aggregated packet
	LOG_MSG_DEBUG("Checking sent.vs.received packet\n");
	for (int i = 0; i < 1; i++)
		bTestResult &= !memcmp(pExpectedPackets[i], pReceivedPackets[i],
				pPacketsSizes[i]);

	return bTestResult;
}

/////////////////////////////////////////////////////////////////////////////////

bool NCMAggregationScenarios::NCMDeaggregationAndAggregationTest(
		Pipe* input, Pipe* output,
		enum ipa_ip_type m_eIP)
{
	//The packets that the aggregated packet will be made of
	Byte pPackets[NUM_PACKETS][MAX_PACKET_SIZE];
	//The real sizes of the packets that the aggregated packet will be made of
	int pPacketsSizes[NUM_PACKETS];
	//Buffers for the packets that will be received
	Byte pReceivedPacket[MAX_PACKET_SIZE];
	//Total size of all the packets that the aggregated packet will be made of
	//(this is the max size of the aggregated packet
	//minus the size of the header and the NDP)
	int nTotalPacketsSize = MAX_PACKET_SIZE - (4 * NUM_PACKETS) - 24;
	//The aggregated packet that will be sent
	Byte pAggregatedPacket[MAX_PACKET_SIZE] = {0};
	uint32_t nIPv4DSTAddr;
	size_t pIpPacketsSizes[NUM_PACKETS];

	//initialize the packets
	for (int i = 0; i < NUM_PACKETS; i++)
	{
		if (NUM_PACKETS - 1 == i)
			pPacketsSizes[i] = nTotalPacketsSize;
		else
			pPacketsSizes[i] = nTotalPacketsSize / NUM_PACKETS;
		while (0 != pPacketsSizes[i] % 4)
			pPacketsSizes[i]++;
		nTotalPacketsSize -= pPacketsSizes[i];

		// Load input data (Eth packet) from file
		pIpPacketsSizes[i] = MAX_PACKET_SIZE;
		if (!LoadEtherPacket(m_eIP, pPackets[i], pIpPacketsSizes[i]))
		{
			LOG_MSG_ERROR("Failed default Packet");
			return false;
		}
		nIPv4DSTAddr = ntohl(0x7F000001);
		memcpy (&pPackets[i][IPV4_DST_ADDR_OFFSET_IN_ETH],&nIPv4DSTAddr,
				sizeof(nIPv4DSTAddr));
		int size = pIpPacketsSizes[i];
		while (size < pPacketsSizes[i])
		{
			pPackets[i][size] = i;
			size++;
		}
	}

	//initializing the aggregated packet
	AggregatePackets(pAggregatedPacket, pPackets, pPacketsSizes, NUM_PACKETS,
			MAX_PACKET_SIZE);

	//send the aggregated packet
	LOG_MSG_DEBUG("Sending aggregated packet into the USB pipe(%d bytes)\n",
			MAX_PACKET_SIZE);
	int nBytesSent = input->Send(pAggregatedPacket, MAX_PACKET_SIZE);
	if (MAX_PACKET_SIZE != nBytesSent)
	{
		LOG_MSG_DEBUG("Sending aggregated packet into the USB pipe(%d bytes) "
				"failed!\n", MAX_PACKET_SIZE);
		return false;
	}

	//receive the aggregated packet
	LOG_MSG_DEBUG("Reading aggregated packet from the USB pipe(%d bytes should "
			"be there)\n", MAX_PACKET_SIZE);
	int nBytesReceived = output->Receive(pReceivedPacket, MAX_PACKET_SIZE);
	if (MAX_PACKET_SIZE != nBytesReceived)
	{
		LOG_MSG_DEBUG("Receiving aggregated packet from the USB pipe(%d bytes) "
				"failed!\n", MAX_PACKET_SIZE);
		LOG_MSG_DEBUG("Received %d bytes\n", nBytesReceived);
		print_buff(pReceivedPacket, nBytesReceived);
		return false;
	}


	//comparing the received packet to the aggregated packet
	LOG_MSG_DEBUG("Checking sent.vs.received packet\n");
	return DeaggragateAndComparePackets(pReceivedPacket, pPackets, pPacketsSizes,
			NUM_PACKETS, nBytesReceived);
}

/////////////////////////////////////////////////////////////////////////////////

bool NCMAggregationScenarios::NCMMultipleDeaggregationAndAggregationTest(
		Pipe* input, Pipe* output,
		enum ipa_ip_type m_eIP)
{
	//The packets that the aggregated packets will be made of
	Byte pPackets[NUM_PACKETS][MAX_PACKET_SIZE];
	//The real sizes of the packets that the aggregated packet will be made of
	int pPacketsSizes[NUM_PACKETS];
	//Buffers for the packets that will be received
	Byte pReceivedPacket[MAX_PACKET_SIZE];
	//Total size of all the packets that the aggregated packet will be made of
	//(this is the max size of the aggregated packet
	//minus the size of the header and the NDP)
	int nTotalPacketsSize = MAX_PACKET_SIZE - (4 * NUM_PACKETS) - 24;
	//The aggregated packet that will be sent
	Byte pAggregatedPacket[NUM_PACKETS][MAX_PACKET_SIZE];
	uint32_t nIPv4DSTAddr;
	size_t pIpPacketsSizes[NUM_PACKETS];

	//initialize the packets
	for (int i = 0; i < NUM_PACKETS; i++)
	{
		if (NUM_PACKETS - 1 == i)
			pPacketsSizes[i] = nTotalPacketsSize;
		else
			pPacketsSizes[i] = nTotalPacketsSize / NUM_PACKETS;
		while (0 != pPacketsSizes[i] % 4)
			pPacketsSizes[i]++;
		nTotalPacketsSize -= pPacketsSizes[i];

		// Load input data (Eth packet) from file
		pIpPacketsSizes[i] = MAX_PACKET_SIZE;
		if (!LoadEtherPacket(m_eIP, pPackets[i], pIpPacketsSizes[i]))
		{
			LOG_MSG_ERROR("Failed default Packet");
			return false;
		}
		nIPv4DSTAddr = ntohl(0x7F000001);
		memcpy (&pPackets[i][IPV4_DST_ADDR_OFFSET_IN_ETH],&nIPv4DSTAddr,
				sizeof(nIPv4DSTAddr));
		int size = pIpPacketsSizes[i];
		while (size < pPacketsSizes[i])
		{
			pPackets[i][size] = i;
			size++;
		}

	}

	//initializing the aggregated packets
	for (int i = 0; i < NUM_PACKETS; i++)
		AggregatePackets(pAggregatedPacket[i], &pPackets[i], &pPacketsSizes[i],
				1, pPacketsSizes[i] + 12 + 16);

	//send the aggregated packets
	for (int i = 0; i < NUM_PACKETS; i++)
	{
		LOG_MSG_DEBUG("Sending aggregated packet %d into the USB pipe(%d "
				"bytes)\n", i, pPacketsSizes[i] + 12 + 16);
		int nBytesSent = input->Send(pAggregatedPacket[i],
				pPacketsSizes[i] + 12 + 16);
		if (pPacketsSizes[i] + 12 + 16 != nBytesSent)
		{
			LOG_MSG_DEBUG("Sending aggregated packet %d into the USB pipe(%d "
					"bytes) failed!\n", i, pPacketsSizes[i] + 12 + 16);
			return false;
		}
	}

	//receive the aggregated packet
	LOG_MSG_DEBUG("Reading aggregated packet from the USB pipe(%d bytes should "
			"be there)\n", MAX_PACKET_SIZE);
	int nBytesReceived = output->Receive(pReceivedPacket, MAX_PACKET_SIZE);
	if (MAX_PACKET_SIZE != nBytesReceived)
	{
		LOG_MSG_DEBUG("Receiving aggregated packet from the USB pipe(%d bytes) "
				"failed!\n", MAX_PACKET_SIZE);
		LOG_MSG_DEBUG("Received %d bytes\n", nBytesReceived);
		print_buff(pReceivedPacket, nBytesReceived);
		return false;
	}


	//comparing the received packet to the aggregated packet
	LOG_MSG_DEBUG("Checking sent.vs.received packet\n");
	return DeaggragateAndComparePackets(pReceivedPacket, pPackets,
			pPacketsSizes, NUM_PACKETS, nBytesReceived);
}

/////////////////////////////////////////////////////////////////////////////////

bool NCMAggregationScenarios::NCMAggregationLoopTest(
		Pipe* input, Pipe* output, enum ipa_ip_type m_eIP)
{
	//The packets that will be sent
	Byte pPackets[NUM_PACKETS][MAX_PACKET_SIZE];
	//The real sizes of the packets that will be sent
	int pPacketsSizes[NUM_PACKETS];
	//Buffer for the packet that will be received
	Byte pReceivedPacket[MAX_PACKET_SIZE];
	//Total size of all sent packets (this is the max size of the aggregated
	//packet minus the size of the header and the NDP)
	int nTotalPacketsSize = MAX_PACKET_SIZE - (4 * NUM_PACKETS) - 24;
	uint32_t nIPv4DSTAddr;
	size_t pIpPacketsSizes[NUM_PACKETS];

	//initialize the packets
	for (int i = 0; i < NUM_PACKETS; i++)
	{
		if (NUM_PACKETS - 1 == i)
			pPacketsSizes[i] = nTotalPacketsSize;
		else
			pPacketsSizes[i] = nTotalPacketsSize / NUM_PACKETS;
		while (0 != pPacketsSizes[i] % 4)
			pPacketsSizes[i]++;
		nTotalPacketsSize -= pPacketsSizes[i];

		// Load input data (Eth packet) from file
		pIpPacketsSizes[i] = MAX_PACKET_SIZE;
		if (!LoadEtherPacket(m_eIP, pPackets[i], pIpPacketsSizes[i]))
		{
			LOG_MSG_ERROR("Failed default Packet");
			return false;
		}
		nIPv4DSTAddr = ntohl(0x7F000001);
		memcpy (&pPackets[i][IPV4_DST_ADDR_OFFSET_IN_ETH],&nIPv4DSTAddr,
				sizeof(nIPv4DSTAddr));
		int size = pIpPacketsSizes[i];
		while (size < pPacketsSizes[i])
		{
			pPackets[i][size] = i;
			size++;
		}
	}

	int num_iters = AGGREGATION_LOOP - 1;
	for (int j = 0; j < num_iters; j++)
	{
		//send the packets
		for (int i = 0; i < NUM_PACKETS; i++)
		{
			LOG_MSG_DEBUG("Sending packet %d into the USB pipe(%d bytes)\n", i,
					pPacketsSizes[i]);
			int nBytesSent = input->Send(pPackets[i], pPacketsSizes[i]);
			if (pPacketsSizes[i] != nBytesSent)
			{
				LOG_MSG_DEBUG("Sending packet %d into the USB pipe(%d bytes) "
						"failed!\n", i, pPacketsSizes[i]);
				return false;
			}
		}

		memset(pReceivedPacket, 0, sizeof(pReceivedPacket));
		//receive the aggregated packet
		LOG_MSG_DEBUG("Reading packet from the USB pipe(%d bytes should be "
				"there)\n", MAX_PACKET_SIZE);
		int nBytesReceived = output->Receive(pReceivedPacket, MAX_PACKET_SIZE);
		if (MAX_PACKET_SIZE != nBytesReceived)
		{
			LOG_MSG_DEBUG("Receiving aggregated packet from the USB pipe(%d "
					"bytes) failed!\n", MAX_PACKET_SIZE);
			print_buff(pReceivedPacket, nBytesReceived);
			return false;
		}

		LOG_MSG_DEBUG("Checking sent.vs.received packet\n");
		if (false == DeaggragateAndComparePackets(pReceivedPacket, pPackets,
				pPacketsSizes, NUM_PACKETS, nBytesReceived))
		{
			LOG_MSG_DEBUG("Comparing aggregated packet failed!\n");
			return false;
		}

	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////

bool NCMAggregationScenarios::NCMAggregationTimeLimitTest(
		Pipe* input, Pipe* output,
		enum ipa_ip_type m_eIP)
{
	//The packets that will be sent
	Byte pPackets[1][MAX_PACKET_SIZE];
	//The real sizes of the packets that will be sent
	int pPacketsSizes[1] = {0};
	//Buffer for the packet that will be received
	Byte pReceivedPacket[MAX_PACKET_SIZE] = {0};
	//Size of aggregated packet
	int nTotalPacketsSize = 24;
	uint32_t nIPv4DSTAddr;
	size_t pIpPacketsSizes[1];

	//initialize the packets
	for (int i = 0; i < 1 ; i++)
	{
		pPacketsSizes[i] = 52 + 4*i;
		nTotalPacketsSize += pPacketsSizes[i] + 4; //size of the packet + 4 bytes for index and length

		// Load input data (Eth packet) from file
		pIpPacketsSizes[i] = MAX_PACKET_SIZE;
		if (!LoadEtherPacket(m_eIP, pPackets[i], pIpPacketsSizes[i]))
		{
			LOG_MSG_ERROR("Failed default Packet");
			return false;
		}
		nIPv4DSTAddr = ntohl(0x7F000001);
		memcpy (&pPackets[i][IPV4_DST_ADDR_OFFSET_IN_ETH],&nIPv4DSTAddr,
				sizeof(nIPv4DSTAddr));
		int size = pIpPacketsSizes[i];
		while (size < pPacketsSizes[i])
		{
			pPackets[i][size] = i;
			size++;
		}
	}
	int nAllPacketsSizes = 0;
	for (int i = 0; i < 1; i++)
		nAllPacketsSizes += pPacketsSizes[i];
	while (0 != nAllPacketsSizes % 4)
	{
		nAllPacketsSizes++;
		nTotalPacketsSize++;  //zero padding for NDP offset to be 4x
	}

	//send the packets
	for (int i = 0; i < 1; i++)
	{
		LOG_MSG_DEBUG("Sending packet %d into the USB pipe(%d bytes)\n", i,
				pPacketsSizes[i]);
		int nBytesSent = input->Send(pPackets[i], pPacketsSizes[i]);
		if (pPacketsSizes[i] != nBytesSent)
		{
			LOG_MSG_DEBUG("Sending packet %d into the USB pipe(%d bytes) "
					"failed!\n", i, pPacketsSizes[i]);
			return false;
		}
	}

	//receive the aggregated packet
	LOG_MSG_DEBUG("Reading packet from the USB pipe(%d bytes should be "
			"there)\n", nTotalPacketsSize);
	int nBytesReceived = output->Receive(pReceivedPacket, nTotalPacketsSize);
	// IPA HW may add padding to the packets to align to 4B
	if (nTotalPacketsSize > nBytesReceived)
	{
		LOG_MSG_DEBUG("Receiving aggregated packet from the USB pipe(%d bytes) "
				"failed!\n", nTotalPacketsSize);
		print_buff(pReceivedPacket, nBytesReceived);
		return false;
	}

	//comparing the received packet to the aggregated packet
	LOG_MSG_DEBUG("Checking sent.vs.received packet\n");
	if (false == DeaggragateAndComparePackets(pReceivedPacket, pPackets,
			pPacketsSizes, 1, nBytesReceived))
	{
		LOG_MSG_DEBUG("Comparing aggregated packet failed!\n");
		print_buff(pReceivedPacket, nBytesReceived);
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////

bool NCMAggregationScenarios::NCMAggregationByteLimitTest(
		Pipe* input, Pipe* output,
		enum ipa_ip_type m_eIP)
{
	//The packets that will be sent
	Byte pPackets[2][MAX_PACKET_SIZE];
	//The real sizes of the packets that will be sent
	int pPacketsSizes[2];
	//Buffer for the packet that will be received
	Byte pReceivedPacket[2*MAX_PACKET_SIZE] = {0};
	//Size of aggregated packet
	int nTotalPacketsSize = 24;
	uint32_t nIPv4DSTAddr;
	size_t pIpPacketsSizes[2];

	//initialize the packets
	for (int i = 0; i < 2; i++)
	{
		pPacketsSizes[i] = (MAX_PACKET_SIZE / 2) + 10;
		nTotalPacketsSize += pPacketsSizes[i] + 4;

		// Load input data (Eth packet) from file
		pIpPacketsSizes[i] = MAX_PACKET_SIZE;
		if (!LoadEtherPacket(m_eIP, pPackets[i], pIpPacketsSizes[i]))
		{
			LOG_MSG_ERROR("Failed default Packet");
			return false;
		}
		nIPv4DSTAddr = ntohl(0x7F000001);
		memcpy (&pPackets[i][IPV4_DST_ADDR_OFFSET_IN_ETH],&nIPv4DSTAddr,
				sizeof(nIPv4DSTAddr));
		int size = pIpPacketsSizes[i];
		while (size < pPacketsSizes[i])
		{
			pPackets[i][size] = i;
			size++;
		}
	}


	//send the packets
	for (int i = 0; i < 2; i++)
	{
		LOG_MSG_DEBUG("Sending packet %d into the USB pipe(%d bytes)\n", i,
				pPacketsSizes[i]);
		int nBytesSent = input->Send(pPackets[i], pPacketsSizes[i]);
		if (pPacketsSizes[i] != nBytesSent)
		{
			LOG_MSG_DEBUG("Sending packet %d into the USB pipe(%d bytes) "
					"failed!\n", i, pPacketsSizes[i]);
			return false;
		}
	}

	//receive the aggregated packet
	LOG_MSG_DEBUG("Reading packet from the USB pipe(%d bytes should be "
			"there)\n", nTotalPacketsSize);
	int nBytesReceived = output->Receive(pReceivedPacket, nTotalPacketsSize);
	// IPA HW may add padding to the packets to align to 4B
	if (nTotalPacketsSize > nBytesReceived)
	{
		LOG_MSG_DEBUG("Receiving aggregated packet from the USB pipe(%d bytes) "
				"failed!\n", nTotalPacketsSize);
		print_buff(pReceivedPacket, nBytesReceived);
		return false;
	}

	//comparing the received packet to the aggregated packet
	LOG_MSG_DEBUG("Checking sent.vs.received packet\n");
	if (false == DeaggragateAndComparePackets(pReceivedPacket, pPackets,
			pPacketsSizes, 2, nBytesReceived))
	{
		LOG_MSG_DEBUG("Comparing aggregated packet failed!\n");
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////

bool NCMAggregationScenarios::NCMAggregationByteLimitTestFC(
	Pipe *input, Pipe *output,
	enum ipa_ip_type m_eIP)
{
	//The packets that will be sent
	Byte pPackets[2][MAX_PACKET_SIZE];
	//The real sizes of the packets that will be sent
	int pPacketsSizes[2];
	//Buffer for the packet that will be received
	Byte pReceivedPacket[2][MAX_PACKET_SIZE] = {0};
	//Size of aggregated packet
	int nTotalPacketsSize = 24 + (MAX_PACKET_SIZE / 2) + 8 + 4;
	uint32_t nIPv4DSTAddr;
	size_t pIpPacketsSizes[2];
	int nBytesReceived;

	for (int i = 0; i < 2; i++)
	{
		pPacketsSizes[i] = (MAX_PACKET_SIZE / 2) + 8;

		// Load input data (Eth packet) from file
		pIpPacketsSizes[i] = MAX_PACKET_SIZE;
		if (!LoadEtherPacket(m_eIP, pPackets[i], pIpPacketsSizes[i]))
		{
			LOG_MSG_ERROR("Failed default Packet");
			return false;
		}
		nIPv4DSTAddr = ntohl(0x7F000001);
		memcpy (&pPackets[i][IPV4_DST_ADDR_OFFSET_IN_ETH],&nIPv4DSTAddr,
				sizeof(nIPv4DSTAddr));
		int size = pIpPacketsSizes[i];
		while (size < pPacketsSizes[i])
		{
			pPackets[i][size] = i;
			size++;
		}
	}


	//send the packets
	for (int i = 0; i < 2; i++)
	{
		LOG_MSG_DEBUG("Sending packet %d into the USB pipe(%d bytes)\n", i,
				pPacketsSizes[i]);
		int nBytesSent = input->Send(pPackets[i], pPacketsSizes[i]);
		if (pPacketsSizes[i] != nBytesSent)
		{
			LOG_MSG_DEBUG("Sending packet %d into the USB pipe(%d bytes) "
					"failed!\n", i, pPacketsSizes[i]);
			return false;
		}
	}

	/* receive the packet */
	LOG_MSG_DEBUG(
		"Reading packets from the USB pipe(%d bytes for each)"
		"\n", nTotalPacketsSize);
	for (int i = 0; i < 2; i++)
	{
		nBytesReceived = output->Receive(pReceivedPacket[i], MAX_PACKET_SIZE);
		if (nTotalPacketsSize != nBytesReceived)
		{
			LOG_MSG_ERROR(
				"Receiving aggregated packet from the USB pipe(%d bytes) "
				"failed!\n", nBytesReceived);
			print_buff(pReceivedPacket[i], nBytesReceived);
			return false;
		}
	}

	//comparing the received packets to the aggregated packets
	LOG_MSG_DEBUG("Checking sent.vs.received packets\n");
	for (int i = 0; i < 2; i++)
	{
		if (false == DeaggragateAndComparePackets(pReceivedPacket[i],
							  &pPackets[i],
							  (int *)&pPacketsSizes[i],
							  1,
							  nBytesReceived))
		{
			LOG_MSG_DEBUG("Comparing aggregated packet failed!\n");
			return false;
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
#define DUAL_FC_ETH_PACKET_L ((MAX_PACKET_SIZE / 2) + 8)
#define DUAL_FC_1_AGG_PACKET_L (12 + DUAL_FC_ETH_PACKET_L + 12 + 4)
#define DUAL_FC_2_AGG_PACKET_L (12 + DUAL_FC_ETH_PACKET_L + DUAL_FC_ETH_PACKET_L + 12 + 4 + 4)
bool NCMAggregationScenarios::NCMAggregationDualDpTestFC(
	Pipe *input, Pipe *output1, Pipe *output2,
	enum ipa_ip_type m_eIP)
{
	int i;
	//The packets that will be sent
	Byte pPackets[4][MAX_PACKET_SIZE];
	//The real sizes of the packets that will be sent
	int pPacketsSizes[4];
	//Buffer for the packet that will be received
	Byte pReceivedPacket[2 * MAX_PACKET_SIZE] = {0};
	Byte pReceivedPacketFC[2][MAX_PACKET_SIZE] = {0};
	uint32_t nIPv4DSTAddr;
	size_t pIpPacketsSizes[4];
	int nBytesReceived;

	for (i = 0; i < 4; i++)
	{
		pPacketsSizes[i] = DUAL_FC_ETH_PACKET_L;

		// Load input data (Eth packet) from file
		pIpPacketsSizes[i] = MAX_PACKET_SIZE;
		if (!LoadEtherPacket(m_eIP, pPackets[i], pIpPacketsSizes[i]))
		{
			LOG_MSG_ERROR("Failed default Packet");
			return false;
		}
		nIPv4DSTAddr = ntohl(0x7F000001 + (i & 0x1));
		memcpy(&pPackets[i][IPV4_DST_ADDR_OFFSET_IN_ETH], &nIPv4DSTAddr,
		       sizeof(nIPv4DSTAddr));
		int size = pIpPacketsSizes[i];
		while (size < pPacketsSizes[i])
		{
			pPackets[i][size] = 0xAA;
			size++;
		}
	}

	//send the packets
	for (int i = 0; i < 4; i++)
	{
		LOG_MSG_DEBUG("Sending packet %d into the USB pipe(%d bytes)\n", i,
				pPacketsSizes[i]);
		int nBytesSent = input->Send(pPackets[i], pPacketsSizes[i]);
		if (pPacketsSizes[i] != nBytesSent)
		{
			LOG_MSG_DEBUG("Sending packet %d into the USB pipe(%d bytes) "
					"failed!\n", i, pPacketsSizes[i]);
			return false;
		}
	}

	/* receive the packets from FC pipe */
	LOG_MSG_DEBUG(
		"Reading packets from the FC pipe (%d bytes for each)"
		"\n", DUAL_FC_1_AGG_PACKET_L);
	for (i = 0; i < 2; i++)
	{
		nBytesReceived = output1->Receive(pReceivedPacketFC[i], MAX_PACKET_SIZE);
		if (DUAL_FC_1_AGG_PACKET_L != nBytesReceived)
		{
			LOG_MSG_ERROR(
				"Receiving aggregated packet from the USB pipe (%d bytes) "
				"failed!\n", nBytesReceived);
			print_buff(pReceivedPacketFC[i], nBytesReceived);
			return false;
		}
	}

	for (i = 0; i < 2; i++)
	{
		if (false == DeaggragateAndComparePackets(pReceivedPacketFC[i],
							  &pPackets[i * 2],
							  (int *)&pPacketsSizes[i * 2],
							  1,
							  nBytesReceived))
		{
			LOG_MSG_DEBUG("Comparing aggregated packet failed!\n");
			return false;
		}
	}

	/* receive the packet from non-FC pipe */
	LOG_MSG_DEBUG(
		"Reading packet from the non-FC pipe (%d bytes)"
		"\n", DUAL_FC_2_AGG_PACKET_L);
	nBytesReceived = output2->Receive(pReceivedPacket, MAX_PACKET_SIZE);
	if (DUAL_FC_2_AGG_PACKET_L != nBytesReceived)
	{
		LOG_MSG_ERROR(
			"Receiving aggregated packet from the USB pipe (%d bytes) "
			"failed!\n", nBytesReceived);
		print_buff(pReceivedPacket, nBytesReceived);
		return false;
	}

	// Setting all source packets IP to 127.0.0.2 for comparison
	nIPv4DSTAddr = ntohl(0x7F000002);
	memcpy(&pPackets[0][IPV4_DST_ADDR_OFFSET_IN_ETH], &nIPv4DSTAddr, sizeof(nIPv4DSTAddr));
	memcpy(&pPackets[2][IPV4_DST_ADDR_OFFSET_IN_ETH], &nIPv4DSTAddr, sizeof(nIPv4DSTAddr));

	if (false == DeaggragateAndComparePackets(&pReceivedPacket[0], pPackets,
			(int *)&pPacketsSizes, 2, nBytesReceived))
	{
		LOG_MSG_DEBUG("Comparing aggregated packet failed!\n");
		print_buff(pReceivedPacket, nBytesReceived);
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////

bool NCMAggregationScenarios::NCMDeaggregationMultipleNDPTest(
		Pipe* input, Pipe* output,
		enum ipa_ip_type m_eIP)
{
	bool bTestResult = true;
	//The packets that the aggregated packet will be made of
	Byte pExpectedPackets[NUM_PACKETS][MAX_PACKET_SIZE];
	//The real sizes of the packets that the aggregated packet will be made of
	int pPacketsSizes[NUM_PACKETS];
	//Buffers for the packets that will be received
	Byte pReceivedPackets[NUM_PACKETS][MAX_PACKET_SIZE];
	//Total size of all the packets that the aggregated packet will be made of
	//(this is the max size of the aggregated packet
	//minus the size of the header and the 2 NDPs)
	int nTotalPacketsSize = MAX_PACKET_SIZE - (4 * NUM_PACKETS) - 36;
	//The aggregated packet that will be sent
	Byte pAggregatedPacket[MAX_PACKET_SIZE] = {0};
	//The stream Id byte for every packet - this will determine on which NDP the
	//packet will appear
	Byte pPacketsStreamId[NUM_PACKETS] = {0};
	uint32_t nIPv4DSTAddr;
	size_t pIpPacketsSizes[NUM_PACKETS];

	//initialize the packets
	for (int i = 0; i < NUM_PACKETS; i++)
	{
		if (NUM_PACKETS - 1 == i)
			pPacketsSizes[i] = nTotalPacketsSize;
		else {
			pPacketsSizes[i] = nTotalPacketsSize / NUM_PACKETS;
			pPacketsSizes[i] += (pPacketsSizes[i] % 4 == 0 ? 0 :
				4 - pPacketsSizes[i] % 4);
		}
		nTotalPacketsSize -= pPacketsSizes[i];
		pPacketsStreamId[i] = i < 3 ? 0 : 1;

		// Load input data (Eth packet) from file
		pIpPacketsSizes[i] = MAX_PACKET_SIZE;
		if (!LoadEtherPacket(m_eIP, pExpectedPackets[i],
				pIpPacketsSizes[i]))
		{
			LOG_MSG_ERROR("Failed default Packet");
			return false;
		}
		nIPv4DSTAddr = ntohl(0x7F000001);
		memcpy (&pExpectedPackets[i][IPV4_DST_ADDR_OFFSET_IN_ETH],&nIPv4DSTAddr,
				sizeof(nIPv4DSTAddr));
		int size = pIpPacketsSizes[i];
		while (size < pPacketsSizes[i])
		{
			pExpectedPackets[i][size] = i;
			size++;
		}
	}

	//initializing the aggregated packet
	AggregatePacketsWithStreamId(pAggregatedPacket, pExpectedPackets,
			pPacketsSizes, NUM_PACKETS, MAX_PACKET_SIZE, pPacketsStreamId);

	//send the aggregated packet
	LOG_MSG_DEBUG("Sending aggregated packet into the USB pipe(%d bytes)\n",
			sizeof(pAggregatedPacket));
	int nBytesSent = input->Send(pAggregatedPacket, sizeof(pAggregatedPacket));
	if (sizeof(pAggregatedPacket) != nBytesSent)
	{
		LOG_MSG_DEBUG("Sending aggregated packet into the USB pipe(%d bytes) "
				"failed!\n", sizeof(pAggregatedPacket));
		return false;
	}

	//receive the packets
	for (int i = 0; i < NUM_PACKETS; i++)
	{
		LOG_MSG_DEBUG("Reading packet %d from the USB pipe(%d bytes should be "
				"there)\n", i, pPacketsSizes[i]);
		int nBytesReceived = output->Receive(pReceivedPackets[i],
				pPacketsSizes[i]);
		if (pPacketsSizes[i] != nBytesReceived)
		{
			LOG_MSG_DEBUG("Receiving packet %d from the USB pipe(%d bytes) "
					"failed!\n", i, pPacketsSizes[i]);
			print_buff(pReceivedPackets[i], nBytesReceived);
			return false;
		}
	}

	//comparing the received packet to the aggregated packet
	LOG_MSG_DEBUG("Checking sent.vs.received packet\n");
	for (int i = 0; i < NUM_PACKETS; i++)
		bTestResult &= !memcmp(pExpectedPackets[i], pReceivedPackets[i],
				pPacketsSizes[i]);

	return bTestResult;
}

/////////////////////////////////////////////////////////////////////////////////

bool NCMAggregationScenarios::NCMAggregationTimeLimitLoopTest(
		Pipe* input, Pipe* output,
		enum ipa_ip_type m_eIP)
{
	//The packets that will be sent
	Byte pPackets[1][MAX_PACKET_SIZE];
	//The real sizes of the packets that will be sent
	int pPacketsSizes[1] = {0};
	//Buffer for the packet that will be received
	Byte pReceivedPacket[MAX_PACKET_SIZE] = {0};
	//Size of aggregated packet
	int nTotalPacketsSize = 24;
	uint32_t nIPv4DSTAddr;
	size_t pIpPacketsSizes[NUM_PACKETS];

	//initialize the packets
	for (int i = 0; i < 1 ; i++)
	{
		pPacketsSizes[i] = 52 + 4*i;
		nTotalPacketsSize += pPacketsSizes[i] + 4; //size of the packet + 4 bytes for index and length

		// Load input data (Eth packet) from file
		pIpPacketsSizes[i] = MAX_PACKET_SIZE;
		if (!LoadEtherPacket(m_eIP, pPackets[i], pIpPacketsSizes[i]))
		{
			LOG_MSG_ERROR("Failed default Packet");
			return false;
		}
		nIPv4DSTAddr = ntohl(0x7F000001);
		memcpy (&pPackets[i][IPV4_DST_ADDR_OFFSET_IN_ETH],&nIPv4DSTAddr,
				sizeof(nIPv4DSTAddr));
		int size = pIpPacketsSizes[i];
		while (size < pPacketsSizes[i])
		{
			pPackets[i][size] = i;
			size++;
		}
	}
	int nAllPacketsSizes = 0;
	for (int i = 0; i < 1; i++)
		nAllPacketsSizes += pPacketsSizes[i];
	while (0 != nAllPacketsSizes % 4)
	{
		nAllPacketsSizes++;
		nTotalPacketsSize++;  //zero padding for NDP offset to be 4x
	}

	for (int k = 0; k < AGGREGATION_LOOP; k++)
	{
		//send the packets
		for (int i = 0; i < 1; i++)
		{
			LOG_MSG_DEBUG("Sending packet %d into the USB pipe(%d bytes)\n", i,
					pPacketsSizes[i]);
			int nBytesSent = input->Send(pPackets[i], pPacketsSizes[i]);
			if (pPacketsSizes[i] != nBytesSent)
			{
				LOG_MSG_DEBUG("Sending packet %d into the USB pipe(%d bytes) "
						"failed!\n", i, pPacketsSizes[i]);
				return false;
			}
		}

		//receive the aggregated packet
		LOG_MSG_DEBUG("Reading packet from the USB pipe(%d bytes should be "
				"there)\n", nTotalPacketsSize);
		int nBytesReceived = output->Receive(pReceivedPacket,
				nTotalPacketsSize);
		// IPA HW may add padding to the packets to align to 4B
		if (nTotalPacketsSize > nBytesReceived)
		{
			LOG_MSG_DEBUG("Receiving aggregated packet from the USB pipe(%d "
					"bytes) failed!\n", nTotalPacketsSize);
			print_buff(pReceivedPacket, nBytesReceived);
			return false;
		}

		//comparing the received packet to the aggregated packet
		LOG_MSG_DEBUG("Checking sent.vs.received packet\n");
		if (false == DeaggragateAndComparePackets(pReceivedPacket, pPackets,
				pPacketsSizes, 1, nBytesReceived))
		{
			LOG_MSG_DEBUG("Comparing aggregated packet failed!\n");
			return false;
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////

bool NCMAggregationScenarios::NCMAggregation0LimitsTest(
		Pipe* input, Pipe* output,
		enum ipa_ip_type m_eIP)
{
	//The packets that will be sent
	Byte pPackets[NUM_PACKETS][MAX_PACKET_SIZE];
	//The real sizes of the packets that will be sent
	int pPacketsSizes[NUM_PACKETS];
	//Buffer for the packet that will be received
	Byte pReceivedPackets[NUM_PACKETS][MAX_PACKET_SIZE];
	//The expected aggregated packets sizes
	int pAggragatedPacketsSizes[NUM_PACKETS] = {0};
	uint32_t nIPv4DSTAddr;
	size_t pIpPacketsSizes[NUM_PACKETS];

	//initialize the packets
	for (int i = 0; i < NUM_PACKETS ; i++)
	{
		pPacketsSizes[i] = 52 + 4*i;

		// Load input data (Eth packet) from file
		pIpPacketsSizes[i] = MAX_PACKET_SIZE;
		if (!LoadEtherPacket(m_eIP, pPackets[i], pIpPacketsSizes[i]))
		{
			LOG_MSG_ERROR("Failed default Packet");
			return false;
		}
		nIPv4DSTAddr = ntohl(0x7F000001);
		memcpy (&pPackets[i][IPV4_DST_ADDR_OFFSET_IN_ETH],&nIPv4DSTAddr,
				sizeof(nIPv4DSTAddr));
		int size = pIpPacketsSizes[i];
		while (size < pPacketsSizes[i])
		{
			pPackets[i][size] = i;
			size++;
		}
	}

	//calculate aggregated packets sizes
	for (int i = 0; i < NUM_PACKETS; i++)
	{
		pAggragatedPacketsSizes[i] += pPacketsSizes[i];
		while (0 != pAggragatedPacketsSizes[i] % 4)
			pAggragatedPacketsSizes[i]++;  //zero padding for NDP offset to be 4x
		pAggragatedPacketsSizes[i] += 28;  //header + NDP
	}

	//send the packets
	for (int i = 0; i < NUM_PACKETS; i++)
	{
		LOG_MSG_DEBUG("Sending packet %d into the USB pipe(%d bytes)\n", i,
				pPacketsSizes[i]);
		int nBytesSent = input->Send(pPackets[i], pPacketsSizes[i]);
		if (pPacketsSizes[i] != nBytesSent)
		{
			LOG_MSG_DEBUG("Sending packet %d into the USB pipe(%d bytes) "
					"failed!\n", i, pPacketsSizes[i]);
			return false;
		}
	}

	//receive the aggregated packets
	for (int i = 0; i < NUM_PACKETS; i++)
	{
		LOG_MSG_DEBUG("Reading packet %d from the USB pipe(%d bytes should be "
				"there)\n", i, pAggragatedPacketsSizes[i]);
		int nBytesReceived = output->Receive(pReceivedPackets[i],
				pAggragatedPacketsSizes[i]);
		// IPA HW may add padding to the packets to align to 4B
		if (pAggragatedPacketsSizes[i] > nBytesReceived)
		{
			LOG_MSG_DEBUG("Receiving aggregated packet %d from the USB pipe(%d "
					"bytes) failed!\n", i, pAggragatedPacketsSizes[i]);
			print_buff(pReceivedPackets[i], nBytesReceived);
			return false;
		}
		pAggragatedPacketsSizes[i] = nBytesReceived;
	}


	//comparing the received packet to the aggregated packet
	LOG_MSG_DEBUG("Checking sent.vs.received packet\n");
	for (int i = 0; i < NUM_PACKETS; i++)
	{
		if (false == DeaggragateAndCompareOnePacket(pReceivedPackets[i],
				pPackets[i], pPacketsSizes[i], pAggragatedPacketsSizes[i]))
		{
			LOG_MSG_DEBUG("Comparing aggregated packet %d failed!\n", i);
			return false;
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

bool NCMAggregationScenarios::DeaggragateAndComparePackets(
		Byte pAggregatedPacket[MAX_PACKET_SIZE],
		Byte pExpectedPackets[MAX_PACKETS_IN_NCM_TESTS][MAX_PACKET_SIZE],
		int pPacketsSizes[MAX_PACKETS_IN_NCM_TESTS], int nNumPackets, int nAggregatedPacketSize)
{
	int nPacketNum = 0;
	int i = 0;
	int nNdpStart = 0;
	Byte pNdpIndex[2] = {0};
	Byte pNdpLen[2] = {0};
	if (0x4e != pAggregatedPacket[i] || 0x43 != pAggregatedPacket[i+1] ||
			0x4d != pAggregatedPacket[i+2]|| 0x48 != pAggregatedPacket[i+3])
	{
		LOG_MSG_DEBUG("Error: Wrong NTH16 signature: 0x%02x 0x%02x 0x%02x "
				"0x%02x(should be 0x4e, 0x43, 0x4d, 0x48)\n",
				pAggregatedPacket[i], pAggregatedPacket[i+1],
				pAggregatedPacket[i+2], pAggregatedPacket[i+3]);
		return false;
	}
	i += 4;
	if (0x0c != pAggregatedPacket[i] || 0x00 != pAggregatedPacket[i+1])
	{
		LOG_MSG_DEBUG("Error: Wrong header length: 0x%02x 0x%02x(should be 0x0c, "
				"0x00)\n",
				pAggregatedPacket[i], pAggregatedPacket[i+1]);
		return false;
	}
	i += 4;  //ignoring sequence number
	if ((nAggregatedPacketSize & 0x00FF) != pAggregatedPacket[i] ||
			(nAggregatedPacketSize >> 8) != pAggregatedPacket[i+1])
	{
		LOG_MSG_DEBUG("Error: Wrong aggregated packet length: 0x%02x 0x%02x"
				"(should be 0x%02x, 0x%02x)\n",
				pAggregatedPacket[i], pAggregatedPacket[i+1],
				nAggregatedPacketSize & 0x00FF, nAggregatedPacketSize >> 8);
		return false;
	}
	i += 2;
	pNdpIndex[0] = pAggregatedPacket[i];  //least significant byte
	pNdpIndex[1] = pAggregatedPacket[i+1];  //most significant byte
	//reading the NDP
	while (0x00 != pNdpIndex[0] || 0x00 != pNdpIndex[1])
	{
		i = pNdpIndex[0] + 256*pNdpIndex[1];  //NDP should begin here
		nNdpStart = i;

		if (0x4E != pAggregatedPacket[i] || 0x43 != pAggregatedPacket[i + 1] ||
			0x4D != pAggregatedPacket[i + 2] || 0x30 != pAggregatedPacket[i + 3])
		{
			LOG_MSG_DEBUG("Error: Wrong NDP16 signature: 0x%02x 0x%02x "
				"0x%02x 0x%02x(should be 0x49, 0x50, 0x53, 0x00)\n",
				pAggregatedPacket[i], pAggregatedPacket[i + 1],
				pAggregatedPacket[i + 2], pAggregatedPacket[i + 3]);
			return false;
		}
		i += 4;
		pNdpLen[0] = pAggregatedPacket[i];  //least significant byte
		pNdpLen[1] = pAggregatedPacket[i+1];  //most significant byte
		if (0x00 != pAggregatedPacket[nNdpStart + pNdpLen[0] + 256*pNdpLen[1] - 2] ||
				0x00 != pAggregatedPacket[nNdpStart + pNdpLen[0] + 256*pNdpLen[1] -1])
		{
			LOG_MSG_DEBUG("Error: Wrong end of NDP: 0x%02x 0x%02x(should be 0x00,"
					" 0x00)\n",
					pAggregatedPacket[nNdpStart + pNdpLen[0] + 256*pNdpLen[1] - 2],
					pAggregatedPacket[nNdpStart + pNdpLen[0] + 256*pNdpLen[1] - 1]);
			return false;
		}
		i += 2;
		pNdpIndex[0] = pAggregatedPacket[i];  //least significant byte
		pNdpIndex[1] = pAggregatedPacket[i+1];  //most significant byte
		i += 2;
		while (i <= nNdpStart + pNdpLen[0] + 256*pNdpLen[1] - 2)
		{ //going over all the datagrams in this NDP
			Byte pDatagramIndex[2] = {0};
			Byte pDatagramLen[2] = {0};
			int packetIndex = 0;
			pDatagramIndex[0] = pAggregatedPacket[i];  //least significant byte
			pDatagramIndex[1] = pAggregatedPacket[i+1];  //most significant byte
			i += 2;
			if (0x00 == pDatagramIndex[0] && 0x00 == pDatagramIndex[1])
				break;  //zero padding after all datagrams
			if (nPacketNum >= nNumPackets)
			{
				LOG_MSG_DEBUG("Error: wrong number of packets: %d(should be %d)\n",
						nPacketNum, nNumPackets);
				return false;
			}
			pDatagramLen[0] = pAggregatedPacket[i];  //least significant byte
			pDatagramLen[1] = pAggregatedPacket[i+1];  //most significant byte
			i += 2;
			packetIndex = pDatagramIndex[0] + 256*pDatagramIndex[1];
			if (pDatagramLen[0] + 256*pDatagramLen[1] != pPacketsSizes[nPacketNum])
			{
				LOG_MSG_DEBUG("Error: Wrong packet %d length: 0x%02x 0x%02x"
						"(should be %d)\n", nPacketNum, pDatagramLen[0],
						pDatagramLen[1], pPacketsSizes[nPacketNum]);
				return false;
			}
			if (0 != memcmp(pExpectedPackets[nPacketNum],
					&pAggregatedPacket[packetIndex], pPacketsSizes[nPacketNum]))
			{
				LOG_MSG_DEBUG("Error: Comparison of packet %d failed!\n",
						nPacketNum);

				return false;
			}
			nPacketNum++;
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////

void NCMAggregationScenarios::AggregatePackets(
		Byte pAggregatedPacket[MAX_PACKET_SIZE]/*output*/,
		Byte pPackets[NUM_PACKETS][MAX_PACKET_SIZE],
		int pPacketsSizes[NUM_PACKETS], int nNumPackets,
		int nAggregatedPacketSize)
{
	int i = 0;
	int pDatagramIndexes[NUM_PACKETS] = {0};
	int nNdpIndex = 0;
	int nNdpLen = 0;
	//NTH16 signature
	pAggregatedPacket[i] = 0x4E;
	pAggregatedPacket[i+1] = 0x43;
	pAggregatedPacket[i+2] = 0x4D;
	pAggregatedPacket[i+3] = 0x48;
	i += 4;
	//header length
	pAggregatedPacket[i] = 0x0c;
	pAggregatedPacket[i+1] = 0x00;
	i += 2;
	//sequence number
	pAggregatedPacket[i] = 0x00;
	pAggregatedPacket[i+1] = 0x00;
	i += 2;
	//aggregated packet length
	pAggregatedPacket[i] = nAggregatedPacketSize & 0x00FF;
	pAggregatedPacket[i+1] = nAggregatedPacketSize >> 8;
	i += 2;
	//NDP index
	for (int j = 0; j < nNumPackets; j++)
		nNdpIndex += pPacketsSizes[j];
	nNdpIndex += i + 2;
	while (0 != nNdpIndex % 4)
		nNdpIndex++;
	pAggregatedPacket[i] = nNdpIndex & 0x00FF;
	pAggregatedPacket[i+1] = nNdpIndex >> 8;
	i += 2;
	//packets
	for (int j = 0; j < nNumPackets; j++)
	{
		pDatagramIndexes[j] = i;
		for (int k = 0; k < pPacketsSizes[j]; k++)
		{
			pAggregatedPacket[i] = pPackets[j][k];
			i++;
		}
	}
	while (i < nNdpIndex)
	{
		pAggregatedPacket[i] = 0x00;
		i++;
	}

	//NDP16 signature
	pAggregatedPacket[i] = 0x4E;
	pAggregatedPacket[i+1] = 0x43;
	pAggregatedPacket[i+2] = 0x4D;
	pAggregatedPacket[i+3] = 0x30;
	i += 4;
	//NDP length
	nNdpLen = 4*nNumPackets + 8 + 2;
	while (nNdpLen % 4 != 0)
		nNdpLen += 2;
	pAggregatedPacket[i] = nNdpLen & 0x00FF;
	pAggregatedPacket[i+1] = nNdpLen >> 8;
	i += 2;
	//next NDP
	pAggregatedPacket[i] = 0x00;
	pAggregatedPacket[i+1] = 0x00;
	i += 2;
	for (int j = 0; j < nNumPackets; j++)
	{
		//datagram index
		pAggregatedPacket[i] = pDatagramIndexes[j] & 0x00FF;
		pAggregatedPacket[i+1] = pDatagramIndexes[j] >> 8;
		i += 2;
		//datagram length
		pAggregatedPacket[i] = pPacketsSizes[j] & 0x00FF;
		pAggregatedPacket[i+1] = pPacketsSizes[j] >> 8;
		i += 2;
	}
	//zeros in the end of NDP
	while (i < nAggregatedPacketSize)
	{
		pAggregatedPacket[i] = 0x00;
		i++;
	}
}

/////////////////////////////////////////////////////////////////////////////////

void NCMAggregationScenarios::AggregatePacketsWithStreamId(
		Byte pAggregatedPacket[MAX_PACKET_SIZE]/*output*/,
		Byte pPackets[NUM_PACKETS][MAX_PACKET_SIZE],
		int pPacketsSizes[NUM_PACKETS], int nNumPackets, int nAggregatedPacketSize,
		Byte pPacketsStreamId[NUM_PACKETS])
{
	int i = 0;
	int n = 0;
	int pDatagramIndexes[NUM_PACKETS] = {0};
	int nNdpIndex[NUM_PACKETS] = {0};
	int nNdpLen = 0;
	Byte currStreamId = pPacketsStreamId[0];
	int nNdpFirstPacket[NUM_PACKETS] = {0};
	int nNdpAfterLastPacket[NUM_PACKETS] = {0};
	int nNumNDPs = 0;
	for (n = 0; n < nNumPackets; n++)
	{
		if (currStreamId != pPacketsStreamId[n])
		{
			nNdpAfterLastPacket[nNumNDPs] = n;
			nNumNDPs++;
			nNdpFirstPacket[nNumNDPs] = n;
			currStreamId = pPacketsStreamId[n];
		}
	}
	nNdpAfterLastPacket[nNumNDPs] = n;
	nNumNDPs++;
	//calculate NDP indexes
	nNdpIndex[0] += 12;  //adding the header
	for (int j = 0; j < nNumNDPs; j++)
	{
		for (n = nNdpFirstPacket[j]; n < nNdpAfterLastPacket[j]; n++)
			nNdpIndex[j] += pPacketsSizes[n];  //adding the packets
		while (0 != nNdpIndex[j] % 4)
			nNdpIndex[j]++;
		if (j < nNumNDPs - 1)
			nNdpIndex[j+1] += nNdpIndex[j] + 12 + 4*(nNdpAfterLastPacket[j] -
					nNdpFirstPacket[j]);  //adding the location after the current NDP to the next NDP
	}
	//start building the aggregated packet
	//NTH16 signature
	pAggregatedPacket[i] = 0x4E;
	pAggregatedPacket[i+1] = 0x43;
	pAggregatedPacket[i+2] = 0x4D;
	pAggregatedPacket[i+3] = 0x48;
	i += 4;
	//header length
	pAggregatedPacket[i] = 0x0c;
	pAggregatedPacket[i+1] = 0x00;
	i += 2;
	//sequence number
	pAggregatedPacket[i] = 0x00;
	pAggregatedPacket[i+1] = 0x00;
	i += 2;
	//aggregated packet length
	pAggregatedPacket[i] = nAggregatedPacketSize & 0x00FF;
	pAggregatedPacket[i+1] = nAggregatedPacketSize >> 8;
	i += 2;
	//first NDP index
	pAggregatedPacket[i] = nNdpIndex[0] & 0x00FF;
	pAggregatedPacket[i+1] = nNdpIndex[0] >> 8;
	i += 2;
	for (n = 0; n < nNumNDPs; n++)
	{
		//packets
		for (int j = nNdpFirstPacket[n]; j < nNdpAfterLastPacket[n]; j++)
		{
			pDatagramIndexes[j] = i;
			for (int k = 0; k < pPacketsSizes[j]; k++)
			{
				pAggregatedPacket[i] = pPackets[j][k];
				i++;
			}
		}
		while (i < nNdpIndex[n])
		{
			pAggregatedPacket[i] = 0x00;
			i++;
		}
		//NDP signature
		pAggregatedPacket[i] = 0x4E;
		pAggregatedPacket[i+1] = 0x43;
		pAggregatedPacket[i+2] = 0x4D;
		pAggregatedPacket[i+3] = 0x30; //pPacketsStreamId[nNdpFirstPacket[n]];
		i += 4;
		//NDP length
		nNdpLen = 4*(nNdpAfterLastPacket[n] - nNdpFirstPacket[n]) + 8 + 2;
		while (nNdpLen % 4 != 0)
			nNdpLen += 2;
		pAggregatedPacket[i] = nNdpLen & 0x00FF;
		pAggregatedPacket[i+1] = nNdpLen >> 8;
		i += 2;
		//next NDP
		pAggregatedPacket[i] = nNdpIndex[n+1] & 0x00FF;
		pAggregatedPacket[i+1] = nNdpIndex[n+1] >> 8;
		i += 2;
		for (int j = nNdpFirstPacket[n]; j < nNdpAfterLastPacket[n]; j++)
		{
			//datagram index
			pAggregatedPacket[i] = pDatagramIndexes[j] & 0x00FF;
			pAggregatedPacket[i+1] = pDatagramIndexes[j] >> 8;
			i += 2;
			//datagram length
			pAggregatedPacket[i] = pPacketsSizes[j] & 0x00FF;
			pAggregatedPacket[i+1] = pPacketsSizes[j] >> 8;
			i += 2;
		}
		//zeros in the end of NDP
		while (i < nNdpIndex[n] + nNdpLen)
		{
			pAggregatedPacket[i] = 0x00;
			i++;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////

bool NCMAggregationScenarios::DeaggragateAndCompareOnePacket(
		Byte pAggregatedPacket[MAX_PACKET_SIZE],
		Byte pExpectedPacket[MAX_PACKET_SIZE], int nPacketsSize,
		int nAggregatedPacketSize)
{
	int nPacketNum = 0;
	int i = 0;
	int nNdpStart = 0;
	Byte pNdpIndex[2] = {0};
	Byte pNdpLen[2] = {0};
	if (0x4e != pAggregatedPacket[i] || 0x43 != pAggregatedPacket[i+1] ||
			0x4d != pAggregatedPacket[i+2]|| 0x48 != pAggregatedPacket[i+3])
	{
		LOG_MSG_DEBUG("Error: Wrong NTH16 signature: 0x%02x 0x%02x 0x%02x "
				"0x%02x(should be 0x4e, 0x43, 0x4d, 0x48)\n",
				pAggregatedPacket[i], pAggregatedPacket[i+1],
				pAggregatedPacket[i+2], pAggregatedPacket[i+3]);
		return false;
	}
	i += 4;
	if (0x0c != pAggregatedPacket[i] || 0x00 != pAggregatedPacket[i+1])
	{
		LOG_MSG_DEBUG("Error: Wrong header length: 0x%02x 0x%02x(should be 0x0c,"
				" 0x00)\n", pAggregatedPacket[i], pAggregatedPacket[i+1]);
		return false;
	}
	i += 4;  //ignoring sequence number
	if ((nAggregatedPacketSize & 0x00FF) != pAggregatedPacket[i] ||
			(nAggregatedPacketSize >> 8) != pAggregatedPacket[i+1])
	{
		LOG_MSG_DEBUG("Error: Wrong aggregated packet length: 0x%02x 0x%02x"
				"(should be 0x%02x, 0x%02x)\n",
				pAggregatedPacket[i], pAggregatedPacket[i+1],
				nAggregatedPacketSize & 0x00FF, nAggregatedPacketSize >> 8);
		return false;
	}
	i += 2;
	pNdpIndex[0] = pAggregatedPacket[i];  //least significant byte
	pNdpIndex[1] = pAggregatedPacket[i+1];  //most significant byte
	//reading the NDP
	while (0x00 != pNdpIndex[0] || 0x00 != pNdpIndex[1])
	{
		i = pNdpIndex[0] + 256*pNdpIndex[1];  //NDP should begin here
		nNdpStart = i;

		if (0x4E != pAggregatedPacket[i] || 0x43 != pAggregatedPacket[i+1] ||
				0x4D != pAggregatedPacket[i+2] || 0x30 != pAggregatedPacket[i+3])
		{
			LOG_MSG_DEBUG("Error: Wrong NDP16 signature: 0x%02x 0x%02x "
					"0x%02x 0x%02x(should be 0x49, 0x50, 0x53, 0x00)\n",
					pAggregatedPacket[i], pAggregatedPacket[i+1],
					pAggregatedPacket[i+2], pAggregatedPacket[i+3]);
			return false;
		}
		i += 4;
		pNdpLen[0] = pAggregatedPacket[i];  //least significant byte
		pNdpLen[1] = pAggregatedPacket[i+1];  //most significant byte
		if (0x00 != pAggregatedPacket[nNdpStart + pNdpLen[0] + 256*pNdpLen[1] - 2] ||
				0x00 != pAggregatedPacket[nNdpStart + pNdpLen[0] + 256*pNdpLen[1] -1])
		{
			LOG_MSG_DEBUG("Error: Wrong end of NDP: 0x%02x 0x%02x(should be "
					"0x00, 0x00)\n",
					pAggregatedPacket[nNdpStart + pNdpLen[0] + 256*pNdpLen[1] - 2],
					pAggregatedPacket[nNdpStart + pNdpLen[0] + 256*pNdpLen[1] - 1]);
			return false;
		}
		i += 2;
		pNdpIndex[0] = pAggregatedPacket[i];  //least significant byte
		pNdpIndex[1] = pAggregatedPacket[i+1];  //most significant byte
		i += 2;
		while (i <= nNdpStart + pNdpLen[0] + 256*pNdpLen[1] - 2)
		{ //going over all the datagrams in this NDP
			Byte pDatagramIndex[2] = {0};
			Byte pDatagramLen[2] = {0};
			int packetIndex = 0;
			pDatagramIndex[0] = pAggregatedPacket[i];  //least significant byte
			pDatagramIndex[1] = pAggregatedPacket[i+1];  //most significant byte
			i += 2;
			if (0x00 == pDatagramIndex[0] && 0x00 == pDatagramIndex[1])
				break;  //zero padding after all datagrams
			if (nPacketNum > 1)
			{
				LOG_MSG_DEBUG("Error: wrong number of packets: %d(should be %d)\n",
						nPacketNum, 1);
				return false;
			}
			pDatagramLen[0] = pAggregatedPacket[i];  //least significant byte
			pDatagramLen[1] = pAggregatedPacket[i+1];  //most significant byte
			i += 2;
			packetIndex = pDatagramIndex[0] + 256*pDatagramIndex[1];
			if (pDatagramLen[0] + 256*pDatagramLen[1] != nPacketsSize)
			{
				LOG_MSG_DEBUG("Error: Wrong packet %d length: 0x%02x 0x%02x"
						"(should be %d)\n", nPacketNum, pDatagramLen[0],
						pDatagramLen[1], nPacketsSize);
				return false;
			}
			if (0 != memcmp(pExpectedPacket, &pAggregatedPacket[packetIndex],
					nPacketsSize))
			{
				LOG_MSG_DEBUG("Error: Comparison of packet %d failed!\n",
						nPacketNum);
				return false;
			}
			nPacketNum++;
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////

bool NCMAggregationScenarios::DeaggragateAndComparePacketsWithStreamId(
		Byte pAggregatedPacket[MAX_PACKET_SIZE],
		Byte pExpectedPackets[][MAX_PACKET_SIZE], int pPacketsSizes[],
		int nNumPackets, int nAggregatedPacketSize,
		Byte pPacketsStreamId[NUM_PACKETS])
{
	int nPacketNum = 0;
	int i = 0;
	int nNdpStart = 0;
	Byte pNdpIndex[2] = {0};
	Byte pNdpLen[2] = {0};
	if (0x4e != pAggregatedPacket[i] || 0x43 != pAggregatedPacket[i+1] ||
			0x4d != pAggregatedPacket[i+2]|| 0x48 != pAggregatedPacket[i+3])
	{
		LOG_MSG_DEBUG("Error: Wrong NTH16 signature: 0x%02x 0x%02x 0x%02x "
				"0x%02x(should be 0x4e, 0x43, 0x4d, 0x48)\n",
				pAggregatedPacket[i], pAggregatedPacket[i+1],
				pAggregatedPacket[i+2], pAggregatedPacket[i+3]);
		return false;
	}
	i += 4;
	if (0x0c != pAggregatedPacket[i] || 0x00 != pAggregatedPacket[i+1])
	{
		LOG_MSG_DEBUG("Error: Wrong header length: 0x%02x 0x%02x(should be "
				"0x0c, 0x00)\n",pAggregatedPacket[i], pAggregatedPacket[i+1]);
		return false;
	}
	i += 4;  //ignoring sequence number
	if ((nAggregatedPacketSize & 0x00FF) != pAggregatedPacket[i] ||
			(nAggregatedPacketSize >> 8) != pAggregatedPacket[i+1])
	{
		LOG_MSG_DEBUG("Error: Wrong aggregated packet length: 0x%02x 0x%02x"
				"(should be 0x%02x, 0x%02x)\n", pAggregatedPacket[i],
				pAggregatedPacket[i+1], nAggregatedPacketSize & 0x00FF,
				nAggregatedPacketSize >> 8);
		return false;
	}
	i += 2;
	pNdpIndex[0] = pAggregatedPacket[i];  //least significant byte
	pNdpIndex[1] = pAggregatedPacket[i+1];  //most significant byte
	//reading the NDP
	while (0x00 != pNdpIndex[0] || 0x00 != pNdpIndex[1])
	{
		i = pNdpIndex[0] + 256*pNdpIndex[1];  //NDP should begin here
		nNdpStart = i;
		if (0x4E != pAggregatedPacket[i] || 0x43 != pAggregatedPacket[i+1] ||
				0x4D != pAggregatedPacket[i+2])
		{
			LOG_MSG_DEBUG("Error: Wrong NDP16 signature: 0x%02x 0x%02x 0x%02x"
					"(should be 0x49, 0x50, 0x53)\n", pAggregatedPacket[i],
					pAggregatedPacket[i+1], pAggregatedPacket[i+2]);
			return false;
		}
		if (pPacketsStreamId[nPacketNum] != pAggregatedPacket[i+3])
		{
			LOG_MSG_DEBUG("Error: Wrong NDP stream id: 0x%02x(should be 0x%02x)\n",
					pAggregatedPacket[i+3], pPacketsStreamId[nPacketNum]);
			return false;
		}
		i += 4;
		pNdpLen[0] = pAggregatedPacket[i];  //least significant byte
		pNdpLen[1] = pAggregatedPacket[i+1];  //most significant byte
		if (0x00 != pAggregatedPacket[nNdpStart + pNdpLen[0] + 256*pNdpLen[1] - 2] ||
				0x00 != pAggregatedPacket[nNdpStart + pNdpLen[0] + 256*pNdpLen[1] -1])
		{
			LOG_MSG_DEBUG("Error: Wrong end of NDP: 0x%02x 0x%02x(should be 0x00, "
					"0x00)\n",
					pAggregatedPacket[nNdpStart + pNdpLen[0] + 256*pNdpLen[1] - 2],
					pAggregatedPacket[nNdpStart + pNdpLen[0] + 256*pNdpLen[1] - 1]);
			return false;
		}
		i += 2;
		pNdpIndex[0] = pAggregatedPacket[i];  //least significant byte
		pNdpIndex[1] = pAggregatedPacket[i+1];  //most significant byte
		i += 2;
		while (i <= nNdpStart + pNdpLen[0] + 256*pNdpLen[1] - 2)
		{ //going over all the datagrams in this NDP
			Byte pDatagramIndex[2] = {0};
			Byte pDatagramLen[2] = {0};
			int packetIndex = 0;
			pDatagramIndex[0] = pAggregatedPacket[i];  //least significant byte
			pDatagramIndex[1] = pAggregatedPacket[i+1];  //most significant byte
			i += 2;
			if (0x00 == pDatagramIndex[0] && 0x00 == pDatagramIndex[1])
				break;  //zero padding after all datagrams
			if (nPacketNum >= nNumPackets)
			{
				LOG_MSG_DEBUG("Error: wrong number of packets: %d(should be %d)\n",
						nPacketNum, nNumPackets);
				return false;
			}
			pDatagramLen[0] = pAggregatedPacket[i];  //least significant byte
			pDatagramLen[1] = pAggregatedPacket[i+1];  //most significant byte
			i += 2;
			packetIndex = pDatagramIndex[0] + 256*pDatagramIndex[1];
			if (pDatagramLen[0] + 256*pDatagramLen[1] != (int)pPacketsSizes[nPacketNum])
			{
				LOG_MSG_DEBUG("Error: Wrong packet %d length: 0x%02x 0x%02x"
						"(should be %d)\n", nPacketNum, pDatagramLen[0],
						pDatagramLen[1], pPacketsSizes[nPacketNum]);
				return false;
			}
			if (0 != memcmp(pExpectedPackets[nPacketNum],
					&pAggregatedPacket[packetIndex], pPacketsSizes[nPacketNum]))
			{
				LOG_MSG_DEBUG("Error: Comparison of packet %d failed!\n",
						nPacketNum);
				return false;
			}
			nPacketNum++;
		}
	}

	return true;
}


class NCMAggregationTest: public NCMAggregationTestFixture {
public:

	/////////////////////////////////////////////////////////////////////////////////

	NCMAggregationTest(bool generic_agg) :
		NCMAggregationTestFixture(generic_agg)
	{
		if (generic_agg)
			m_name = "GenNCMAggregationTest";
		else
			m_name = "NCMAggregationTest";
		m_description = "NCM Aggregation test - sends 5 packets and receives 1 "
				"aggregated packet";
	}

	/////////////////////////////////////////////////////////////////////////////////

	virtual bool AddRules(void)
	{
		return AddRules1HeaderAggregation();
	} // AddRules()

	/////////////////////////////////////////////////////////////////////////////////

	bool TestLogic(void)
	{
		return NCMAggregationScenarios::NCMAggregationTest(&m_UsbToIpaPipe,
				&m_IpaToUsbPipeAgg, m_eIP);
	}

	/////////////////////////////////////////////////////////////////////////////////
};

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

class NCMDeaggregationTest: public NCMAggregationTestFixture {
public:

	/////////////////////////////////////////////////////////////////////////////////

	NCMDeaggregationTest(bool generic_agg) :
		NCMAggregationTestFixture(generic_agg)
	{
		if (generic_agg)
			m_name = "GenNCMDeaggregationTest";
		else
			m_name = "NCMDeaggregationTest";
		m_description = "NCM Deaggregation test - sends an aggregated packet made from"
				"5 packets and receives 5 packets";
	}

	/////////////////////////////////////////////////////////////////////////////////

	virtual bool AddRules(void)
	{
		return AddRulesDeaggregation();
	} // AddRules()

	/////////////////////////////////////////////////////////////////////////////////

	bool TestLogic(void)
	{
		return NCMAggregationScenarios::NCMDeaggregationTest(&m_UsbToIpaPipeDeagg, &m_IpaToUsbPipe, m_eIP);
	}

	/////////////////////////////////////////////////////////////////////////////////
};

class NCMDeaggregationOnePacketTest: public NCMAggregationTestFixture {
public:

	/////////////////////////////////////////////////////////////////////////////////

	NCMDeaggregationOnePacketTest(bool generic_agg) :
		NCMAggregationTestFixture(generic_agg)
	{
		if (generic_agg)
			m_name = "GenNCMDeaggregationOnePacketTest";
		else
			m_name = "NCMDeaggregationOnePacketTest";
		m_description = "NCM Deaggregation one packet test - sends an aggregated packet made"
				"of 1 packet and receives 1 packet";
	}

	/////////////////////////////////////////////////////////////////////////////////

	virtual bool AddRules(void)
	{
		return AddRulesDeaggregation();
	} // AddRules()

	/////////////////////////////////////////////////////////////////////////////////

	bool TestLogic(void)
	{
		return NCMAggregationScenarios::NCMDeaggregationOnePacketTest(&m_UsbToIpaPipeDeagg, &m_IpaToUsbPipe, m_eIP);
	}

	/////////////////////////////////////////////////////////////////////////////////
};

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////


class NCMDeaggregationAndAggregationTest: public NCMAggregationTestFixture {
public:

	/////////////////////////////////////////////////////////////////////////////////

	NCMDeaggregationAndAggregationTest(bool generic_agg)
		: NCMAggregationTestFixture(generic_agg)
	{
		if (generic_agg)
			m_name = "GenNCMDeaggregationAndAggregationTest";
		else
			m_name = "NCMDeaggregationAndAggregationTest";
		m_description = "NCM Deaggregation and Aggregation test - sends an aggregated "
				"packet made from 5 packets and receives the same aggregated packet";
	}

	/////////////////////////////////////////////////////////////////////////////////

	virtual bool AddRules(void)
	{
		return AddRules1HeaderAggregation();
	} // AddRules()

	/////////////////////////////////////////////////////////////////////////////////

	bool TestLogic(void)
	{
		return NCMAggregationScenarios::NCMDeaggregationAndAggregationTest(
				&m_UsbToIpaPipeDeagg, &m_IpaToUsbPipeAgg, m_eIP);
	}

	/////////////////////////////////////////////////////////////////////////////////

};

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////


class NCMMultipleDeaggregationAndAggregationTest:
	public NCMAggregationTestFixture {
public:

	/////////////////////////////////////////////////////////////////////////////////

	NCMMultipleDeaggregationAndAggregationTest(bool generic_agg) :
		NCMAggregationTestFixture(generic_agg)
	{
		if (generic_agg)
			m_name = "GenNCMMultipleDeaggregationAndAggregationTest";
		else
			m_name = "NCMMultipleDeaggregationAndAggregationTest";
		m_description = "NCM Multiple Deaggregation and Aggregation test - sends 5 aggregated "
				"packets each one made of 1 packet and receives an aggregated packet made of the"
				"5 packets";
	}

	/////////////////////////////////////////////////////////////////////////////////

	virtual bool AddRules(void)
	{
		return AddRules1HeaderAggregation();
	} // AddRules()

	/////////////////////////////////////////////////////////////////////////////////

	bool TestLogic(void)
	{
		return NCMAggregationScenarios::NCMMultipleDeaggregationAndAggregationTest(
				&m_UsbToIpaPipeDeagg, &m_IpaToUsbPipeAgg, m_eIP);
	}

	/////////////////////////////////////////////////////////////////////////////////

};

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

class NCMAggregationLoopTest: public NCMAggregationTestFixture {
public:

	/////////////////////////////////////////////////////////////////////////////////

	NCMAggregationLoopTest(bool generic_agg)
		: NCMAggregationTestFixture(generic_agg)
	{
		if (generic_agg)
			m_name = "GenNCMggregationLoopTest";
		else
			m_name = "NCMggregationLoopTest";
		m_description = "NCM Aggregation Loop test - sends 5 packets and expects to"
				"receives 1 aggregated packet a few times";
	}

	/////////////////////////////////////////////////////////////////////////////////

	virtual bool AddRules(void)
	{
		return AddRules1HeaderAggregation();
	} // AddRules()

	/////////////////////////////////////////////////////////////////////////////////

	bool TestLogic(void)
	{
		return NCMAggregationScenarios::NCMAggregationLoopTest(
				&m_UsbToIpaPipe, &m_IpaToUsbPipeAgg, m_eIP);
	}

	/////////////////////////////////////////////////////////////////////////////////
};

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

class NCMAggregationTimeLimitTest: public NCMAggregationTestFixture {
public:

	/////////////////////////////////////////////////////////////////////////////////

	NCMAggregationTimeLimitTest(bool generic_agg)
		: NCMAggregationTestFixture(generic_agg)
	{
		if (generic_agg)
			m_name = "GenNCMAggregationTimeLimitTest";
		else
			m_name = "NCMAggregationTimeLimitTest";
		m_description = "NCM Aggregation time limit test - sends 1 small packet "
				"smaller than the byte limit and receives 1 aggregated packet";
	}

	/////////////////////////////////////////////////////////////////////////////////

	virtual bool AddRules(void)
	{
		return AddRules1HeaderAggregationTime();
	} // AddRules()

	/////////////////////////////////////////////////////////////////////////////////

	bool TestLogic(void)
	{
		return NCMAggregationScenarios::NCMAggregationTimeLimitTest(
				&m_UsbToIpaPipe, &m_IpaToUsbPipeAggTime, m_eIP);
	}

	/////////////////////////////////////////////////////////////////////////////////
};

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

class NCMAggregationByteLimitTest: public NCMAggregationTestFixture {
public:

	/////////////////////////////////////////////////////////////////////////////////

	NCMAggregationByteLimitTest(bool generic_agg)
		: NCMAggregationTestFixture(generic_agg)
	{
		if (generic_agg)
			m_name = "GenNCMAggregationByteLimitTest";
		else
			m_name = "NCMAggregationByteLimitTest";
		m_description = "NCM Aggregation byte limit test - sends 2 packets that together "
				"are larger than the byte limit ";
	}

	/////////////////////////////////////////////////////////////////////////////////

	virtual bool AddRules(void)
	{
		return AddRules1HeaderAggregation();
	} // AddRules()

	/////////////////////////////////////////////////////////////////////////////////

	bool TestLogic(void)
	{
		return NCMAggregationScenarios::NCMAggregationByteLimitTest(
				&m_UsbToIpaPipe, &m_IpaToUsbPipeAgg, m_eIP);
	}

	/////////////////////////////////////////////////////////////////////////////////
};

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

class NCMAggregationByteLimitTestFC: public NCMAggregationTestFixture {
public:

	/////////////////////////////////////////////////////////////////////////////////

	NCMAggregationByteLimitTestFC(bool generic_agg)
		: NCMAggregationTestFixture(generic_agg) {
		if (generic_agg)
			m_name = "GenNCMAggregationByteLimitTestFC";
		else
			m_name = "NCMAggregationByteLimitTestFC";
		m_description = "NCMAggregationByteLimitTestFC - Send 4 Eth packet with FC"
			"and expect 4 aggregated NCM packets.";
	}

	/////////////////////////////////////////////////////////////////////////////////

	virtual bool AddRules(void) {
		return AddRules1HeaderAggregation(true);
	} // AddRules()

	/////////////////////////////////////////////////////////////////////////////////

	bool TestLogic(void)
	{
		return NCMAggregationScenarios::NCMAggregationByteLimitTestFC(
				&m_UsbToIpaPipe, &m_IpaToUsbPipeAgg, m_eIP);
	}
};

class NCMAggregationDualDpTestFC : public NCMAggregationTestFixture
{
public:

	/////////////////////////////////////////////////////////////////////////////////

	NCMAggregationDualDpTestFC(bool generic_agg)
		: NCMAggregationTestFixture(generic_agg)
	{
		if (generic_agg) m_name = "GenNCMAggregationDualDpTestFC";
		else m_name = "NCMAggregationDualDpTestFC";
		m_description = "NCMAggregationDualDpTestFC - Send Eth packets "
			"on two datapathes: one with FC and one without. "
			"Expect 2 aggregated NCM packets on pipe with FC. "
			"Expect one aggregated NCM packet on pipe without FC. ";
	}

	/////////////////////////////////////////////////////////////////////////////////

	virtual bool AddRules(void)
	{
		return AddRulesAggDualFC(&m_UsbToIpaPipe,
					 &m_IpaToUsbPipeAggTime,
					 &m_IpaToUsbPipeAgg);
	}

	/////////////////////////////////////////////////////////////////////////////////

	bool TestLogic(void)
	{
		return NCMAggregationScenarios::NCMAggregationDualDpTestFC(
			&m_UsbToIpaPipe, &m_IpaToUsbPipeAggTime, &m_IpaToUsbPipeAgg, m_eIP);
	}
};

class NCMAggregationDualDpTestFcRoutingBased : public NCMAggregationTestFixture
{
public:

	/////////////////////////////////////////////////////////////////////////////////

	NCMAggregationDualDpTestFcRoutingBased(bool generic_agg)
		: NCMAggregationTestFixture(generic_agg)
	{
		if (generic_agg) m_name = "GenNCMAggregationDualDpTestFcRoutingBased";
		else m_name = "NCMAggregationDualDpTestFcRoutingBased";
		m_description = "NCMAggregationDualDpTestFcRoutingBased - Send Eth packets "
			"on two datapathes: one with RT based FC and one without. "
			"Expect 2 aggregated NCM packets on pipe with RT based FC. "
			"Expect one aggregated NCM packet on pipe without RT based FC. ";
	}

	/////////////////////////////////////////////////////////////////////////////////

	virtual bool AddRules(void)
	{
		return AddRulesAggDualFcRoutingBased(&m_UsbToIpaPipe,
					 &m_IpaToUsbPipeAggTime,
					 &m_IpaToUsbPipeAgg);
	}

	/////////////////////////////////////////////////////////////////////////////////

	bool TestLogic(void)
	{
		return NCMAggregationScenarios::NCMAggregationDualDpTestFC(
			&m_UsbToIpaPipe, &m_IpaToUsbPipeAggTime, &m_IpaToUsbPipeAgg, m_eIP);
	}
};

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

class NCMDeaggregationMultipleNDPTest: public NCMAggregationTestFixture {
public:

	/////////////////////////////////////////////////////////////////////////////////

	NCMDeaggregationMultipleNDPTest(bool generic_agg)
		: NCMAggregationTestFixture(generic_agg)
	{
		if (generic_agg)
			m_name = "GenNCMDeaggregationMultipleNDPTest";
		else
			m_name = "NCMDeaggregationMultipleNDPTest";
		m_description = "NCM Deaggregation multiple NDP test - sends an aggregated"
				"packet made from 5 packets and 2 NDPs and receives 5 packets";
	}

	/////////////////////////////////////////////////////////////////////////////////

	virtual bool AddRules(void)
	{
		return AddRulesDeaggregation();
	} // AddRules()

	/////////////////////////////////////////////////////////////////////////////////

	bool TestLogic(void)
	{
		return NCMAggregationScenarios::NCMDeaggregationMultipleNDPTest(
				&m_UsbToIpaPipeDeagg, &m_IpaToUsbPipe, m_eIP);
	}

	/////////////////////////////////////////////////////////////////////////////////
};

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

class NCMAggregationTimeLimitLoopTest: public NCMAggregationTestFixture {
public:

	/////////////////////////////////////////////////////////////////////////////////

	NCMAggregationTimeLimitLoopTest(bool generic_agg)
		: NCMAggregationTestFixture(generic_agg)
	{
		if (generic_agg)
			m_name = "GenNCMAggregationTimeLimitLoopTest";
		else
			m_name = "NCMAggregationTimeLimitLoopTest";
		m_description = "NCM Aggregation time limit loop test - sends 5 small packet "
				"smaller than the byte limit and receives 5 aggregated packet";
	}

	/////////////////////////////////////////////////////////////////////////////////

	virtual bool AddRules(void)
	{
		return AddRules1HeaderAggregationTime();
	} // AddRules()

	/////////////////////////////////////////////////////////////////////////////////

	bool TestLogic(void)
	{
		return NCMAggregationScenarios::NCMAggregationTimeLimitLoopTest(
				&m_UsbToIpaPipe, &m_IpaToUsbPipeAggTime, m_eIP);
	}

	/////////////////////////////////////////////////////////////////////////////////
};

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

class NCMAggregation0LimitsTest: public NCMAggregationTestFixture {
public:

	/////////////////////////////////////////////////////////////////////////////////

	NCMAggregation0LimitsTest(bool generic_agg)
		: NCMAggregationTestFixture(generic_agg)
	{
		if (generic_agg)
			m_name = "GenNCMAggregation0LimitsTest";
		else
			m_name = "NCMAggregation0LimitsTest";
		m_description = "NCM Aggregation 0 limits test - sends 5 packets and expects"
				"to get each packet back aggregated (both size and time limits are 0)";
	}

	/////////////////////////////////////////////////////////////////////////////////

	virtual bool AddRules(void)
	{
		return AddRules1HeaderAggregation0Limits();
	} // AddRules()

	/////////////////////////////////////////////////////////////////////////////////

	bool TestLogic(void)
	{
		return NCMAggregationScenarios::NCMAggregation0LimitsTest(
				&m_UsbToIpaPipe, &m_IpaToUsbPipeAgg0Limits, m_eIP);
	}

	/////////////////////////////////////////////////////////////////////////////////
};

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/* Legacy NCM tests */

static NCMAggregationTest ncmAggregationTest(false);
static NCMDeaggregationTest ncmDeaggregationTest(false);
static NCMDeaggregationOnePacketTest ncmDeaggregationOnePacketTest(false);
static NCMDeaggregationAndAggregationTest ncmDeaggregationAndAggregationTest(false);
static NCMMultipleDeaggregationAndAggregationTest
		ncmMultipleDeaggregationAndAggregationTest(false);
static NCMAggregationLoopTest ncmAggregationLoopTest(false);
static NCMDeaggregationMultipleNDPTest ncmDeaggregationMultipleNDPTest(false);
static NCMAggregationTimeLimitTest ncmAggregationTimeLimitTest(false);
static NCMAggregationByteLimitTest ncmAggregationByteLimitTest(false);
static NCMAggregationTimeLimitLoopTest ncmAggregationTimeLimitLoopTest(false);
static NCMAggregation0LimitsTest ncmAggregation0LimitsTest(false);

/* Generic Aggregation NCM tests */

static NCMAggregationTest genNcmAggregationTest(true);
static NCMDeaggregationTest genNcmDeaggregationTest(true);
static NCMDeaggregationOnePacketTest genNcmDeaggregationOnePacketTest(true);
static NCMDeaggregationAndAggregationTest genNcmDeaggregationAndAggregationTest(true);
static NCMMultipleDeaggregationAndAggregationTest genNcmMultipleDeaggregationAndAggregationTest(true);
static NCMAggregationLoopTest genNcmAggregationLoopTest(true);
static NCMDeaggregationMultipleNDPTest genNcmDeaggregationMultipleNDPTest(true);
static NCMAggregationTimeLimitTest genNcmAggregationTimeLimitTest(true);
static NCMAggregationByteLimitTest genNcmAggregationByteLimitTest(true);
static NCMAggregationByteLimitTestFC genNcmAggregationByteLimitTestFC(true);
static NCMAggregationDualDpTestFC genNCMAggregationDualDpTestFC(true);
static NCMAggregationDualDpTestFcRoutingBased genNCMAggregationDualDpTestFcRoutingBased(true);
static NCMAggregationTimeLimitLoopTest genNcmAggregationTimeLimitLoopTest(true);
static NCMAggregation0LimitsTest genNcmAggregation0LimitsTest(true);

/////////////////////////////////////////////////////////////////////////////////
//                                  EOF                                      ////
/////////////////////////////////////////////////////////////////////////////////
