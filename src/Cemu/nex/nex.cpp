#include "prudp.h"
#include "nex.h"
#include "nexThread.h"

#include "util/crypto/md5.h"

// for inet_pton:
#if BOOST_OS_WINDOWS
#include <WS2tcpip.h>
#else
#include <arpa/inet.h>
#endif

uint32 _currentCallId = 1;

sint32 nexService_parseResponse(uint8* data, sint32 length, nexServiceResponse_t* response)
{
	if (length < 4)
		return 0;
	// get length field
	uint32 responseLength = *(uint32*)(data + 0x0);
	length -= 4;
	if (responseLength > (uint32)length)
		return 0;
	if (responseLength < 6)
		return 0;
	uint8 protocolId = *(uint8*)(data + 0x04);
	bool isRequest = (protocolId & 0x80) != 0;
	protocolId &= 0x7F;
	if (isRequest)
	{
#ifdef CEMU_DEBUG_ASSERT
		assert_dbg(); // should never reach since we handle requests before this function is called
#endif
	}
	uint8 success = *(uint8*)(data + 0x5);
	if (success == 0)
	{
		// error
		uint32 errorCode;
		if (length < 0xA)
		{
			return 0;
		}
		errorCode = *(uint32*)(data + 0x6);
		response->errorCode = errorCode;
		response->callId = *(uint32*)(data + 0xA);
		response->isSuccessful = false;
		response->protocolId = protocolId;
		response->methodId = 0xFFFFFFFF;
		return responseLength + 4;
	}
	else
	{
		if (responseLength < 0xA)
			return 0;
		response->errorCode = 0;
		response->isSuccessful = true;
		response->protocolId = protocolId;
		response->callId = *(uint32*)(data + 0x6);
		response->methodId = (*(uint32*)(data + 0xA)) & 0x7FFF;
		response->data = nexPacketBuffer(data + 0xE, responseLength - (0xE - 4), false);
		return responseLength+4;
	}
	return 0;
}

sint32 nexService_parseRequest(uint8* data, sint32 length, nexServiceRequest_t* request)
{
	if (length < 4)
		return 0;
	// get length field
	uint32 requestLength = *(uint32*)(data + 0x0) + 4;
	if (requestLength > (uint32)length)
		return 0;
	if (requestLength < 13)
		return 0;
	uint8 protocolId = *(uint8*)(data + 0x4);
	bool isRequest = (protocolId & 0x80) != 0;
	protocolId &= 0x7F;
	if(isRequest == false)
		assert_dbg();
	uint32 callId = *(uint32*)(data + 0x5);
	uint32 methodId = *(uint32*)(data + 0x9);

	uint8* dataPtr = (data + 0xD);
	sint32 dataLength = (sint32)(requestLength - 0xD);

	request->callId = callId;
	request->methodId = methodId;
	request->protocolId = protocolId;

	request->data = nexPacketBuffer(dataPtr, dataLength, false);

	return requestLength;
}

nexService::nexService()
{
	connectionState = STATE_CONNECTING;
	conNexService = nullptr;
	isAsync = false;
	isDestroyed = false;
	isSecureService = false;
}

nexService::nexService(prudpClient* con) : nexService()
{
	if (con->IsConnected() == false)
		cemu_assert_suspicious();
	this->conNexService = con;
	bufferReceive = std::vector<uint8>(1024 * 4);
}

nexService::nexService(uint32 ip, uint16 port, const char* accessKey) : nexService()
{
	// unsecured connection
	isSecureService = false;
	conNexService = new prudpClient(ip, port, accessKey);
	bufferReceive = std::vector<uint8>(1024 * 4);
}

nexService::~nexService()
{
	// call error handlers for unfinished method calls
	for (auto& it : list_activeRequests)
	{
		nexServiceResponse_t response = { 0 };
		response.isSuccessful = false;
		response.errorCode = ERR_TIMEOUT;
		response.custom = it.custom;
		if (it.nexServiceResponse)
			it.nexServiceResponse(this, &response);
		else
		{
			it.cb2(&response);
		}
	}
	if (conNexService)
		delete conNexService;
}

void nexService::destroy()
{
	if (nexThread_isCurrentThread())
	{
		delete this;
		return;
	}
	if (this->isAsync)
		isDestroyed = true;
	else
		delete this;
}

bool nexService::isMarkedForDestruction()
{
	return isDestroyed;
}

void nexService::callMethod(uint8 protocolId, uint32 methodId, nexPacketBuffer* parameter, void(*nexServiceResponse)(nexService* nex, nexServiceResponse_t* serviceResponse), void* custom, bool callHandlerIfError)
{
	queuedRequest_t queueRequest = { 0 };
	queueRequest.protocolId = protocolId;
	queueRequest.methodId = methodId;
	queueRequest.parameterData.assign(parameter->getDataPtr(), parameter->getDataPtr() + parameter->getWriteIndex());
	queueRequest.nexServiceResponse = nexServiceResponse;
	queueRequest.custom = custom;
	queueRequest.callHandlerIfError = callHandlerIfError;
	mtx_queuedRequests.lock();
	queuedRequests.push_back(queueRequest);
	mtx_queuedRequests.unlock();
}

void nexService::callMethod(uint8 protocolId, uint32 methodId, nexPacketBuffer* parameter, std::function<void(nexServiceResponse_t*)> cb, bool callHandlerIfError)
{
	queuedRequest_t queueRequest = { 0 };
	queueRequest.protocolId = protocolId;
	queueRequest.methodId = methodId;
	queueRequest.parameterData.assign(parameter->getDataPtr(), parameter->getDataPtr() + parameter->getWriteIndex());
	queueRequest.nexServiceResponse = nullptr;
	queueRequest.cb2 = cb;
	queueRequest.callHandlerIfError = callHandlerIfError;
	mtx_queuedRequests.lock();
	queuedRequests.push_back(queueRequest);
	mtx_queuedRequests.unlock();
}

void nexService::processQueuedRequest(queuedRequest_t* queuedRequest)
{
	uint32 callId = _currentCallId;
	_currentCallId++;
	// check state of connection
	if (conNexService->GetConnectionState() != prudpClient::ConnectionState::Connected)
	{
		nexServiceResponse_t response = { 0 };
		response.isSuccessful = false;
		response.errorCode = ERR_NO_CONNECTION;
		response.custom = queuedRequest->custom;
		if (queuedRequest->nexServiceResponse)
			queuedRequest->nexServiceResponse(this, &response);
		else
		{
			queuedRequest->cb2(&response);
		}
		return;
	}
	uint8 packetBuffer[1024 * 8];
	*(uint32*)(packetBuffer + 0x00) = 1 + 4 + 4 + (uint32)queuedRequest->parameterData.size(); // size
	*(uint8*)(packetBuffer + 0x04) = queuedRequest->protocolId | PROTOCOL_BIT_REQUEST;
	*(uint32*)(packetBuffer + 0x05) = callId;
	*(uint32*)(packetBuffer + 0x09) = queuedRequest->methodId;
	if (queuedRequest->parameterData.size() >= 1024 * 7)
		assert_dbg();
	memcpy((packetBuffer + 0x0D), &queuedRequest->parameterData.front(), queuedRequest->parameterData.size());
	sint32 length = 0xD + (sint32)queuedRequest->parameterData.size();
	conNexService->SendDatagram(packetBuffer, length, true);
	// remember request
	nexActiveRequestInfo_t requestInfo = { 0 };
	requestInfo.callId = callId;
	requestInfo.methodId = queuedRequest->methodId;
	requestInfo.protocolId = queuedRequest->protocolId;
	if (queuedRequest->nexServiceResponse)
	{
		requestInfo.nexServiceResponse = queuedRequest->nexServiceResponse;
		requestInfo.custom = queuedRequest->custom;
	}
	else
	{
		requestInfo.cb2 = queuedRequest->cb2;
	}
	requestInfo.handleError = queuedRequest->callHandlerIfError;
	requestInfo.requestTime = prudpGetMSTimestamp();
	list_activeRequests.push_back(requestInfo);
}

void nexService::update()
{
	if (connectionState == STATE_CONNECTED)
	{
		updateNexServiceConnection();
		// process any queued requests
		mtx_queuedRequests.lock();
		for (auto& request : queuedRequests)
		{
			processQueuedRequest(&request);
		}
		queuedRequests.clear();
		mtx_queuedRequests.unlock();
	}
	else if (connectionState == STATE_CONNECTING)
	{
		updateTemporaryConnections();
	}
	// handle request timeouts
	uint32 currentTimestamp = prudpGetMSTimestamp();
	sint32 idx = 0;
	while (idx < list_activeRequests.size())
	{
		auto& it = list_activeRequests[idx];
		if ((uint32)(currentTimestamp - it.requestTime) >= 10000)
		{
			// time out after 10 seconds
			nexServiceResponse_t response = { 0 };
			response.isSuccessful = false;
			response.errorCode = ERR_TIMEOUT;
			response.custom = it.custom;
			if (it.nexServiceResponse)
				it.nexServiceResponse(this, &response);
			else
			{
				it.cb2(&response);
			}
			list_activeRequests.erase(list_activeRequests.cbegin()+idx);
			continue;
		}
		idx++;
	}
}

prudpClient* nexService::getPRUDPConnection()
{
	return conNexService;
}

sint32 nexService::getState()
{
	return connectionState;
}
	
void nexService::registerForAsyncProcessing()
{
	if (isAsync)
		return;
	nexThread_registerService(this);
	isAsync = true;
}

void nexService::updateTemporaryConnections()
{
	// check for connection
	conNexService->Update();
	if (conNexService->IsConnected())
	{
		if (connectionState == STATE_CONNECTING)
			connectionState = STATE_CONNECTED;
	}
	if (conNexService->GetConnectionState() == prudpClient::ConnectionState::Disconnected)
		connectionState = STATE_DISCONNECTED;
}

// returns true if the packet is a nex RPC request, in case of error false is returned
bool nexIsRequest(uint8* data, sint32 length)
{
	if (length < 5)
		return false;
	uint8 protocolId = *(uint8*)(data + 0x04);
	bool isRequest = (protocolId & 0x80) != 0;
	return isRequest;
}

void nexService::registerProtocolRequestHandler(uint8 protocol, void(*processRequest)(nexServiceRequest_t* request), void* custom)
{
	protocolHandler_t protocolHandler;
	protocolHandler.protocol = protocol;
	protocolHandler.processRequest = processRequest;
	protocolHandler.custom = custom;
	list_requestHandlers.push_back(protocolHandler);
}

void nexService::sendRequestResponse(nexServiceRequest_t* request, uint32 errorCode, uint8* data, sint32 length)
{
	cemu_assert_debug(length == 0); // non-zero length is todo

	uint8 packetBuffer[256];
	nexPacketBuffer response(packetBuffer, sizeof(packetBuffer), true);

	bool isSuccess = (errorCode == 0);
	// write placeholder for response length
	response.writeU32(0);
	// header fields
	response.writeU8(request->protocolId&0x7F);
	response.writeU8(isSuccess);
	if (isSuccess)
	{
		response.writeU32(request->callId);
		response.writeU32(request->methodId);
		// write data
		// todo
	}
	else
	{
		response.writeU32(errorCode);
		response.writeU32(request->callId);
	}
	// update length field
	*(uint32*)response.getDataPtr() = response.getWriteIndex()-4;
	if(request->nex->conNexService)
		request->nex->conNexService->SendDatagram(response.getDataPtr(), response.getWriteIndex(), true);
}

void nexService::updateNexServiceConnection()
{
	if (conNexService->GetConnectionState() == prudpClient::ConnectionState::Disconnected)
	{
		this->connectionState = STATE_DISCONNECTED;
		return;
	}
	conNexService->Update();
	sint32 datagramLen = conNexService->ReceiveDatagram(bufferReceive);
	if (datagramLen > 0)
	{
		if (nexIsRequest(&bufferReceive[0], datagramLen))
		{
			// request
			nexServiceRequest_t nexServiceRequest;
			sint32 parsedLength = nexService_parseRequest(&bufferReceive[0], datagramLen, &nexServiceRequest);
			if (parsedLength > 0)
			{
				nexServiceRequest.nex = this;
				// call request handler
				for (auto& it : list_requestHandlers)
				{
					if (it.protocol == nexServiceRequest.protocolId)
					{
						nexServiceRequest.custom = it.custom;
						it.processRequest(&nexServiceRequest);
						break;
					}
				}
			}
			cemu_assert_debug(parsedLength == datagramLen);
		}
		else
		{
			// response
			nexServiceResponse_t nexServiceResponse;
			sint32 parsedLength = nexService_parseResponse(&bufferReceive[0], datagramLen, &nexServiceResponse);
			if (parsedLength > 0)
			{
				nexServiceResponse.nex = this;
				// call response handler
				for (sint32 i = 0; i < list_activeRequests.size(); i++)
				{
					if (nexServiceResponse.callId == list_activeRequests[i].callId &&
						nexServiceResponse.protocolId == list_activeRequests[i].protocolId &&
						(nexServiceResponse.methodId == list_activeRequests[i].methodId || nexServiceResponse.methodId == 0xFFFFFFFF))
					{
						nexServiceResponse.custom = list_activeRequests[i].custom;
						if (nexServiceResponse.isSuccessful || list_activeRequests[i].handleError)
						{
							if (list_activeRequests[i].nexServiceResponse)
								list_activeRequests[i].nexServiceResponse(this, &nexServiceResponse);
							else
							{
								list_activeRequests[i].cb2(&nexServiceResponse);
							}
						}
						// remove entry
						list_activeRequests.erase(list_activeRequests.begin() + i);
						break;
					}
				}
			}
			cemu_assert_debug(parsedLength == datagramLen);
		}
	}
}

bool _extractStationUrlParamValue(const char* urlStr, const char* paramName, char* output, sint32 maxLength)
{	
	size_t paramNameLen = strlen(paramName);
	const char* optionPtr = strstr(urlStr, paramName);
	while (optionPtr)
	{
		if (optionPtr[paramNameLen] == '=')
		{
			output[maxLength - 1] = '\0';
			for (sint32 i = 0; i < maxLength - 1; i++)
			{
				char c = optionPtr[paramNameLen + 1 + i];
				if (c == ';' || c == '\0')
				{
					output[i] = '\0';
					break;
				}
				output[i] = c;
			}
			return true;
		}
		// next
		optionPtr = strstr(optionPtr+1, paramName);
	}
	return false;
}

void nexServiceAuthentication_parseStationURL(char* urlStr, prudpStationUrl* stationUrl)
{
	// example:
	// prudps:/address=34.210.xxx.xxx;port=60181;CID=1;PID=2;sid=1;stream=10;type=2

	memset(stationUrl, 0, sizeof(prudpStationUrl));

	char optionValue[128];
	if (_extractStationUrlParamValue(urlStr, "address", optionValue, sizeof(optionValue)))
	{
		inet_pton(AF_INET, optionValue, &stationUrl->ip);
	}
	if (_extractStationUrlParamValue(urlStr, "port", optionValue, sizeof(optionValue)))
	{
		stationUrl->port = atoi(optionValue);
	}
	if (_extractStationUrlParamValue(urlStr, "CID", optionValue, sizeof(optionValue)))
	{
		stationUrl->cid = atoi(optionValue);
	}
	if (_extractStationUrlParamValue(urlStr, "PID", optionValue, sizeof(optionValue)))
	{
		stationUrl->pid = atoi(optionValue);
	}
	if (_extractStationUrlParamValue(urlStr, "sid", optionValue, sizeof(optionValue)))
	{
		stationUrl->sid = atoi(optionValue);
	}
	if (_extractStationUrlParamValue(urlStr, "stream", optionValue, sizeof(optionValue)))
	{
		stationUrl->stream = atoi(optionValue);
	}
	if (_extractStationUrlParamValue(urlStr, "type", optionValue, sizeof(optionValue)))
	{
		stationUrl->type = atoi(optionValue);
	}
}

typedef struct  
{
	uint32 userPid;
	uint8 kerberosTicket[1024];
	sint32 kerberosTicketSize;
	uint8 kerberosTicket2[4096];
	sint32 kerberosTicket2Size;
	prudpStationUrl server;
	// progress info
	bool hasError;
	bool done;
}authenticationService_t;

void nexServiceAuthentication_handleResponse_requestTicket(nexService* nex, nexServiceResponse_t* response)
{
	authenticationService_t* authService = (authenticationService_t*)response->custom;
	if (response->isSuccessful == false)
	{
		cemuLog_log(LogType::Force, "NEX: RPC error while requesting auth ticket with error code 0x{:08x}", response->errorCode);
		authService->hasError = true;
		return;
	}
	uint32 returnValue = response->data.readU32();
	if (returnValue & 0x80000000)
	{
		cemuLog_log(LogType::Force, "NEX: Failed to request auth ticket with error code 0x{:08x}", returnValue);
		authService->hasError = true;
	}
	authService->kerberosTicket2Size = response->data.readBuffer(authService->kerberosTicket2, sizeof(authService->kerberosTicket2));
	if (response->data.hasReadOutOfBounds())
	{
		authService->hasError = true;
		cemuLog_log(LogType::Force, "NEX: Out of bounds error while reading auth ticket");
		return;
	}
	authService->done = true;
}

void nexServiceAuthentication_handleResponse_login(nexService* nex, nexServiceResponse_t* response)
{
	authenticationService_t* authService = (authenticationService_t*)response->custom;
	if (response->isSuccessful == false)
	{
		authService->hasError = true;
		cemuLog_log(LogType::Force, "NEX: RPC error in login response 0x{:08x}", response->errorCode);
		return;
	}

	uint32 returnValue = response->data.readU32();
	if (returnValue & 0x80000000)
	{
		authService->hasError = true;
		cemuLog_log(LogType::Force, "NEX: Error 0x{:08x} in login response (returnCode 0x{:08x})", response->errorCode, returnValue);
		return;
	}

	uint32 userPid = response->data.readU32();
	
	// kerberos ticket
	authService->kerberosTicketSize = response->data.readBuffer(authService->kerberosTicket, sizeof(authService->kerberosTicket));
	// RVConnection data
	// server address (regular protocol)
	char serverAddress[1024];
	response->data.readString(serverAddress, sizeof(serverAddress));
	nexServiceAuthentication_parseStationURL(serverAddress, &authService->server);
	// special protocols
	if (response->data.readU32() != 0)
		assert_dbg(); // list not empty
	char specialAddress[32];
	response->data.readString(specialAddress, sizeof(specialAddress));
	// V1 has extra info here
	// server name
	char serverName[256];
	response->data.readString(serverName, sizeof(serverName));
	if (response->data.hasReadOutOfBounds())
	{
		authService->hasError = true;
		cemuLog_log(LogType::Force, "NEX: Read out of bounds");
		return;
	}
	// request ticket data
	uint8 tempNexBufferArray[1024];
	nexPacketBuffer packetBuffer(tempNexBufferArray, sizeof(tempNexBufferArray), true);
	packetBuffer.writeU32(authService->userPid);
	packetBuffer.writeU32(authService->server.pid);
	nex->callMethod(NEX_PROTOCOL_AUTHENTICATION, 3, &packetBuffer, nexServiceAuthentication_handleResponse_requestTicket, authService);
}

typedef struct  
{
	bool isComplete;
	bool isSuccessful;
}nexServiceSecureRegisterExData_t;

void nexServiceSecure_handleResponse_RegisterEx(nexService* nex, nexServiceResponse_t* response)
{
	nexServiceSecureRegisterExData_t* info = (nexServiceSecureRegisterExData_t*)response->custom;
	info->isComplete = true;
	uint32 returnCode = response->data.readU32();
	if (response->isSuccessful == false || response->data.hasReadOutOfBounds())
	{
		cemuLog_log(LogType::Force, "NEX: RPC error in secure register");
		info->isSuccessful = false;
		info->isComplete = true;
		return;
	}
	if (returnCode & 0x80000000)
	{
		cemuLog_log(LogType::Force, "NEX: Secure register failed with error code 0x{:08x}", returnCode);
		info->isSuccessful = false;
		info->isComplete = true;
		return;
	}
	// remaining data is skipped
	info->isSuccessful = true;
	info->isComplete = true;
	return;
}

nexService* nex_secureLogin(prudpAuthServerInfo* authServerInfo, const char* accessKey, const char* nexToken)
{
	prudpClient* prudpSecureSock = new prudpClient(authServerInfo->server.ip, authServerInfo->server.port, accessKey, authServerInfo);
	// wait until connected
	while (true)
	{
		prudpSecureSock->Update();
		if (prudpSecureSock->IsConnected())
		{
			break;
		}
		if (prudpSecureSock->GetConnectionState() == prudpClient::ConnectionState::Disconnected)
		{
			// timeout or disconnected
			cemuLog_log(LogType::Force, "NEX: Secure login connection time-out");
			delete prudpSecureSock;
			return nullptr;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	// create nex service layer
	nexService* nex = new nexService(prudpSecureSock);
	// secureService: RegisterEx
	uint8 tempNexBufferArray[4096];
	nexPacketBuffer packetBuffer(tempNexBufferArray, sizeof(tempNexBufferArray), true);
	
	char clientStationUrl[256];
	sprintf(clientStationUrl, "prudp:/port=%u;natf=0;natm=0;pmp=0;sid=15;type=2;upnp=0", (uint32)nex->getPRUDPConnection()->GetSourcePort());
	// station url list
	packetBuffer.writeU32(1);
	packetBuffer.writeString(clientStationUrl);
	// login data
	packetBuffer.writeCustomType(nexNintendoLoginData(nexToken));
	
	nexServiceSecureRegisterExData_t secureRegisterExData = { 0 };
	nex->callMethod(NEX_PROTOCOL_SECURE, 4, &packetBuffer, nexServiceSecure_handleResponse_RegisterEx, &secureRegisterExData);
	while (true)
	{
		nex->update();
		if (secureRegisterExData.isComplete)
			break;
		if (nex->getState() == nexService::STATE_DISCONNECTED)
		{
			cemuLog_log(LogType::Force, "NEX: Connection error while registering");
			break;
		}
	}
	if (secureRegisterExData.isSuccessful == false)
	{
		cemuLog_log(LogType::Force, "NEX: Failed to register to secure server");
		nex->destroy();
		return nullptr;
	}
	return nex;
}

nexService* nex_establishSecureConnection(uint32 authServerIp, uint16 authServerPort, const char* accessKey, uint32 pid, const char* nexPassword, const char* nexToken)
{
	nexService* authConnection = new nexService(authServerIp, authServerPort, accessKey);
	// wait until connected
	while (authConnection->getState() == nexService::STATE_CONNECTING)
	{
		authConnection->update();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (authConnection->getState() != nexService::STATE_CONNECTED)
	{
		// error
		authConnection->destroy();
		cemuLog_log(LogType::Force, "NEX: Failed to connect to the NEX server");
		return nullptr;
	}
	// send auth login request
	uint8 tempNexBufferArray[1024];
	nexPacketBuffer packetBuffer(tempNexBufferArray, sizeof(tempNexBufferArray), true);
	char pidStr[32];
	sprintf(pidStr, "%u", pid);
	packetBuffer.writeString(pidStr);
	authenticationService_t nexAuthService = { 0 };
	nexAuthService.userPid = pid;
	authConnection->callMethod(NEX_PROTOCOL_AUTHENTICATION, 1, &packetBuffer, nexServiceAuthentication_handleResponse_login, &nexAuthService);
	while (true)
	{
		if (nexAuthService.hasError || nexAuthService.done || authConnection->getState() != nexService::STATE_CONNECTED)
			break;
		authConnection->update();
	}
	if (nexAuthService.hasError || authConnection->getState() != nexService::STATE_CONNECTED)
	{
		// error
		authConnection->destroy();
		cemuLog_log(LogType::Force, "NEX: Error during authentication");
		return nullptr;
	}
	// close connection to auth server
	authConnection->destroy();
	// calculate kerberos decryption key
	uint32 md5LoopCount = 65000 + (pid % 1024);
	md5LoopCount--;
	uint8 kerberosKey[16];
	MD5_CTX md5Ctx;
	MD5_Init(&md5Ctx);
	MD5_Update(&md5Ctx, nexPassword, (unsigned long)strlen(nexPassword));
	MD5_Final(kerberosKey, &md5Ctx);
	for (uint32 i = 0; i < md5LoopCount; i++)
	{
		MD5_Init(&md5Ctx);
		MD5_Update(&md5Ctx, kerberosKey, 16);
		MD5_Final(kerberosKey, &md5Ctx);
	}
	if (nexAuthService.kerberosTicket2Size < 16)
	{
		nexAuthService.hasError = true;
		cemuLog_log(LogType::Force, "NEX: Kerberos ticket too short");
		return nullptr;
	}
	// check hmac of ticket
	uint8 hmacTicket[16];
	hmacMD5(kerberosKey, 16, nexAuthService.kerberosTicket2, nexAuthService.kerberosTicket2Size - 16, hmacTicket);
	if (memcmp(hmacTicket, nexAuthService.kerberosTicket2 + nexAuthService.kerberosTicket2Size - 16, 16) != 0)
	{
		nexAuthService.hasError = true;
		cemuLog_log(LogType::Force, "NEX: Kerberos ticket hmac invalid");
		return nullptr;
	}
	// auth info
	auto authServerInfo = std::make_unique<prudpAuthServerInfo>();
	// decrypt ticket
	RC4Ctx rc4Ticket;
	RC4_initCtx(&rc4Ticket, kerberosKey, 16);
	RC4_transform(&rc4Ticket, nexAuthService.kerberosTicket2, nexAuthService.kerberosTicket2Size - 16, nexAuthService.kerberosTicket2);
	nexPacketBuffer packetKerberosTicket(nexAuthService.kerberosTicket2, nexAuthService.kerberosTicket2Size - 16, false);
	uint8 secureKey[16]; // for friends server it's 16 bytes, all others use 32 bytes
	packetKerberosTicket.readData(secureKey, 16);
	uint32 ukn = packetKerberosTicket.readU32();
	authServerInfo->secureTicketLength = packetKerberosTicket.readBuffer(authServerInfo->secureTicket, sizeof(authServerInfo->secureTicket));

	if (packetKerberosTicket.hasReadOutOfBounds())
	{
		cemuLog_log(LogType::Force, "NEX: Parse error");
		return nullptr;
	}

	memcpy(authServerInfo->kerberosKey, kerberosKey, 16);
	memcpy(authServerInfo->secureKey, secureKey, 16);
	memcpy(&authServerInfo->server, &nexAuthService.server, sizeof(prudpStationUrl));
	authServerInfo->userPid = pid;

	return nex_secureLogin(authServerInfo.get(), accessKey, nexToken);
}
