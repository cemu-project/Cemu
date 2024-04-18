#pragma once
#include "nexTypes.h"
#include "Common/socket.h"

#define RC4_N 256

struct RC4Ctx
{
	unsigned char S[RC4_N];
	int i;
	int j;
};

void RC4_initCtx(RC4Ctx* rc4Ctx, const char* key);
void RC4_initCtx(RC4Ctx* rc4Ctx, unsigned char* key, int keyLen);
void RC4_transform(RC4Ctx* rc4Ctx, unsigned char* input, int len, unsigned char* output);

struct prudpStreamSettings
{
	uint8 checksumBase; // calculated from key
	uint8 accessKeyDigest[16]; // MD5 hash of key
	RC4Ctx rc4Client;
	RC4Ctx rc4Server;
};

struct prudpStationUrl
{
	uint32 ip;
	uint16 port;
	sint32 cid;
	sint32 pid;
	sint32 sid;
	sint32 stream;
	sint32 type;
};

struct prudpAuthServerInfo
{
	uint32 userPid;
	uint8 secureKey[16];
	uint8 kerberosKey[16];
	uint8 secureTicket[1024];
	sint32 secureTicketLength;
	prudpStationUrl server;
};

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

	prudpPacket(prudpStreamSettings* streamSettings, uint8 src, uint8 dst, uint8 type, uint16 flags, uint8 sessionId, uint16 sequenceId, uint32 packetSignature);
	bool requiresAck();
	void setData(uint8* data, sint32 length);
	void setFragmentIndex(uint8 fragmentIndex);
	sint32 buildData(uint8* output, sint32 maxLength);
	uint8 GetType() const { return type; }
	uint16 GetSequenceId() const { return m_sequenceId; }

private:
	uint32 packetSignature();

	uint8 calculateChecksum(uint8* data, sint32 length);

private:
	uint8 src;
	uint8 dst;
	uint8 type;
	uint8 fragmentIndex;
	uint16 flags;
	uint8 sessionId;
	uint32 specifiedPacketSignature;
	prudpStreamSettings* streamSettings;
	std::vector<uint8> packetData;
	bool isEncrypted;
	uint16 m_sequenceId{0};

};

class prudpIncomingPacket
{
public:
	prudpIncomingPacket(prudpStreamSettings* streamSettings, uint8* data, sint32 length);

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
	prudpStreamSettings* streamSettings = nullptr;
};

class prudpClient
{
	struct PacketWithAckRequired
	{
		PacketWithAckRequired(prudpPacket* packet, uint32 initialSendTimestamp) :
			packet(packet), initialSendTimestamp(initialSendTimestamp), lastRetryTimestamp(initialSendTimestamp) { }
		prudpPacket* packet;
		uint32 initialSendTimestamp;
		uint32 lastRetryTimestamp;
		sint32 retryCount{0};
	};
public:
	enum class ConnectionState : uint8
	{
	  Connecting,
	  Connected,
	  Disconnected
  };

	prudpClient(uint32 dstIp, uint16 dstPort, const char* key);
	prudpClient(uint32 dstIp, uint16 dstPort, const char* key, prudpAuthServerInfo* authInfo);
	~prudpClient();

	bool IsConnected() const { return m_currentConnectionState == ConnectionState::Connected; }
	ConnectionState GetConnectionState() const { return m_currentConnectionState; }
	uint16 GetSourcePort() const { return m_srcPort; }

	bool Update(); // update connection state and check for incoming packets. Returns true if ReceiveDatagram() should be called

	sint32 ReceiveDatagram(std::vector<uint8>& outputBuffer);
	void SendDatagram(uint8* input, sint32 length, bool reliable = true);

private:
	prudpClient();

	void HandleIncomingPacket(std::unique_ptr<prudpIncomingPacket> incomingPacket);
	void DirectSendPacket(prudpPacket* packet);
	sint32 KerberosEncryptData(uint8* input, sint32 length, uint8* output);
	void QueuePacket(prudpPacket* packet);

	void AcknowledgePacket(uint16 sequenceId);
	void SortIncomingDataPacket(std::unique_ptr<prudpIncomingPacket> incomingPacket);

	void SendCurrentHandshakePacket();

private:
	uint16 m_srcPort;
	uint32 m_dstIp;
	uint16 m_dstPort;
	uint8 m_srcVPort;
	uint8 m_dstVPort;
	prudpStreamSettings m_streamSettings;
	std::vector<PacketWithAckRequired> m_dataPacketsWithAckReq;
	std::vector<std::unique_ptr<prudpIncomingPacket>> m_incomingPacketQueue;

	// connection handshake state
	bool m_hasSynAck{false};
	bool m_hasConAck{false};
	uint32 m_lastHandshakeTimestamp{0};
	uint8 m_handshakeRetryCount{0};

	// connection
	ConnectionState m_currentConnectionState;
	uint32 m_serverConnectionSignature;
	uint32 m_clientConnectionSignature;
	uint32 m_lastPingTimestamp;

	uint16 m_outgoingReliableSequenceId{2}; // 1 is reserved for CON
	uint16 m_incomingSequenceId;

	uint16 m_outgoingSequenceId_ping{0};
	uint8 m_unacknowledgedPingCount{0};

	uint8 m_clientSessionId;
	uint8 m_serverSessionId;

	// secure
	bool m_isSecureConnection{false};
	prudpAuthServerInfo m_authInfo;

	// socket
	SOCKET m_socketUdp;
};

uint32 prudpGetMSTimestamp();