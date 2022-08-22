#pragma once
#include "nexTypes.h"
#include<mutex>

const int NEX_PROTOCOL_AUTHENTICATION = 0xA;
const int NEX_PROTOCOL_SECURE = 0xB;
const int NEX_PROTOCOL_FRIENDS_WIIU = 0x66;

class nexService;

typedef struct
{
	nexService* nex;
	bool isSuccessful;
	uint32 errorCode;
	uint32 callId;
	uint32 methodId;
	uint8  protocolId;
	nexPacketBuffer data;
	void* custom;
}nexServiceResponse_t;

typedef struct
{
	nexService* nex;
	uint32 callId;
	uint32 methodId;
	uint8  protocolId;
	nexPacketBuffer data;
	void* custom;
}nexServiceRequest_t;

class prudpClient;

class nexService
{
private:
	typedef struct
	{
		uint8 protocolId;
		uint32 methodId;
		uint32 callId;
		void(*nexServiceResponse)(nexService* nex, nexServiceResponse_t* serviceResponse);
		void* custom;
		bool handleError; // if set to true, call nexServiceResponse with errorCode set (else the callback is skipped when the call is not successful)
		uint32 requestTime; // timestamp of when the request was sent
		// alternative callback handler
		std::function<void(nexServiceResponse_t*)> cb2;
	}nexActiveRequestInfo_t;

	typedef struct  
	{
		uint8 protocolId;
		uint32 methodId;
		bool callHandlerIfError;
		//nexPacketBuffer* parameter;
		std::vector<uint8> parameterData;
		void(*nexServiceResponse)(nexService* nex, nexServiceResponse_t* serviceResponse);
		void* custom;
		// alternative callback handler
		std::function<void(nexServiceResponse_t*)> cb2;
	}queuedRequest_t;

	typedef struct
	{
		uint8 protocol;
		void(*processRequest)(nexServiceRequest_t* request);
		void* custom;
	}protocolHandler_t;

public:
	static const int STATE_CONNECTING = 0;
	static const int STATE_CONNECTED = 1;
	static const int STATE_DISCONNECTED = 2;

	static const unsigned int ERR_TIMEOUT = (0xFFFFFFFF);
	static const unsigned int ERR_NO_CONNECTION = (0xFFFFFFFE);

	nexService(prudpClient* con);
	nexService(uint32 ip, uint16 port, const char* accessKey);

	const uint8 PROTOCOL_BIT_REQUEST = 0x80;

	void callMethod(uint8 protocolId, uint32 methodId, nexPacketBuffer* parameter, void(*nexServiceResponse)(nexService* nex, nexServiceResponse_t* serviceResponse), void* custom, bool callHandlerIfError = false);
	void callMethod(uint8 protocolId, uint32 methodId, nexPacketBuffer* parameter, std::function<void(nexServiceResponse_t*)> cb, bool callHandlerIfError);
	void registerProtocolRequestHandler(uint8 protocol, void(*processRequest)(nexServiceRequest_t* request), void* custom);
	void sendRequestResponse(nexServiceRequest_t* request, uint32 errorCode, uint8* data, sint32 length);
	void update();
	prudpClient* getPRUDPConnection();
	sint32 getState();
	void registerForAsyncProcessing();
	void destroy();
	bool isMarkedForDestruction();

private:
	nexService();
	~nexService();
	void updateTemporaryConnections();
	void updateNexServiceConnection();
	void processQueuedRequest(queuedRequest_t* queuedRequest);
private:
	//bool serviceIsConnected;
	uint8 connectionState;
	prudpClient* conNexService;
	bool isAsync;
	bool isDestroyed; // if set, delete object asynchronously
	std::vector<nexActiveRequestInfo_t> list_activeRequests;
	// protocol request handlers
	std::vector<protocolHandler_t> list_requestHandlers;
	// packet buffer
	std::vector<uint8> bufferReceive;
	// auth
	bool isSecureService;
	// request queue
	std::mutex mtx_queuedRequests;
	std::vector<queuedRequest_t> queuedRequests;
};

nexService* nex_establishSecureConnection(uint32 authServerIp, uint16 authServerPort, const char* accessKey, uint32 pid, const char* nexPassword, const char* nexToken);