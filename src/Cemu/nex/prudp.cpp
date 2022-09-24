#include "prudp.h"
#include "util/crypto/md5.h"

#include<bitset>
#include<random>

#include <boost/random/uniform_int.hpp>

void swap(unsigned char *a, unsigned char *b) 
{
	int tmp = *a;
	*a = *b;
	*b = tmp;
}

void KSA(unsigned char *key, int keyLen, unsigned char *S)
{
	int j = 0;

	for (int i = 0; i < RC4_N; i++)
		S[i] = i;

	for (int i = 0; i < RC4_N; i++) 
	{
		j = (j + S[i] + key[i % keyLen]) % RC4_N;

		swap(&S[i], &S[j]);
	}
}

void PRGA(unsigned char *S, unsigned char* input, int len, unsigned char* output) 
{
	int i = 0;
	int j = 0;

	for (size_t n = 0; n < len; n++) 
	{
		i = (i + 1) % RC4_N;
		j = (j + S[i]) % RC4_N;

		swap(&S[i], &S[j]);
		int rnd = S[(S[i] + S[j]) % RC4_N];

		output[n] = rnd ^ input[n];
	}
}

void RC4(char* key, unsigned char* input, int len, unsigned char* output) 
{
	unsigned char S[RC4_N];
	KSA((unsigned char*)key, (int)strlen(key), S);
	PRGA(S, input, len, output);
}

void RC4_initCtx(RC4Ctx_t* rc4Ctx, const char* key)
{
	rc4Ctx->i = 0;
	rc4Ctx->j = 0;
	KSA((unsigned char*)key, (int)strlen(key), rc4Ctx->S);
}

void RC4_initCtx(RC4Ctx_t* rc4Ctx, unsigned char* key, int keyLen)
{
	rc4Ctx->i = 0;
	rc4Ctx->j = 0;
	KSA(key, keyLen, rc4Ctx->S);
}

void RC4_transform(RC4Ctx_t* rc4Ctx, unsigned char* input, int len, unsigned char* output)
{
	int i = rc4Ctx->i;
	int j = rc4Ctx->j;

	for (size_t n = 0; n < len; n++)
	{
		i = (i + 1) % RC4_N;
		j = (j + rc4Ctx->S[i]) % RC4_N;

		swap(&rc4Ctx->S[i], &rc4Ctx->S[j]);
		int rnd = rc4Ctx->S[(rc4Ctx->S[i] + rc4Ctx->S[j]) % RC4_N];

		output[n] = rnd ^ input[n];
	}

	rc4Ctx->i = i;
	rc4Ctx->j = j;
}

uint32 prudpGetMSTimestamp()
{
	return GetTickCount();
}

std::bitset<10000> _portUsageMask;

uint16 getRandomSrcPRUDPPort()
{
	while (true)
	{
		sint32 p = rand() % 10000;
		if (_portUsageMask.test(p))
			continue;
		_portUsageMask.set(p);
		return 40000 + p;
	}
	return 0;
}

void releasePRUDPPort(uint16 port)
{
	uint32 bitIndex = port - 40000;
	_portUsageMask.reset(bitIndex);
}

std::mt19937_64 prudpRG(GetTickCount());
// workaround for static asserts when using uniform_int_distribution
boost::random::uniform_int_distribution<int> prudpDis8(0, 0xFF);

uint8 prudp_generateRandomU8()
{
	return prudpDis8(prudpRG);
}

uint32 prudp_generateRandomU32()
{
	uint32 v = prudp_generateRandomU8();
	v <<= 8;
	v |= prudp_generateRandomU8();
	v <<= 8;
	v |= prudp_generateRandomU8();
	v <<= 8;
	v |= prudp_generateRandomU8();
	return v;
}

uint8 prudp_calculateChecksum(uint8 checksumBase, uint8* data, sint32 length)
{
	uint32 checksum32 = 0;
	for (sint32 i = 0; i < length / 4; i++)
	{
		checksum32 += *(uint32*)(data + i * 4);
	}
	uint8 checksum = checksumBase;
	for (sint32 i = length&(~3); i < length; i++)
	{
		checksum += data[i];
	}
	checksum += (uint8)((checksum32 >> 0) & 0xFF);
	checksum += (uint8)((checksum32 >> 8) & 0xFF);
	checksum += (uint8)((checksum32 >> 16) & 0xFF);
	checksum += (uint8)((checksum32 >> 24) & 0xFF);
	return checksum;
}

// calculate the actual size of the packet from the unprocessed raw data
// returns 0 on error
sint32 prudpPacket::calculateSizeFromPacketData(uint8* data, sint32 length)
{
	// check if packet is long enough to hold basic header
	if (length < 0xB)
		return 0;
	// get flags fields
	uint16 typeAndFlags = *(uint16*)(data + 0x02);
	uint16 type = (typeAndFlags&0xF);
	uint16 flags = (typeAndFlags >> 4);
	if ((flags&FLAG_HAS_SIZE) == 0)
		return length; // without a size field, we cant calculate the length
	sint32 calculatedSize;
	if (type == TYPE_SYN)
	{
		if (length < (0xB + 0x4 + 2))
			return 0;
		uint16 payloadSize = *(uint16*)(data+0xB+0x4);
		calculatedSize = 0xB + 0x4 + 2 + (sint32)payloadSize + 1; // base header + connection signature (SYN param) + payloadSize field + checksum after payload
		if (calculatedSize > length)
			return 0;
		return calculatedSize;
	}
	else if (type == TYPE_CON)
	{
		if (length < (0xB + 0x4 + 2))
			return 0;
		uint16 payloadSize = *(uint16*)(data + 0xB + 0x4);
		calculatedSize = 0xB + 0x4 + 2 + (sint32)payloadSize + 1; // base header + connection signature (CON param) + payloadSize field + checksum after payload
		// note: For secure connections the extra data is part of the payload
		if (calculatedSize > length)
			return 0;
		return calculatedSize;
	}
	else if (type == TYPE_DATA)
	{
		if (length < (0xB + 1 + 2))
			return 0;
		uint16 payloadSize = *(uint16*)(data + 0xB + 1);
		calculatedSize = 0xB + 1 + 2 + (sint32)payloadSize + 1; // base header + fragmentIndex (DATA param) + payloadSize field + checksum after payload
		if (calculatedSize > length)
			return 0;
		return calculatedSize;
	}
	else if (type == TYPE_PING)
	{
		if (length < (0xB + 2))
			return 0;
		uint16 payloadSize = *(uint16*)(data + 0xB);
		calculatedSize = 0xB + 2 + (sint32)payloadSize + 1; // base header + payloadSize field + checksum after payload
		if (calculatedSize > length)
			return 0;
		return calculatedSize;
	}
	else
		assert_dbg(); // unknown packet type (todo - add disconnect and ping packet, then make this function return 0 for all unknown types)
	return length;
}

prudpPacket::prudpPacket(prudpStreamSettings_t* streamSettings, uint8 src, uint8 dst, uint8 type, uint16 flags, uint8 sessionId, uint16 sequenceId, uint32 packetSignature)
{
	this->src = src;
	this->dst = dst;
	this->type = type;
	this->flags = flags;
	this->sessionId = sessionId;
	this->sequenceId = sequenceId;
	this->specifiedPacketSignature = packetSignature;
	this->streamSettings = streamSettings;
	this->fragmentIndex = 0;
	this->isEncrypted = false;
}

bool prudpPacket::requiresAck()
{
	return (flags&FLAG_NEED_ACK) != 0;
}

sint32 prudpPacket::buildData(uint8* output, sint32 maxLength)
{
	// PRUDP V0
	// encrypt packet data
	if (this->type == prudpPacket::TYPE_DATA)
	{
		// dont save new rc4 state if the packet is not reliable or requires no ack?
		if (packetData.size() > 0)
		{
			if (isEncrypted == false) // only encrypt once
			{
				RC4_transform(&streamSettings->rc4Client, &packetData.front(), (int)packetData.size(), &packetData.front());
				isEncrypted = true;
			}
		}
	}
	// build packet
	uint8* packetBuffer = output;
	sint32 writeIndex = 0;
	// write constant header
	*(uint8*)(packetBuffer + 0x00) = src;
	*(uint8*)(packetBuffer + 0x01) = dst;
	uint16 typeAndFlags = (this->flags << 4) | (this->type);
	*(uint16*)(packetBuffer + 0x02) = typeAndFlags;
	*(uint8*)(packetBuffer + 0x04) = sessionId;
	*(uint32*)(packetBuffer + 0x05) = packetSignature();
	*(uint16*)(packetBuffer + 0x09) = sequenceId;
	writeIndex = 0xB;
	// variable fields
	if (this->type == TYPE_SYN)
	{
		*(uint32*)(packetBuffer + writeIndex) = 0; // connection signature (always 0 for SYN packet)
		writeIndex += 4;
	}
	else if (this->type == TYPE_CON)
	{
		// connection signature (+ kerberos data if secure connection)
		memcpy(packetBuffer + writeIndex, &packetData.front(), packetData.size());
		writeIndex += (int)packetData.size();
	}
	else if (this->type == TYPE_DATA)
	{
		*(uint8*)(packetBuffer + writeIndex) = fragmentIndex; // fragment index
		writeIndex += 1;
		if (packetData.empty() == false)
		{
			memcpy(packetBuffer + writeIndex, &packetData.front(), packetData.size());
			writeIndex += (int)packetData.size();
		}
	}
	else if (this->type == TYPE_PING)
	{
		// no data
	}
	else
		assert_dbg();
	// checksum
	*(uint8*)(packetBuffer + writeIndex) = calculateChecksum(packetBuffer, writeIndex);
	writeIndex++;

	return writeIndex;
}

uint32 prudpPacket::packetSignature()
{
	if (type == TYPE_SYN)
		return 0;
	else if (type == TYPE_CON || type == TYPE_PING)
		return specifiedPacketSignature;
	else if (type == TYPE_DATA)
	{
		if (packetData.size() == 0)
			return 0x12345678;

		HMACMD5Ctx ctx;
		hmacMD5_init_limK_to_64(streamSettings->accessKeyDigest, 16, &ctx);
		hmacMD5_update(&packetData.front(), (int)packetData.size(), &ctx);

		uint8 digest[16];
		hmacMD5_final(digest, &ctx);
		uint32 pSig = *(uint32*)digest;
		return pSig;
	}

	assert_dbg();
	return 0;
}

void prudpPacket::setData(uint8* data, sint32 length)
{
	packetData.resize(length);
	memcpy(&packetData.front(), data, length);
}

void prudpPacket::setFragmentIndex(uint8 fragmentIndex)
{
	this->fragmentIndex = fragmentIndex;
}

uint8 prudpPacket::calculateChecksum(uint8* data, sint32 length)
{
	return prudp_calculateChecksum(streamSettings->checksumBase, data, length);
}

prudpIncomingPacket::prudpIncomingPacket()
{
	src = 0;
	dst = 0;
	flags = 0;
	type = 0;
	sessionId = 0;
	packetSignature = 0;
	sequenceId = 0;
	fragmentIndex = 0;
	hasData = false;
	isInvalid = false;
	streamSettings = nullptr;
}

prudpIncomingPacket::prudpIncomingPacket(prudpStreamSettings_t* streamSettings, uint8* data, sint32 length) : prudpIncomingPacket()
{
	if (length < 0xB + 1)
	{
		isInvalid = true;
		return;
	}
	this->streamSettings = streamSettings;
	// verify checksum first
	uint8 actualChecksum = calculateChecksum(data, length - 1);
	uint8 packetChecksum = *(uint8*)(data + length - 1);
	if (actualChecksum != packetChecksum)
	{
		isInvalid = true;
		return;
	}
	length--; // remove checksum from length
	// verify constant header
	this->src = *(uint8*)(data + 0x00);
	this->dst = *(uint8*)(data + 0x01);
	uint16 typeAndFlags = *(uint16*)(data + 0x02);
	this->flags = (typeAndFlags >> 4) & 0xFFF;
	this->type = (typeAndFlags & 0xF);
	this->sessionId = *(uint8*)(data + 0x4);
	this->packetSignature = *(uint32*)(data + 0x5);
	this->sequenceId = *(uint16*)(data + 0x9);
	// read dynamic fields
	sint32 readIndex = 0xB;
	if (this->type == prudpPacket::TYPE_SYN)
	{
		// SYN packet
		if (readIndex < 4)
		{
			isInvalid = true;
			return;
		}
		packetData.resize(4);
		*(uint32*)(&packetData.front()) = *(uint32*)(data + readIndex);
		hasData = true;
		readIndex += 4;
		// ignore FLAG_HAS_SIZE (would come here, usually with a value of 0)
		return;
	}
	else if (this->type == prudpPacket::TYPE_CON)
	{
		// CON packet
		if (readIndex < 4)
		{
			isInvalid = true;
			return;
		}
		// this packet has another 4 byte signature but it's always zero? (value is ignored for now)
		// ignore FLAG_HAS_SIZE
	}
	else if (this->type == prudpPacket::TYPE_PING)
	{
		// PING packet
		// ignore payload
	}
	else if (this->type == prudpPacket::TYPE_DATA)
	{
		// can we assume that reliable data packets always need to have a unique sequence id? (Even if it's a multi-fragment frame)
		// unreliable packets must have a sequence id too or how else would we know when to decrypt it?

		bool hasPayloadSize = (this->flags & prudpPacket::FLAG_HAS_SIZE) != 0;
		// verify length
		if ((length-readIndex) < 1+(hasPayloadSize?2:0))
		{
			// too short
			isInvalid = true;
			return;
		}
		// read fragment index
		this->fragmentIndex = *(uint8*)(data + readIndex);
		readIndex += sizeof(uint8);
		// read payload size (optional)
		if (hasPayloadSize)
		{
			uint16 payloadSize = *(uint32*)(data + readIndex);
			readIndex += sizeof(uint16);
			// verify payload size
			if ((length - readIndex) != payloadSize)
			{
				assert_dbg(); // mismatch, ignore packet or use specified payload size?
			}
		}
		// read payload
		if (readIndex < length)
		{
			sint32 dataSize = length - readIndex;
			packetData.resize(dataSize);
			memcpy(&packetData.front(), data + readIndex, dataSize);
		}
		return;
	}
	else if (this->type == prudpPacket::TYPE_DISCONNECT)
	{
		// DISCONNECT packet
		// ignore payload
	}
	else
	{
#ifdef CEMU_DEBUG_ASSERT
		assert_dbg();
#endif
	}
}

bool prudpIncomingPacket::hasError()
{
	return isInvalid;
}

uint8 prudpIncomingPacket::calculateChecksum(uint8* data, sint32 length)
{
	return prudp_calculateChecksum(streamSettings->checksumBase, data, length);
}

void prudpIncomingPacket::decrypt()
{
	if (packetData.empty())
		return;
	RC4_transform(&streamSettings->rc4Server, &packetData.front(), (int)packetData.size(), &packetData.front());
}

#define PRUDP_VPORT(__streamType, __port) (((__streamType)<<4) | (__port))

prudpClient::prudpClient()
{
	currentConnectionState = STATE_CONNECTING;
	serverConnectionSignature = 0;
	clientConnectionSignature = 0;
	hasSentCon = false;
	outgoingSequenceId = 0;
	incomingSequenceId = 0;

	clientSessionId = 0;
	serverSessionId = 0;

	isSecureConnection = false;
}

prudpClient::~prudpClient()
{
	if (srcPort != 0)
	{
		releasePRUDPPort(srcPort);
		closesocket(socketUdp);
	}
}

prudpClient::prudpClient(uint32 dstIp, uint16 dstPort, const char* key) : prudpClient()
{
	this->dstIp = dstIp;
	this->dstPort = dstPort;
	// get unused random source port
	for (sint32 tries = 0; tries < 5; tries++)
	{
		srcPort = getRandomSrcPRUDPPort();
		// create and bind udp socket
		socketUdp = socket(AF_INET, SOCK_DGRAM, 0);
		struct sockaddr_in udpServer;
		udpServer.sin_family = AF_INET;
		udpServer.sin_addr.s_addr = INADDR_ANY;
		udpServer.sin_port = htons(srcPort);
		if (bind(socketUdp, (struct sockaddr *)&udpServer, sizeof(udpServer)) == SOCKET_ERROR)
		{
			if (tries == 4)
			{
				forceLog_printf("PRUDP: Failed to bind UDP socket");
				currentConnectionState = STATE_DISCONNECTED;
				srcPort = 0;
				return;
			}
			releasePRUDPPort(srcPort);
			closesocket(socketUdp);
			continue;
		}
		else
			break;
	}
	// set socket to non-blocking mode
#if BOOST_OS_WINDOWS
	u_long nonBlockingMode = 1;  // 1 to enable non-blocking socket
	ioctlsocket(socketUdp, FIONBIO, &nonBlockingMode);
#else
	int flags = fcntl(socketUdp, F_GETFL);
	fcntl(socketUdp, F_SETFL, flags | O_NONBLOCK);
#endif
	// generate frequently used parameters
	this->vport_src = PRUDP_VPORT(prudpPacket::STREAM_TYPE_SECURE, 0xF);
	this->vport_dst = PRUDP_VPORT(prudpPacket::STREAM_TYPE_SECURE, 0x1);
	// set stream settings
	uint8 checksumBase = 0;
	for (sint32 i = 0; key[i] != '\0'; i++)
	{
		checksumBase += key[i];
	}
	streamSettings.checksumBase = checksumBase;
	MD5_CTX md5Ctx;
	MD5_Init(&md5Ctx);
	MD5_Update(&md5Ctx, key, (int)strlen(key));
	MD5_Final(streamSettings.accessKeyDigest, &md5Ctx);
	// init stream ciphers
	RC4_initCtx(&streamSettings.rc4Server, "CD&ML");
	RC4_initCtx(&streamSettings.rc4Client, "CD&ML");
	// send syn packet
	prudpPacket* synPacket = new prudpPacket(&streamSettings, vport_src, vport_dst, prudpPacket::TYPE_SYN, prudpPacket::FLAG_NEED_ACK, 0, 0, 0);
	queuePacket(synPacket, dstIp, dstPort);
	outgoingSequenceId++;
	// set incoming sequence id to 1
	incomingSequenceId = 1;
}

prudpClient::prudpClient(uint32 dstIp, uint16 dstPort, const char* key, authServerInfo_t* authInfo) : prudpClient(dstIp, dstPort, key)
{
	RC4_initCtx(&streamSettings.rc4Server, authInfo->secureKey, 16);
	RC4_initCtx(&streamSettings.rc4Client, authInfo->secureKey, 16);
	this->isSecureConnection = true;
	memcpy(&this->authInfo, authInfo, sizeof(authServerInfo_t));
}

bool prudpClient::isConnected()
{
	return currentConnectionState == STATE_CONNECTED;
}

uint8 prudpClient::getConnectionState()
{
	return currentConnectionState;
}

void prudpClient::acknowledgePacket(uint16 sequenceId)
{
	auto it = std::begin(list_packetsWithAckReq);
	while (it != std::end(list_packetsWithAckReq))
	{
		if (it->packet->sequenceId == sequenceId)
		{
			delete it->packet;
			list_packetsWithAckReq.erase(it);
			return;
		}
		it++;
	}
}

void prudpClient::sortIncomingDataPacket(prudpIncomingPacket* incomingPacket)
{
	uint16 sequenceIdIncomingPacket = incomingPacket->sequenceId;
	// find insert index
	sint32 insertIndex = 0;
	while (insertIndex < queue_incomingPackets.size() )
	{
		uint16 seqDif = sequenceIdIncomingPacket - queue_incomingPackets[insertIndex]->sequenceId;
		if (seqDif&0x8000)
			break; // negative seqDif -> insert before current element
#ifdef CEMU_DEBUG_ASSERT
		if (seqDif == 0)
			assert_dbg(); // same sequence id, sort by fragment index?
#endif
		insertIndex++;
	}
	// insert
	sint32 currentSize = (sint32)queue_incomingPackets.size();
	queue_incomingPackets.resize(currentSize+1);
	for(sint32 i=currentSize; i>insertIndex; i--)
	{
		queue_incomingPackets[i] = queue_incomingPackets[i - 1];
	}
	queue_incomingPackets[insertIndex] = incomingPacket;
}

sint32 prudpClient::kerberosEncryptData(uint8* input, sint32 length, uint8* output)
{
	RC4Ctx_t rc4Kerberos;
	RC4_initCtx(&rc4Kerberos, this->authInfo.secureKey, 16);
	memcpy(output, input, length);
	RC4_transform(&rc4Kerberos, output, length, output);
	// calculate and append hmac
	hmacMD5(this->authInfo.secureKey, 16, output, length, output+length);
	return length + 16;
}

void prudpClient::handleIncomingPacket(prudpIncomingPacket* incomingPacket)
{
	if (incomingPacket->flags&prudpPacket::FLAG_ACK)
	{
		// ack packet
		acknowledgePacket(incomingPacket->sequenceId);
		if ((incomingPacket->type == prudpPacket::TYPE_DATA || incomingPacket->type == prudpPacket::TYPE_PING) && incomingPacket->packetData.empty())
		{
			// ack packet
			delete incomingPacket;
			return;
		}
	}
	// special cases
	if (incomingPacket->type == prudpPacket::TYPE_SYN)
	{
		if (hasSentCon == false && incomingPacket->hasData && incomingPacket->packetData.size() == 4)
		{
			this->serverConnectionSignature = *(uint32*)&incomingPacket->packetData.front();
			this->clientSessionId = prudp_generateRandomU8();
			// generate client session id
			this->clientConnectionSignature = prudp_generateRandomU32();
			// send con packet
			prudpPacket* conPacket = new prudpPacket(&streamSettings, vport_src, vport_dst, prudpPacket::TYPE_CON, prudpPacket::FLAG_NEED_ACK|prudpPacket::FLAG_RELIABLE, this->clientSessionId, outgoingSequenceId, serverConnectionSignature);
			outgoingSequenceId++;

			if (this->isSecureConnection)
			{
				// set packet specific data (client connection signature)
				uint8 tempBuffer[512];
				nexPacketBuffer conData(tempBuffer, sizeof(tempBuffer), true);
				conData.writeU32(this->clientConnectionSignature);
				conData.writeBuffer(authInfo.secureTicket, authInfo.secureTicketLength);
				// encrypted request data
				uint8 requestData[4 * 3];
				uint8 requestDataEncrypted[4 * 3 + 0x10];
				*(uint32*)(requestData + 0x0) = authInfo.userPid;
				*(uint32*)(requestData + 0x4) = authInfo.server.cid;
				*(uint32*)(requestData + 0x8) = prudp_generateRandomU32(); // todo - check value
				sint32 encryptedSize = kerberosEncryptData(requestData, sizeof(requestData), requestDataEncrypted);
				conData.writeBuffer(requestDataEncrypted, encryptedSize);
				conPacket->setData(conData.getDataPtr(), conData.getWriteIndex());
			}
			else
			{
				// set packet specific data (client connection signature)
				conPacket->setData((uint8*)&this->clientConnectionSignature, sizeof(uint32));
			}
			// sent packet
			queuePacket(conPacket, dstIp, dstPort);
			// remember con packet as sent
			hasSentCon = true;
		}
		delete incomingPacket;
		return;
	}
	else if (incomingPacket->type == prudpPacket::TYPE_CON)
	{
		// connected!
		if (currentConnectionState == STATE_CONNECTING)
		{
			lastPingTimestamp = prudpGetMSTimestamp();
			serverSessionId = incomingPacket->sessionId;
			currentConnectionState = STATE_CONNECTED;
			//printf("Connection established. ClientSession %02x ServerSession %02x\n", clientSessionId, serverSessionId);
		}
		delete incomingPacket;
		return;
	}
	else if (incomingPacket->type == prudpPacket::TYPE_DATA)
	{
		// verify some values
		uint16 seqDist = incomingPacket->sequenceId - incomingSequenceId;
		if (seqDist >= 0xC000)
		{
			// outdated
			delete incomingPacket;
			return;
		}
		// check if packet is already queued
		for (auto& it : queue_incomingPackets)
		{
			if (it->sequenceId == incomingPacket->sequenceId)
			{
				// already queued (should check other values too, like packet type?)
				forceLogDebug_printf("Duplicate PRUDP packet received");
				delete incomingPacket;
				return;
			}
		}
		// put into ordered receive queue
		sortIncomingDataPacket(incomingPacket);
	}
	else if (incomingPacket->type == prudpPacket::TYPE_DISCONNECT)
	{
		currentConnectionState = STATE_DISCONNECTED;
		return;
	}
	else
	{
		// ignore unknown packet
		delete incomingPacket;
		return;
	}

	if (incomingPacket->flags&prudpPacket::FLAG_NEED_ACK && incomingPacket->type == prudpPacket::TYPE_DATA)
	{
		// send ack back
		prudpPacket* ackPacket = new prudpPacket(&streamSettings, vport_src, vport_dst, prudpPacket::TYPE_DATA, prudpPacket::FLAG_ACK, this->clientSessionId, incomingPacket->sequenceId, 0);
		queuePacket(ackPacket, dstIp, dstPort);
	}
}

bool prudpClient::update()
{
	if (currentConnectionState == STATE_DISCONNECTED)
	{
		return false;
	}
	uint32 currentTimestamp = prudpGetMSTimestamp();
	// check for incoming packets
	uint8 receiveBuffer[4096];
	while (true)
	{
		sockaddr receiveFrom = { 0 };
		socklen_t receiveFromLen = sizeof(receiveFrom);
		sint32 r = recvfrom(socketUdp, (char*)receiveBuffer, sizeof(receiveBuffer), 0, &receiveFrom, &receiveFromLen);
		if (r >= 0)
		{
			//printf("RECV 0x%04x byte\n", r);
			// todo: Verify sender (receiveFrom)
			// calculate packet size
			sint32 pIdx = 0;
			while (pIdx < r)
			{
				sint32 packetLength = prudpPacket::calculateSizeFromPacketData(receiveBuffer + pIdx, r - pIdx);
				if (packetLength <= 0 || (pIdx + packetLength) > r)
				{
					//printf("Invalid packet length\n");
					break;
				}
				prudpIncomingPacket* incomingPacket = new prudpIncomingPacket(&streamSettings, receiveBuffer + pIdx, packetLength);
				pIdx += packetLength;
				if (incomingPacket->hasError())
				{
					delete incomingPacket;
					break;
				}
				if (incomingPacket->type != prudpPacket::TYPE_CON && incomingPacket->sessionId != serverSessionId)
				{
					delete incomingPacket;
					continue; // different session
				}
				handleIncomingPacket(incomingPacket);
			}
		}
		else
			break;
	}
	// check for ack timeouts
	for (auto &it : list_packetsWithAckReq)
	{
		if ((currentTimestamp - it.lastRetryTimestamp) >= 2300)
		{
			if (it.retryCount >= 7)
			{
				// after too many retries consider the connection dead
				currentConnectionState = STATE_DISCONNECTED;
			}
			// resend
			directSendPacket(it.packet, dstIp, dstPort);
			it.lastRetryTimestamp = currentTimestamp;
			it.retryCount++;
		}
	}
	// check if we need to send another ping
	if (currentConnectionState == STATE_CONNECTED && (currentTimestamp - lastPingTimestamp) >= 20000)
	{
		// send ping
		prudpPacket* pingPacket = new prudpPacket(&streamSettings, vport_src, vport_dst, prudpPacket::TYPE_PING, prudpPacket::FLAG_NEED_ACK | prudpPacket::FLAG_RELIABLE, this->clientSessionId, this->outgoingSequenceId, serverConnectionSignature);
		this->outgoingSequenceId++; // increase since prudpPacket::FLAG_RELIABLE is set (note: official Wii U friends client sends ping packets without FLAG_RELIABLE)
		queuePacket(pingPacket, dstIp, dstPort);
		lastPingTimestamp = currentTimestamp;
	}
	return false;
}

void prudpClient::directSendPacket(prudpPacket* packet, uint32 dstIp, uint16 dstPort)
{
	uint8 packetBuffer[prudpPacket::PACKET_RAW_SIZE_MAX];

	sint32 len = packet->buildData(packetBuffer, prudpPacket::PACKET_RAW_SIZE_MAX);

	sockaddr_in destAddr;
	destAddr.sin_family = AF_INET;
	destAddr.sin_port = htons(dstPort);
	destAddr.sin_addr.s_addr = dstIp;
	sendto(socketUdp, (const char*)packetBuffer, len, 0, (const sockaddr*)&destAddr, sizeof(destAddr));
}

void prudpClient::queuePacket(prudpPacket* packet, uint32 dstIp, uint16 dstPort)
{
	if (packet->requiresAck())
	{
		// remember this packet until we receive the ack
		prudpAckRequired_t ackRequired = { 0 };
		ackRequired.packet = packet;
		ackRequired.initialSendTimestamp = prudpGetMSTimestamp();
		ackRequired.lastRetryTimestamp = ackRequired.initialSendTimestamp;
		list_packetsWithAckReq.push_back(ackRequired);
		directSendPacket(packet, dstIp, dstPort);
	}
	else
	{
		directSendPacket(packet, dstIp, dstPort);
		delete packet;
	}
}

void prudpClient::sendDatagram(uint8* input, sint32 length, bool reliable)
{
	if (reliable == false)
	{
		assert_dbg(); // todo
	}
	if (length >= 0x300)
		assert_dbg(); // too long, need to split into multiple fragments

	// single fragment data packet
	prudpPacket* packet = new prudpPacket(&streamSettings, vport_src, vport_dst, prudpPacket::TYPE_DATA, prudpPacket::FLAG_NEED_ACK | prudpPacket::FLAG_RELIABLE, clientSessionId, outgoingSequenceId, 0);
	outgoingSequenceId++;
	packet->setFragmentIndex(0);
	packet->setData(input, length);
	queuePacket(packet, dstIp, dstPort);
}

uint16 prudpClient::getSourcePort()
{
	return this->srcPort;
}

SOCKET prudpClient::getSocket()
{
	if (currentConnectionState == STATE_DISCONNECTED)
	{
		return INVALID_SOCKET;
	}
	return this->socketUdp;
}

sint32 prudpClient::receiveDatagram(std::vector<uint8>& outputBuffer)
{
	if (queue_incomingPackets.empty())
		return -1;
	prudpIncomingPacket* incomingPacket = queue_incomingPackets[0];
	if (incomingPacket->sequenceId != this->incomingSequenceId)
		return -1;

	if (incomingPacket->fragmentIndex == 0)
	{
		// single-fragment packet
		// decrypt
		incomingPacket->decrypt();
		// read data
		sint32 datagramLen = (sint32)incomingPacket->packetData.size();
		if (datagramLen > 0)
		{
			// resize buffer if necessary
			if (datagramLen > outputBuffer.size())
				outputBuffer.resize(datagramLen);
			// to conserve memory we will also shrink the buffer if it was previously extended beyond 64KB
			constexpr size_t BUFFER_TARGET_SIZE = 1024 * 64;
			if (datagramLen < BUFFER_TARGET_SIZE && outputBuffer.size() > BUFFER_TARGET_SIZE)
				outputBuffer.resize(BUFFER_TARGET_SIZE);
			// copy datagram to buffer
			memcpy(outputBuffer.data(), &incomingPacket->packetData.front(), datagramLen);
		}
		delete incomingPacket;
		// remove packet from queue
		sint32 size = (sint32)queue_incomingPackets.size();
		size--;
		for (sint32 i = 0; i < size; i++)
		{
			queue_incomingPackets[i] = queue_incomingPackets[i + 1];
		}
		queue_incomingPackets.resize(size);
		// advance expected sequence id
		this->incomingSequenceId++;
		return datagramLen;
	}
	else
	{
		// multi-fragment packet
		if (incomingPacket->fragmentIndex != 1)
			return -1; // first packet of the chain not received yet
		// verify chain
		sint32 packetIndex = 1;
		sint32 chainLength = -1; // if full chain found, set to count of packets
		for(sint32 i=1; i<queue_incomingPackets.size(); i++)
		{
			uint8 itFragmentIndex = queue_incomingPackets[packetIndex]->fragmentIndex;
			// sequence id must increase by 1 for every packet
			if (queue_incomingPackets[packetIndex]->sequenceId != (this->incomingSequenceId+i) )
				return -1; // missing packets
			// last fragment in chain is marked by fragment index 0
			if (itFragmentIndex == 0)
			{
				chainLength = i+1;
				break;
			}
			packetIndex++;
		}
		if (chainLength < 1)
			return -1; // chain not complete
		// extract data from packet chain
		sint32 writeIndex = 0;
		for (sint32 i = 0; i < chainLength; i++)
		{
			incomingPacket = queue_incomingPackets[i];
			// decrypt
			incomingPacket->decrypt();
			// extract data
			sint32 datagramLen = (sint32)incomingPacket->packetData.size();
			if (datagramLen > 0)
			{
				// make sure output buffer can fit the data
				if ((writeIndex + datagramLen) > outputBuffer.size())
					outputBuffer.resize(writeIndex + datagramLen + 4 * 1024);
				memcpy(outputBuffer.data()+writeIndex, &incomingPacket->packetData.front(), datagramLen);
				writeIndex += datagramLen;
			}
			// free packet memory
			delete incomingPacket;
		}
		// remove packets from queue
		sint32 size = (sint32)queue_incomingPackets.size();
		size -= chainLength;
		for (sint32 i = 0; i < size; i++)
		{
			queue_incomingPackets[i] = queue_incomingPackets[i + chainLength];
		}
		queue_incomingPackets.resize(size);
		this->incomingSequenceId += chainLength;
		return writeIndex;
	}
	return -1;
}
