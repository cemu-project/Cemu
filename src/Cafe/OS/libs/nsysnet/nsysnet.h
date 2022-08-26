#pragma once
#include <set>
#include <vector>

#if BOOST_OS_WINDOWS
#include <WinSock2.h>
#else
#include <sys/socket.h>
#define SOCKET int
#define closesocket close
#endif

typedef signed int WUSOCKET;

void nsysnet_load();
WUSOCKET nsysnet_createVirtualSocketFromExistingSocket(SOCKET existingSocket);
void nsysnet_notifyCloseSharedSocket(SOCKET existingSocket);

struct wu_fd_set
{
	uint32be mask;
};

void _translateFDSet(fd_set* hostSet, struct wu_fd_set* fdset, sint32 nfds);
void _translateFDSetRev(struct wu_fd_set* fdset, fd_set* hostSet, sint32 nfds);

sint32 nsysnet_getVirtualSocketHandleFromHostHandle(SOCKET s);

namespace nsysnet
{

#define NSSL_OK (0)
#define NSSL_INVALID_CTX (0xFFD7FFFF)

	struct NSSLInternalState_t
	{
		bool destroyed;
		uint32 sslVersion;
		uint32 clientPKI;
		std::set<uint32> serverPKIs;
		std::vector<std::vector<uint8>> serverCustomPKIs;
	};

	NSSLInternalState_t* GetNSSLContext(sint32 index);

	void wuResetFD(struct wu_fd_set* fdset);
	void wuSetFD(struct wu_fd_set* fdset, sint32 fd);
}