#pragma once
#include "nexTypes.h"
#include "Common/socket.h"

#define RC4_N 256

typedef struct
{
	unsigned char S[RC4_N];
	int i;
	int j;
}RC4Ctx_t;

void RC4_initCtx(RC4Ctx_t* rc4Ctx, char *key);
void RC4_initCtx(RC4Ctx_t* rc4Ctx, unsigned char* key, int keyLen);
void RC4_transform(RC4Ctx_t* rc4Ctx, unsigned char* input, int len, unsigned char* output);

typedef struct
{
	uint8 checksumBase; // calculated from key
	uint8 accessKeyDigest[16]; // MD5 hash of key
	RC4Ctx_t rc4Client;
	RC4Ctx_t rc4Server;
}prudpStreamSettings_t;

typedef struct
{
	uint32 ip;
	uint16 port;
	sint32 cid;
	sint32 pid;
	sint32 sid;
	sint32 stream;
	sint32 type;
}stationUrl_t;

typedef struct
{
	uint32 userPid;
	uint8 secureKey[16];
	uint8 kerberosKey[16];
	uint8 secureTicket[1024];
	sint32 secureTicketLength;
	stationUrl_t server;
}authServerInfo_t;

uint8 prudp_calculateChecksum(uint8 checksumBase, uint8* data, sint32 length);

class prudpPacket
{
public:
	static const int PACKET_RAW_SIZE_MAX = 500;

	static const int TYPE_SYN = 0;
	static const int TYPE_CON = 1;
	static const int TYPE_DATA = 2;
	static const int TYPE_DISCONNECT = 3;
	static const int TYPE_PING = 4;

	static const int STREAM_TYPE_SECURE = 0xA;

	static const int FLAG_ACK = 0x1;
	static const int FLAG_RELIABLE = 0x2; // if this flag is set, increase sequenceId
	static const int FLAG_NEED_ACK = 0x4;
	static const int FLAG_HAS_SIZE = 0x8;

	static sint32 calculateSizeFromPacketData(uint8* data, sint32 length);

	prudpPacket(prudpStreamSettings_t* streamSettings, uint8 src, uint8 dst, uint8 type, uint16 flags, uint8 sessionId, uint16 sequenceId, uint32 packetSignature);
	bool requiresAck();
	void setData(uint8* data, sint32 length);
	void setFragmentIndex(uint8 fragmentIndex);
	sint32 buildData(uint8* output, sint32 maxLength);

private:
	uint32 packetSignature();

	uint8 calculateChecksum(uint8* data, sint32 length);

public:
	uint16 sequenceId;

private:
	uint8 src;
	uint8 dst;
	uint8 type;
	uint8 fragmentIndex;
	uint16 flags;
	uint8 sessionId;
	uint32 specifiedPacketSignature;
	prudpStreamSettings_t* streamSettings;
	std::vector<uint8> packetData;
	bool isEncrypted;
};

class prudpIncomingPacket
{
public:
	prudpIncomingPacket(prudpStreamSettings_t* streamSettings, uint8* data, sint32 length);

	bool hasError();

	uint8 calculateChecksum(uint8* data, sint32 length);
	void decrypt();

private:
	prudpIncomingPacket();

public:
	// prudp header
	uint8 src;
	uint8 dst;
	uint16 flags;
	uint8 type;
	uint8 sessionId;
	uint32 packetSignature;
	uint16 sequenceId;
	uint8 fragmentIndex;
	bool hasData = false;
	std::vector<uint8> packetData;

private:
	bool isInvalid = false;
	prudpStreamSettings_t* streamSettings = nullptr;

};

typedef struct
{
	prudpPacket* packet;
	uint32 initialSendTimestamp;
	uint32 lastRetryTimestamp;
	sint32 retryCount;
}prudpAckRequired_t;

class prudpClient
{
public:
	static const int STATE_CONNECTING = 0;
	static const int STATE_CONNECTED = 1;
	static const int STATE_DISCONNECTED = 2;

public:
	prudpClient(uint32 dstIp, uint16 dstPort, const char* key);
	prudpClient(uint32 dstIp, uint16 dstPort, const char* key, authServerInfo_t* authInfo);
	~prudpClient();

	bool isConnected();

	uint8 getConnectionState();
	void acknowledgePacket(uint16 sequenceId);
	void sortIncomingDataPacket(prudpIncomingPacket* incomingPacket);
	void handleIncomingPacket(prudpIncomingPacket* incomingPacket);
	bool update(); // check for new incoming packets, returns true if receiveDatagram() should be called

	sint32 receiveDatagram(std::vector<uint8>& outputBuffer);
	void sendDatagram(uint8* input, sint32 length, bool reliable = true);

	uint16 getSourcePort();

	SOCKET getSocket();

private:
	prudpClient();
	void directSendPacket(prudpPacket* packet, uint32 dstIp, uint16 dstPort);
	sint32 kerberosEncryptData(uint8* input, sint32 length, uint8* output);
	void queuePacket(prudpPacket* packet, uint32 dstIp, uint16 dstPort);

private:
	uint16 srcPort;
	uint32 dstIp;
	uint16 dstPort;
	uint8 vport_src;
	uint8 vport_dst;
	prudpStreamSettings_t streamSettings;
	std::vector<prudpAckRequired_t> list_packetsWithAckReq;
	std::vector<prudpIncomingPacket*> queue_incomingPackets;
	
	// connection
	uint8 currentConnectionState;
	uint32 serverConnectionSignature;
	uint32 clientConnectionSignature;
	bool hasSentCon;
	uint32 lastPingTimestamp;

	uint16 outgoingSequenceId;
	uint16 incomingSequenceId;

	uint8 clientSessionId;
	uint8 serverSessionId;

	// secure
	bool isSecureConnection;
	authServerInfo_t authInfo;

	// socket
	SOCKET socketUdp;
};

uint32 prudpGetMSTimestamp();