#include "Cafe/OS/common/OSCommon.h"
#include "nsysnet.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/IOSU/legacy/iosu_crypto.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"

#include "Common/socket.h"

#if BOOST_OS_UNIX

#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAEINPROGRESS EINPROGRESS
#define WSAESHUTDOWN ESHUTDOWN
#define WSAECONNABORTED ECONNABORTED
#define WSAHOST_NOT_FOUND EAI_NONAME
#define WSAENOTCONN ENOTCONN

#define GETLASTERR errno

#endif // BOOST_OS_UNIX

#if BOOST_OS_WINDOWS

#define GETLASTERR WSAGetLastError()

#endif //BOOST_OS_WINDOWS

#define WU_AF_INET			2

#define WU_SOCK_STREAM		1
#define WU_SOCK_DGRAM		2

#define WU_IPPROTO_IP		0
#define WU_IPPROTO_TCP		6
#define WU_IPPROTO_UDP		17

#define WU_SO_REUSEADDR		0x0004
#define WU_SO_KEEPALIVE		0x0008
#define WU_SO_DONTROUTE		0x0010
#define WU_SO_BROADCAST		0x0020
#define WU_SO_LINGER		0x0080
#define WU_SO_OOBINLINE		0x0100
#define WU_SO_TCPSACK		0x0200
#define WU_SO_WINSCALE		0x0400
#define WU_SO_SNDBUF		0x1001
#define WU_SO_RCVBUF		0x1002
#define WU_SO_SNDLOWAT		0x1003
#define WU_SO_RCVLOWAT		0x1004
#define WU_SO_LASTERROR		0x1007
#define WU_SO_TYPE			0x1008
#define WU_SO_HOPCNT		0x1009
#define WU_SO_MAXMSG		0x1010
#define WU_SO_RXDATA		0x1011
#define WU_SO_TXDATA		0x1012
#define WU_SO_MYADDR		0x1013
#define WU_SO_NBIO			0x1014
#define WU_SO_BIO			0x1015
#define WU_SO_NONBLOCK		0x1016
#define WU_SO_UNKNOWN1019	0x1019 // tcp related
#define WU_SO_UNKNOWN101A	0x101A // tcp related
#define WU_SO_UNKNOWN101B	0x101B // tcp related
#define WU_SO_NOSLOWSTART	0x4000
#define WU_SO_RUSRBUF		0x10000

#define WU_TCP_ACKDELAYTIME		0x2001
#define WU_TCP_NOACKDELAY		0x2002
#define WU_TCP_MAXSEG			0x2003
#define WU_TCP_NODELAY			0x2004
#define WU_TCP_UNKNOWN			0x2005 // amount of mss received before sending an ack

#define WU_IP_TOS				3
#define WU_IP_TTL				4
#define WU_IP_MULTICAST_IF		9
#define WU_IP_MULTICAST_TTL		10
#define WU_IP_MULTICAST_LOOP	11
#define WU_IP_ADD_MEMBERSHIP	12
#define WU_IP_DROP_MEMBERSHIP	13
#define WU_IP_UNKNOWN			14

#define WU_SOL_SOCKET		-1 // this constant differs from Win32 socket API

#define WU_MSG_PEEK			0x02
#define WU_MSG_DONTWAIT		0x20

// error codes
#define WU_SO_SUCCESS		0x0000
#define WU_SO_EWOULDBLOCK	0x0006
#define WU_SO_ECONNRESET	0x0008
#define WU_SO_ENOTCONN		0x0009
#define WU_SO_EINVAL		0x000B
#define WU_SO_EINPROGRESS	0x0016
#define WU_SO_EAFNOSUPPORT  0x0021

#define WU_SO_ESHUTDOWN		0x000F


typedef signed int WUSOCKET;

bool sockLibReady = false;

void nsysnetExport_socket_lib_init(PPCInterpreter_t* hCPU)
{
	sockLibReady = true;
#if BOOST_OS_WINDOWS
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif // BOOST_OS_WINDOWS
	osLib_returnFromFunction(hCPU, 0); // 0 -> Success
}

void nsysnetExport_socket_lib_finish(PPCInterpreter_t* hCPU)
{
	sockLibReady = false;
#if BOOST_OS_WINDOWS
	WSACleanup();
#endif // BOOST_OS_WINDOWS
	osLib_returnFromFunction(hCPU, 0); // 0 -> Success
}

static uint32be* __gh_errno_ptr()
{
	OSThread_t* osThread = coreinit::OSGetCurrentThread();
	return &osThread->context.ghs_errno;
}

void _setSockError(sint32 errCode)
{
	*(uint32be*)__gh_errno_ptr() = (uint32)errCode;
}

sint32 _getSockError()
{
	return (sint32)*(uint32be*)__gh_errno_ptr();
}

// error translation modes for _translateError
#define _ERROR_MODE_DEFAULT		0
#define _ERROR_MODE_CONNECT		1
#define _ERROR_MODE_ACCEPT		2

sint32 _translateError(sint32 returnCode, sint32 wsaError, sint32 mode = _ERROR_MODE_DEFAULT)
{
	if (mode == _ERROR_MODE_ACCEPT)
	{
		// accept mode
		if (returnCode >= 0)
		{
			_setSockError(WU_SO_SUCCESS);
			return returnCode;
		}
	}
	else
	{
		// any other mode
		if (returnCode == 0)
		{
			_setSockError(WU_SO_SUCCESS);
			return 0;
		}
	}
	// handle WSA error
	switch (wsaError)
	{
	case 0:
		_setSockError(WU_SO_SUCCESS);
		break;
	case WSAEWOULDBLOCK:
		if( mode == _ERROR_MODE_CONNECT )
			_setSockError(WU_SO_EINPROGRESS);
		else
			_setSockError(WU_SO_EWOULDBLOCK);
		break;
	case WSAEINPROGRESS:
		_setSockError(WU_SO_EINPROGRESS);
		break;
	case WSAECONNABORTED:
		debug_printf("WSAECONNABORTED\n");
#ifdef CEMU_DEBUG_ASSERT
		assert_dbg();
#endif
		break;
	case WSAESHUTDOWN:
		_setSockError(WU_SO_ESHUTDOWN);
		break;
	case WSAENOTCONN:
		_setSockError(WU_SO_ENOTCONN);
		break;
	default:
		cemuLog_logDebug(LogType::Force, "Unhandled wsaError {}", wsaError);
		_setSockError(99999); // unhandled error
	}
	return -1;
}

void nsysnetExport_socketlasterr(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Socket, "socketlasterr() -> {}", _getSockError());
	osLib_returnFromFunction(hCPU, _getSockError());
}

typedef struct  
{
	uint32 handle;
	bool isShutdownRecv;
	bool isShutdownSend;
	// socket creation info
	sint32 family;
	sint32 type;
	sint32 protocol;
	// host side info
	SOCKET s;
	// socket options
	bool isNonBlocking;
}virtualSocket_t;

typedef struct
{
	uint32 wu_s_addr;
}wu_in_addr;

struct wu_sockaddr
{
	uint16 sa_family;
	uint8  sa_data[14]; // IPv4
};

void sockaddr_guest2host(wu_sockaddr* input, sockaddr* output)
{
	output->sa_family = _swapEndianU16(input->sa_family);
	memcpy(output->sa_data, input->sa_data, 14);
}

void sockaddr_host2guest(sockaddr* input, wu_sockaddr* output)
{
	output->sa_family = _swapEndianU16(input->sa_family);
	memcpy(output->sa_data, input->sa_data, 14);
}

struct wu_addrinfo 
{
	sint32 ai_flags;
	sint32 ai_family;
	sint32 ai_socktype;
	sint32 ai_protocol;
	sint32 ai_addrlen;
	MPTR   ai_canonname;
	MPTR   ai_addr;
	MPTR   ai_next;
};

#define WU_SOCKET_LIMIT			(32) // only 32 socket handles are supported per running process

virtualSocket_t* virtualSocketTable[WU_SOCKET_LIMIT] = { 0 };

sint32 _getFreeSocketHandle()
{
	for (sint32 i = 0; i < WU_SOCKET_LIMIT; i++)
	{
		if (virtualSocketTable[i] == NULL)
			return i + 1;
	}
	return 0;
}

#if BOOST_OS_WINDOWS
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR,12)
#endif // BOOST_OS_WINDOWS

WUSOCKET nsysnet_createVirtualSocket(sint32 family, sint32 type, sint32 protocol)
{
	sint32 s = _getFreeSocketHandle();
	if (s == 0)
	{
		cemuLog_logDebug(LogType::Force, "Ran out of socket handles");
		cemu_assert(false);
	}
	virtualSocket_t* vs = (virtualSocket_t*)malloc(sizeof(virtualSocket_t));
	memset(vs, 0, sizeof(virtualSocket_t));
	vs->family = family;
	vs->type = type;
	vs->protocol = protocol;
	vs->handle = s;
	virtualSocketTable[s - 1] = vs;
	// init host socket
	vs->s = socket(family, type, protocol);
	#if BOOST_OS_WINDOWS
	// disable reporting of PORT_UNREACHABLE for UDP sockets
	if (protocol == IPPROTO_UDP)
	{
		BOOL bNewBehavior = FALSE;
		DWORD dwBytesReturned = 0;
		WSAIoctl(vs->s, SIO_UDP_CONNRESET, &bNewBehavior, sizeof bNewBehavior, NULL, 0, &dwBytesReturned, NULL, NULL);
	}
	#endif // BOOST_OS_WINDOWS
	return vs->handle;
}

WUSOCKET nsysnet_createVirtualSocketFromExistingSocket(SOCKET existingSocket)
{
	cemuLog_logDebug(LogType::Force, "nsysnet_createVirtualSocketFromExistingSocket - incomplete");


	sint32 s = _getFreeSocketHandle();
	if (s == 0)
	{
		cemuLog_logDebug(LogType::Force, "Ran out of socket handles");
		cemu_assert(false);
	}
	virtualSocket_t* vs = (virtualSocket_t*)malloc(sizeof(virtualSocket_t));
	memset(vs, 0, sizeof(virtualSocket_t));
#if BOOST_OS_WINDOWS
	// SO_TYPE -> type
	// SO_BSP_STATE -> protocol + other info
	// SO_PROTOCOL_INFO -> protocol + type?

	WSAPROTOCOL_INFO protocolInfo = { 0 };
	int optLen = sizeof(protocolInfo);
	getsockopt(existingSocket, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&protocolInfo, &optLen);
	// todo - translate protocolInfo 
	vs->family = protocolInfo.iAddressFamily;
	vs->type = protocolInfo.iSocketType;
	vs->protocol = protocolInfo.iSocketType;
#else
	{
		int type;
		socklen_t optlen;
		getsockopt(vs->s, SOL_SOCKET, SO_TYPE, &type, &optlen);
		vs->type = type;
		vs->protocol = type;
	}
	{
		sockaddr saddr;
		socklen_t len;
		getsockname(vs->s, &saddr, &len);
		vs->family = saddr.sa_family;
	}
#endif

	vs->handle = s;
	virtualSocketTable[s - 1] = vs;
	vs->s = existingSocket;
	return vs->handle;
}

void nsysnet_notifyCloseSharedSocket(SOCKET existingSocket)
{
	for (sint32 i = 0; i < WU_SOCKET_LIMIT; i++)
	{
		if (virtualSocketTable[i] && virtualSocketTable[i]->s == existingSocket)
		{
			// remove entry
			free(virtualSocketTable[i]);
			virtualSocketTable[i] = nullptr;
			return;
		}
	}
}

virtualSocket_t* nsysnet_getVirtualSocketObject(WUSOCKET s)
{
	uint8 handleType = 0;
	s--;
	if (s < 0 || s >= WU_SOCKET_LIMIT)
		return NULL;
	return virtualSocketTable[s];
}

sint32 nsysnet_getVirtualSocketHandleFromHostHandle(SOCKET s)
{
	for (sint32 i = 0; i < WU_SOCKET_LIMIT; i++)
	{
		if (virtualSocketTable[i] && virtualSocketTable[i]->s == s)
			return i + 1;
	}
	return -1;
}

void nsysnetExport_socket(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "socket({},{},{})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
	ppcDefineParamS32(family, 0);
	ppcDefineParamS32(type, 1);
	ppcDefineParamS32(protocol, 2);

	if (sockLibReady == false)
	{
		_setSockError(0x2B);
		osLib_returnFromFunction(hCPU, -1);
		return;
	}
	// below here Ioctl code should start

	// check family param
	if (family != WU_AF_INET)
	{
		cemuLog_logDebug(LogType::Force, "socket(): Unsupported family");
		// todo - error code
		osLib_returnFromFunction(hCPU, -1);
		return;
	}
	// check type param
	if (type != WU_SOCK_STREAM && type != WU_SOCK_DGRAM)
	{
		cemuLog_logDebug(LogType::Force, "socket(): Unsupported family");
		// todo - error code
		osLib_returnFromFunction(hCPU, -1);
		return;
	}

	if (protocol != WU_IPPROTO_TCP && protocol != WU_IPPROTO_UDP && protocol != WU_IPPROTO_IP)
	{
		cemuLog_logDebug(LogType::Force, "socket(): Unsupported protocol");
		// todo - error code
		osLib_returnFromFunction(hCPU, -1);
		return;
	}

	WUSOCKET s = nsysnet_createVirtualSocket(family, type, protocol);
	cemuLog_log(LogType::Socket, "Created socket handle {}", s);
	osLib_returnFromFunction(hCPU, s);
}

void nsysnetExport_mw_socket(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "mw_socket");
	nsysnetExport_socket(hCPU);
}

void nsysnetExport_shutdown(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "shutdown({},{})", hCPU->gpr[3], hCPU->gpr[4]);
	ppcDefineParamS32(s, 0);
	ppcDefineParamS32(how, 1);

	sint32 r = 0;
	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	if (vs == NULL)
	{
		assert_dbg();
	}
	else
	{
		r = shutdown(vs->s, how);
		if (how == 0)
		{
			// shutdown recv
			vs->isShutdownRecv = true;
		}
		else if (how == 1)
		{
			// shutdown send
			vs->isShutdownSend = true;
		}
		else if (how == 2)
		{
			// shutdown recv & send
			vs->isShutdownRecv = true;
			vs->isShutdownSend = true;
		}
		else
			assert_dbg();
	}
	_setSockError(0); // todo
	osLib_returnFromFunction(hCPU, r);
}

void nsysnetExport_socketclose(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "socketclose({})", hCPU->gpr[3]);
	ppcDefineParamS32(s, 0);

	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	if (vs)
	{
		closesocket(vs->s);
		free(vs);
		virtualSocketTable[s - 1] = NULL;
	}
	osLib_returnFromFunction(hCPU, 0);
}
sint32 _socket_nonblock(SOCKET s, u_long mode)
{
#if BOOST_OS_WINDOWS
	return ioctlsocket(s, FIONBIO, &mode);
#else
	int flags = fcntl(s, F_GETFL);
	if(mode)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;
	return fcntl(s, F_SETFL, flags);
#endif
}
void nsysnetExport_setsockopt(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "setsockopt({},0x{:x},0x{:05x},0x{:08x},{})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7]);
	ppcDefineParamS32(s, 0);
	ppcDefineParamS32(level, 1);
	ppcDefineParamS32(optname, 2);
	ppcDefineParamStr(optval, 3);
	ppcDefineParamS32(optlen, 4);

	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	if (!vs)
	{
		cemu_assert_suspicious();
	}
	else
	{
		// translate level to host API
		sint32 hostLevel;
		if (level == WU_SOL_SOCKET)
		{
			hostLevel = SOL_SOCKET;
			// handle op
			if (optname == WU_SO_REUSEADDR)
			{
				if (optlen != 4)
					cemu_assert_suspicious();
				sint32 optvalLE = _swapEndianU32(*(uint32*)optval);
				sint32 r = setsockopt(vs->s, hostLevel, SO_REUSEADDR, (char*)&optvalLE, 4);
				if (r != 0)
					cemu_assert_suspicious();
			}
			else if (optname == WU_SO_NBIO || optname == WU_SO_BIO)
			{
				// similar to WU_SO_NONBLOCK but always sets blocking (_BIO) or non-blocking (_NBIO) mode regardless of option value
				if (optlen == 4)
				{
					sint32 optvalLE = _swapEndianU32(*(uint32*)optval);
				}
				else if (optlen == 0)
				{
					// no opt needed
				}
				else
					cemu_assert_suspicious();
				bool setNonBlocking = optname == WU_SO_NBIO;
				u_long mode = setNonBlocking ? 1 : 0;
				_socket_nonblock(vs->s,  mode);
				vs->isNonBlocking = setNonBlocking;
			}
			else if (optname == WU_SO_NONBLOCK)
			{
				if (optlen != 4)
					assert_dbg();
				sint32 optvalLE = _swapEndianU32(*(uint32*)optval);
				u_long mode = optvalLE;  // 1 -> enable, 0 -> disable
				_socket_nonblock(vs->s,  mode);
				vs->isNonBlocking = mode != 0;
			}
			else if (optname == WU_SO_KEEPALIVE)
			{
				cemuLog_logDebug(LogType::Socket, "todo: setsockopt() for WU_SO_KEEPALIVE");
			}
			else if (optname == WU_SO_WINSCALE)
			{
				cemuLog_logDebug(LogType::Socket, "todo: setsockopt() for WU_SO_WINSCALE");
			}
			else if (optname == WU_SO_RCVBUF)
			{
				cemuLog_log(LogType::Socket, "Set receive buffer size to 0x{:08x}", _swapEndianU32(*(uint32*)optval));
				if (optlen != 4)
					assert_dbg();
				sint32 optvalLE = _swapEndianU32(*(uint32*)optval);
				u_long mode = optvalLE;
				if (setsockopt(vs->s, SOL_SOCKET, SO_RCVBUF, (const char*)&mode, sizeof(u_long)) != 0)
					assert_dbg();
			}
			else if (optname == WU_SO_SNDBUF)
			{
				cemuLog_log(LogType::Socket, "Set send buffer size to 0x{:08x}", _swapEndianU32(*(uint32*)optval));
				if (optlen != 4)
					assert_dbg();
				sint32 optvalLE = _swapEndianU32(*(uint32*)optval);
				u_long mode = optvalLE;
				if (setsockopt(vs->s, SOL_SOCKET, SO_SNDBUF, (const char*)&mode, sizeof(u_long)) != 0)
					assert_dbg();
			}
			else
			{
				cemuLog_logDebug(LogType::Force, "setsockopt(WU_SOL_SOCKET): Unsupported optname 0x{:08x}", optname);
			}
		}
		else if (level == WU_IPPROTO_TCP)
		{
			hostLevel = IPPROTO_TCP;
			if (optname == WU_TCP_NODELAY)
			{
				if (optlen != 4)
					assert_dbg();
				sint32 optvalLE = _swapEndianU32(*(uint32*)optval);
				u_long mode = optvalLE;  // 1 -> enable, 0 -> disable
				if (setsockopt(vs->s, IPPROTO_TCP, TCP_NODELAY, (const char*)&mode, sizeof(u_long)) != 0)
					assert_dbg();
			}
			else
			{
				cemuLog_logDebug(LogType::Force, "setsockopt(WU_IPPROTO_TCP): Unsupported optname 0x{:08x}", optname);
			}
		}
		else if (level == WU_IPPROTO_IP)
		{
			hostLevel = IPPROTO_IP;
			if (optname == WU_IP_MULTICAST_IF || optname == WU_IP_MULTICAST_TTL ||
				optname == WU_IP_MULTICAST_LOOP || optname == WU_IP_ADD_MEMBERSHIP ||
				optname == WU_IP_DROP_MEMBERSHIP)
			{
				cemuLog_logDebug(LogType::Socket, "todo: setsockopt() for multicast");
			}
			else if(optname == WU_IP_TTL || optname == WU_IP_TOS)
			{
				cemuLog_logDebug(LogType::Force, "setsockopt(WU_IPPROTO_IP): Unsupported optname 0x{:08x}", optname);
			}
			else
				assert_dbg();

		}
		else
			assert_dbg();
	}
	osLib_returnFromFunction(hCPU, 0);
}

void nsysnetExport_getsockopt(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "getsockopt({},0x{:x},0x{:05x},0x{:08x},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7]);
	ppcDefineParamS32(s, 0);
	ppcDefineParamS32(level, 1);
	ppcDefineParamS32(optname, 2);
	ppcDefineParamStr(optval, 3);
	ppcDefineParamMPTR(optlenMPTR, 4);

	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	if (vs == NULL)
	{
		cemu_assert_debug(false); // todo
		return;
	}

	sint32 r = 0;

	// translate level to host API
	sint32 hostLevel;
	if (level == WU_SOL_SOCKET)
	{
		hostLevel = SOL_SOCKET;
		// handle op
		if (optname == WU_SO_LASTERROR)
		{
			int optvalLE = 0;
			socklen_t optlenLE = 4;
			r = getsockopt(vs->s, hostLevel, SO_ERROR, (char*)&optvalLE, &optlenLE);
			r = _translateError(r, GETLASTERR);
			if (memory_readU32(optlenMPTR) != 4)
				assert_dbg();
			memory_writeU32(optlenMPTR, 4);
			if (optvalLE != 0)
				assert_dbg(); // todo -> Translate error code/status
			*(uint32*)optval = _swapEndianU32(optvalLE);
			// YouTube app uses this to check if the connection attempt was successful after non-blocking connect()
		}
		else if (optname == WU_SO_RCVBUF)
		{
			int optvalLE = 0;
			socklen_t optlenLE = 4;
			r = getsockopt(vs->s, hostLevel, SO_RCVBUF, (char*)&optvalLE, &optlenLE);
			r = _translateError(r, GETLASTERR);
			if (memory_readU32(optlenMPTR) != 4)
				assert_dbg();
			memory_writeU32(optlenMPTR, 4);
			*(uint32*)optval = _swapEndianU32(optvalLE);
			// used by Amazon Video app when a video starts playing
		}
		else if (optname == WU_SO_SNDBUF)
		{
			int optvalLE = 0;
			socklen_t optlenLE = 4;
			r = getsockopt(vs->s, hostLevel, SO_SNDBUF, (char*)&optvalLE, &optlenLE);
			r = _translateError(r, GETLASTERR);
			if (memory_readU32(optlenMPTR) != 4)
				assert_dbg();
			memory_writeU32(optlenMPTR, 4);
			*(uint32*)optval = _swapEndianU32(optvalLE);
			// used by Lost Reavers after some loading screens
		}
		else if (optname == WU_SO_TYPE)
		{
			if (memory_readU32(optlenMPTR) != 4)
				assert_dbg();
			int optvalLE = 0;
			socklen_t optlenLE = 4;
			memory_writeU32(optlenMPTR, 4);
			*(uint32*)optval = _swapEndianU32(vs->type);
			r = WU_SO_SUCCESS;
		}
        else if (optname == WU_SO_NONBLOCK)
        {
            if (memory_readU32(optlenMPTR) != 4)
                assert_dbg();
            int optvalLE = 0;
            socklen_t optlenLE = 4;
            memory_writeU32(optlenMPTR, 4);
            *(uint32*)optval = _swapEndianU32(vs->isNonBlocking ? 1 : 0);
            r = WU_SO_SUCCESS;
        }
		else
		{
			cemuLog_logDebug(LogType::Force, "getsockopt(WU_SOL_SOCKET): Unsupported optname 0x{:08x}", optname);
		}
	}
	else
	{
		cemuLog_logDebug(LogType::Force, "getsockopt(): Unsupported level 0x{:08x}", level);
	}

	osLib_returnFromFunction(hCPU, r);
}

void nsysnetExport_inet_aton(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStr(ip, 0);
	ppcDefineParamStructPtr(addr, wu_in_addr, 1);
	cemuLog_log(LogType::Socket, "inet_aton(\"{}\",0x{:08x})", ip, hCPU->gpr[4]);

	// _parse_ipad -> todo
	sint32 d0, d1, d2, d3;
	if (sscanf(ip, "%d.%d.%d.%d", &d0, &d1, &d2, &d3) != 4)
	{
		cemu_assert_debug(false);
		osLib_returnFromFunction(hCPU,  0); // todo - return correct error code
		return;
	}

	if (d0 < 0 || d0 > 255 ||
		d1 < 0 || d1 > 255 ||
		d2 < 0 || d2 > 255 ||
		d3 < 0 || d3 > 255)
	{
		cemu_assert_debug(false);
		osLib_returnFromFunction(hCPU, 0);
		return;
	}

	addr->wu_s_addr = _swapEndianU32((d0 << 24) | (d1 << 16) | (d2 << 8) | (d3 << 0));

	osLib_returnFromFunction(hCPU, 1); // 0 -> error, 1 -> success
}

void nsysnetExport_inet_pton(PPCInterpreter_t* hCPU)
{
	ppcDefineParamS32(af, 0);
	ppcDefineParamStr(ip, 1);
	ppcDefineParamStructPtr(addr, wu_in_addr, 2);
	
	if (af != 2)
	{
		cemuLog_log(LogType::Force, "inet_pton() only supports AF_INET");
		osLib_returnFromFunction(hCPU, 0);
		return;
	}

	sint32 d0, d1, d2, d3;
	bool invalidIp = false;
	if (sscanf(ip, "%d.%d.%d.%d", &d0, &d1, &d2, &d3) != 4)
		invalidIp = true;
	if (d0 < 0 || d0 > 255)
		invalidIp = true;
	if (d1 < 0 || d1 > 255)
		invalidIp = true;
	if (d2 < 0 || d2 > 255)
		invalidIp = true;
	if (d3 < 0 || d3 > 255)
		invalidIp = true;
	if (invalidIp)
	{
		cemuLog_log(LogType::Socket, "inet_pton({}, \"{}\", 0x{:08x}) -> Invalid ip", af, ip, hCPU->gpr[5]);
        _setSockError(WU_SO_EAFNOSUPPORT);
		osLib_returnFromFunction(hCPU, 0); // 0 -> invalid address
		return;
	}

	addr->wu_s_addr = _swapEndianU32((d0 << 24) | (d1 << 16) | (d2 << 8) | (d3 << 0));
	cemuLog_log(LogType::Socket, "inet_pton({}, \"{}\", 0x{:08x}) -> Ok", af, ip, hCPU->gpr[5]);

	osLib_returnFromFunction(hCPU, 1); // 1 -> success
}

namespace nsysnet
{
    const char* inet_ntop(sint32 af, const void* src, char* dst, uint32 size)
    {
        if( af != WU_AF_INET)
        {
            // set error
            _setSockError(WU_SO_EAFNOSUPPORT);
            return nullptr;
        }
        const uint8* ip = (const uint8*)src;
        char buf[32];
        sprintf(buf, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        size_t bufLen = strlen(buf);
        if( (bufLen+1) > size )
        {
            // set error
            _setSockError(WU_SO_EAFNOSUPPORT);
            return nullptr;
        }
        strcpy(dst, buf);
        cemuLog_log(LogType::Socket, "inet_ntop -> {}", buf);
        return dst;
    }
}

MEMPTR<char> _ntoa_tempString = nullptr;

void nsysnetExport_inet_ntoa(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStructPtr(addr, wu_in_addr, 0);
	cemuLog_log(LogType::Socket, "inet_ntoa(0x{:08x})", hCPU->gpr[3]);

	if (_ntoa_tempString == nullptr)
		_ntoa_tempString = (char*)memory_getPointerFromVirtualOffset(OSAllocFromSystem(64, 4));

	sprintf(_ntoa_tempString.GetPtr(), "%d.%d.%d.%d", ((uint8*)addr)[0], ((uint8*)addr)[1], ((uint8*)addr)[2], ((uint8*)addr)[3]);

	osLib_returnFromFunction(hCPU, _ntoa_tempString.GetMPTR());
}

void nsysnetExport_htons(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "htons(0x{:04x})", hCPU->gpr[3]);
	ppcDefineParamU32(v, 0);
	osLib_returnFromFunction(hCPU, v); // return value as-is
}

void nsysnetExport_htonl(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "htonl(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamU32(v, 0);
	osLib_returnFromFunction(hCPU, v); // return value as-is
}

void nsysnetExport_ntohs(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "ntohs(0x{:04x})", hCPU->gpr[3]);
	ppcDefineParamU32(v, 0);
	osLib_returnFromFunction(hCPU, v); // return value as-is
}

void nsysnetExport_ntohl(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "ntohl(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamU32(v, 0);
	osLib_returnFromFunction(hCPU, v); // return value as-is
}

void nsysnetExport_bind(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "bind({},0x{:08x},{})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
	ppcDefineParamS32(s, 0);
	ppcDefineParamStructPtr(addr, struct wu_sockaddr, 1);
	ppcDefineParamS32(len, 2);


	if (len != sizeof(struct wu_sockaddr))
		assert_dbg();

	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	sint32 r = 0;
	if (vs == NULL)
	{
		assert_dbg();
	}
	else
	{
		if (sizeof(sockaddr) != 16)
			assert_dbg();
		sockaddr hostAddr;
		hostAddr.sa_family = _swapEndianU16(addr->sa_family);
		memcpy(hostAddr.sa_data, addr->sa_data, 14);
		sint32 hr = bind(vs->s, &hostAddr, sizeof(sockaddr));
		r = _translateError(hr, GETLASTERR);


		cemuLog_log(LogType::Socket, "bind address: {}.{}.{}.{}:{} result: {}", addr->sa_data[2], addr->sa_data[3], addr->sa_data[4], addr->sa_data[5], _swapEndianU16(*(uint16*)addr->sa_data), hr);

	}

	osLib_returnFromFunction(hCPU, r);
}

void nsysnetExport_listen(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "listen({},{})", hCPU->gpr[3], hCPU->gpr[4]);
	ppcDefineParamS32(s, 0);
	ppcDefineParamS32(queueSize, 1);

	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	sint32 r = 0;
	if (vs == NULL)
	{
		assert_dbg();
	}
	else
	{
		sint32 hr = listen(vs->s, queueSize);
		if (hr != 0)
			r = -1;
		// todo: Set proper coreinit errno (via _setSockError)
	}

	osLib_returnFromFunction(hCPU, r);
}

void nsysnetExport_accept(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "accept({},0x{:08x},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
	ppcDefineParamS32(s, 0);
	ppcDefineParamStructPtr(addr, struct wu_sockaddr, 1);
	ppcDefineParamMPTR(lenMPTR, 2);

	sint32 r = 0;
	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	if (vs == NULL)
	{
		assert_dbg();
		// todo
		return;
	}

	if (memory_readU32(lenMPTR) != 16)
	{
		cemuLog_log(LogType::Force, "invalid sockaddr len in accept()");
		cemu_assert_debug(false);
		osLib_returnFromFunction(hCPU, 0);
		return;
	}

	if (vs->isNonBlocking)
	{
		sockaddr hostAddr;
		socklen_t hostLen = sizeof(sockaddr);
		SOCKET hr = accept(vs->s, &hostAddr, &hostLen);
		if (hr != SOCKET_ERROR)
		{
			r = nsysnet_createVirtualSocketFromExistingSocket(hr);
			_setSockError(WU_SO_SUCCESS);
		}
		else
		{
			r = _translateError((sint32)hr, (sint32)GETLASTERR, _ERROR_MODE_ACCEPT);
		}
		sockaddr_host2guest(&hostAddr, addr);
	}
	else
	{
		// blocking accept is not supported yet
		cemuLog_log(LogType::Force, "blocking accept() not supported");
		cemu_assert_debug(false);
	}

	osLib_returnFromFunction(hCPU, r);
}

void nsysnetExport_connect(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "connect({},0x{:08x},{})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
	ppcDefineParamS32(s, 0);
	ppcDefineParamStructPtr(addr, struct wu_sockaddr, 1);
	ppcDefineParamS32(len, 2);

	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	sint32 r = 0;
	if (vs == NULL)
	{
		assert_dbg();
		return;
	}

	if (sizeof(sockaddr) != 16)
		assert_dbg();
	sockaddr hostAddr;
	hostAddr.sa_family = _swapEndianU16(addr->sa_family);
	memcpy(hostAddr.sa_data, addr->sa_data, 14);
	sint32 hr = connect(vs->s, &hostAddr, sizeof(sockaddr));
	cemuLog_log(LogType::Force, "Attempt connect to {}.{}.{}.{}:{}", (sint32)(uint8)hostAddr.sa_data[2], (sint32)(uint8)hostAddr.sa_data[3], (sint32)(uint8)hostAddr.sa_data[4], (sint32)(uint8)hostAddr.sa_data[5], _swapEndianU16(*(uint16*)hostAddr.sa_data+0));

	r = _translateError(hr, GETLASTERR, _ERROR_MODE_CONNECT);

	osLib_returnFromFunction(hCPU, r);
}

void _setSocketSendRecvNonBlockingMode(SOCKET s, bool isNonBlocking)
{
	u_long mode = isNonBlocking ? 1 : 0;
	sint32 r = _socket_nonblock(s,  mode);
}

void nsysnetExport_send(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "send({},0x{:08x},{},0x{:x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6]);
	ppcDefineParamS32(s, 0);
	ppcDefineParamStr(msg, 1);
	ppcDefineParamS32(len, 2);
	ppcDefineParamS32(flags, 3);

	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	sint32 r = 0;
	if (vs == NULL)
	{
		assert_dbg();
		return;
	}
	int hostFlags = 0;
	bool requestIsNonBlocking = (flags&WU_MSG_DONTWAIT) != 0;
	flags &= ~WU_MSG_DONTWAIT;
	if (requestIsNonBlocking != vs->isNonBlocking && vs->isNonBlocking == false)
		assert_dbg();

	if (flags)
		assert_dbg();

	sint32 hr = send(vs->s, msg, len, hostFlags);
	cemuLog_log(LogType::Socket, "Sent {} bytes", hr);
	_translateError(hr <= 0 ? -1 : 0, GETLASTERR);
	r = hr;

	osLib_returnFromFunction(hCPU, r);
}

void nsysnetExport_recv(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "recv({},0x{:08x},{},0x{:x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6]);
	ppcDefineParamS32(s, 0);
	ppcDefineParamStr(msg, 1);
	ppcDefineParamS32(len, 2);
	ppcDefineParamS32(flags, 3);

	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	sint32 r = 0;
	if (vs == NULL)
	{
		assert_dbg();
		return;
	}
	int hostFlags = 0;
	bool requestIsNonBlocking = (flags&WU_MSG_DONTWAIT) != 0;
	flags &= ~WU_MSG_DONTWAIT;
	if (flags&WU_MSG_PEEK)
	{
		hostFlags |= MSG_PEEK;
		flags &= ~WU_MSG_PEEK;
	}

	if (vs->isNonBlocking)
		requestIsNonBlocking = vs->isNonBlocking; // non-blocking sockets always operate in MSG_DONTWAIT mode?

	if (flags)
		assert_dbg();
	if (requestIsNonBlocking != vs->isNonBlocking)
		_setSocketSendRecvNonBlockingMode(vs->s, requestIsNonBlocking);
	// if blocking, yield thread until there is an error or at least 1 byte was received
	if (requestIsNonBlocking == false)
	{
		_setSocketSendRecvNonBlockingMode(vs->s, true);
		while (true)
		{
			char tempBuffer[1];
			sint32 tr = recv(vs->s, tempBuffer, 1, MSG_PEEK);
			if (tr == 1)
				break;
			if (tr == 0)
				break; // connection closed
			if (tr < 0 && GETLASTERR != WSAEWOULDBLOCK)
				break;
			// yield thread
			coreinit::OSSleepTicks(coreinit::EspressoTime::GetTimerClock() / 5000); // let thread wait 0.2ms to give other threads CPU time
			// todo - eventually we should find a way to asynchronously signal recv instead of busy-looping here
		}
		_setSocketSendRecvNonBlockingMode(vs->s, requestIsNonBlocking);
	}
	// receive
	sint32 hr = recv(vs->s, msg, len, hostFlags);
	_translateError(hr <= 0 ? -1 : 0, GETLASTERR);
	if (requestIsNonBlocking != vs->isNonBlocking)
		_setSocketSendRecvNonBlockingMode(vs->s, vs->isNonBlocking);
	cemuLog_log(LogType::Socket, "Received {} bytes", hr);
	r = hr;

	osLib_returnFromFunction(hCPU, r);
}

struct wu_timeval 
{
	uint32 tv_sec;
	uint32 tv_usec;
};

void _translateFDSet(fd_set* hostSet, struct wu_fd_set* fdset, sint32 nfds, int *hostnfds)
{
	FD_ZERO(hostSet);
	if (fdset == NULL)
		return;

#if BOOST_OS_UNIX
	int maxfd;
	if(hostnfds)
		maxfd = *hostnfds;
	else
		maxfd = -1;
#endif

	uint32 mask = fdset->mask;
	for (sint32 i = 0; i < nfds; i++)
	{
		if( ((mask>>i)&1) == 0 )
			continue;
		sint32 socketHandle = i;
		virtualSocket_t* vs = nsysnet_getVirtualSocketObject(socketHandle);
		if(vs == NULL)
			continue; // socket invalid

#if BOOST_OS_UNIX
		if(vs->s > maxfd)
			maxfd = vs->s;
#endif

		FD_SET(vs->s, hostSet);
	}

#if BOOST_OS_UNIX
	if(hostnfds)
		*hostnfds = maxfd;
#endif
}

void _translateFDSetRev(struct wu_fd_set* fdset, fd_set* hostSet, sint32 nfds)
{
	if (fdset == NULL)
		return;
	uint32 mask = _swapEndianU32(0);
#if BOOST_OS_WINDOWS
	for (sint32 i = 0; i < (sint32)hostSet->fd_count; i++)
	{
		sint32 virtualSocketHandle = nsysnet_getVirtualSocketHandleFromHostHandle(hostSet->fd_array[i]);
		if (virtualSocketHandle < 0)
			cemu_assert_debug(false);
		mask |= (1<<virtualSocketHandle);
	}
#else
	for (sint32 i = 0; i < WU_SOCKET_LIMIT; i++)
	{
		if (virtualSocketTable[i] && virtualSocketTable[i]->s && FD_ISSET(virtualSocketTable[i]->s, hostSet))
			mask |= (1 << virtualSocketTable[i]->handle);
	}
#endif
	fdset->mask = mask;

}

void nsysnetExport_select(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "select({},0x{:08x},0x{:08x},0x{:08x},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7]);
	ppcDefineParamS32(nfds, 0);
	ppcDefineParamStructPtr(readfds, struct wu_fd_set, 1);
	ppcDefineParamStructPtr(writefds, struct wu_fd_set, 2);
	ppcDefineParamStructPtr(exceptfds, struct wu_fd_set, 3);
	ppcDefineParamStructPtr(timeOut, struct wu_timeval, 4);

	//cemuLog_log(LogType::Socket, "rm {:08x} wm {:08x} em {:08x}", readfds ? _swapEndianU32(readfds->mask) : 0, writefds ? _swapEndianU32(writefds->mask) : 0, exceptfds ? _swapEndianU32(exceptfds->mask) : 0);

	sint32 r = 0;

	// translate fd_set
	fd_set _readfds, _writefds, _exceptfds;

	// handle empty select
	if ((readfds == NULL || readfds->mask == 0) && (writefds == NULL || writefds->mask == 0) && (exceptfds == NULL || exceptfds->mask == 0))
	{
		if (timeOut == NULL || (timeOut->tv_sec == 0 && timeOut->tv_usec == 0))
		{
			// return immediately
			cemuLog_log(LogType::Socket, "select returned immediately because of empty fdsets without timeout");
			osLib_returnFromFunction(hCPU, 0);
			return;
		}
		else
		{
			//// empty select with timeout is not allowed
			//_setSockError(WU_SO_EINVAL);
			//osLib_returnFromFunction(hCPU, -1);
			//cemuLog_log(LogType::Socket, "select returned SO_EINVAL because of empty fdsets with timeout");

			// when fd sets are empty but timeout is set, then just wait and do nothing?
			// Lost Reavers seems to expect this case to return 0 (it hardcodes empty fd sets and timeout comes from curl_multi_timeout)

			timeval tv;
			tv.tv_sec = timeOut->tv_sec;
			tv.tv_usec = timeOut->tv_usec;
			select(0, nullptr, nullptr, nullptr, &tv);
			cemuLog_log(LogType::Socket, "select returned 0 because of empty fdsets with timeout");
			osLib_returnFromFunction(hCPU, 0);
			
			return;
		}
	}



	timeval tv = { 0 };

	if (timeOut == NULL)
	{
		// return immediately
		cemuLog_log(LogType::Socket, "select returned immediately because of null timeout");
		osLib_returnFromFunction(hCPU, 0);
		return;
	}

	uint64 msTimeout = (_swapEndianU32(timeOut->tv_usec) / 1000) + (_swapEndianU32(timeOut->tv_sec) * 1000);
	uint32 startTime = GetTickCount();
	while (true)
	{
		int hostnfds = -1;
		_translateFDSet(&_readfds, readfds, nfds, &hostnfds);
		_translateFDSet(&_writefds, writefds, nfds, &hostnfds);
		_translateFDSet(&_exceptfds, exceptfds, nfds, &hostnfds);
		r = select(hostnfds + 1, readfds ? &_readfds : NULL, writefds ? &_writefds : NULL, exceptfds ? &_exceptfds : NULL, &tv);
		if (r < 0)
		{
			cemuLog_logDebug(LogType::Force, "select() failed");
			// timeout
			_translateError(r, GETLASTERR);
			//_setSockError(WU_SO_SUCCESS);
			// in case of error, clear all FD sets (?)
			if (readfds)
				readfds->mask = 0;
			if (writefds)
				writefds->mask = 0;
			if (exceptfds)
				exceptfds->mask = 0;
			break;
		}
		else if (r == 0)
		{
			// check for timeout
			if ((GetTickCount() - startTime) >= msTimeout )
			{
				// timeout
				_setSockError(WU_SO_SUCCESS);
				r = 0;
				// in case of timeout, clear all FD sets
				if (readfds)
					readfds->mask = 0;
				if (writefds)
					writefds->mask = 0;
				if (exceptfds)
					exceptfds->mask = 0;
				break;
			}
			// yield thread
			PPCCore_switchToScheduler();
		}
		else
		{
			// cemuLog_log(LogType::Socket, "select returned {}. Read {} Write {} Except {}", r, _readfds.fd_count, _writefds.fd_count, _exceptfds.fd_count);
			cemuLog_log(LogType::Socket, "select returned {}.", r);

			_translateFDSetRev(readfds, &_readfds, nfds);
			_translateFDSetRev(writefds, &_writefds, nfds);
			_translateFDSetRev(exceptfds, &_exceptfds, nfds);
			break;
		}
	}
	//cemuLog_log(LogType::Force, "selectEndTime {}", timeGetTime());


	//extern int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	//	struct timeval *timeout);

	cemuLog_log(LogType::Socket, "select returned {}", r);
	osLib_returnFromFunction(hCPU, r);
}

void nsysnetExport_getsockname(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "getsockname({},0x{:08x},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);

	ppcDefineParamS32(s, 0);
	ppcDefineParamStructPtr(addr, struct wu_sockaddr, 1);
	ppcDefineParamStructPtr(lenPtr, uint32, 2);
	sint32 r = 0;
	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	if (vs == NULL)
	{
		assert_dbg();
	}
	else
	{
		struct sockaddr hostAddr;
		socklen_t hostLen = sizeof(struct sockaddr);
		sint32 hr = getsockname(vs->s, &hostAddr, &hostLen);
		if (hr == 0)
		{
			addr->sa_family = _swapEndianU16(hostAddr.sa_family);
			memcpy(addr->sa_data, hostAddr.sa_data, 14);
		}
		else
		{
			assert_dbg();
		}
		//sint32 hr = listen(vs->s, queueSize);
		//if (hr != 0)
		//	r = -1;
		//// todo: Set proper coreinit errno (via _setSockError)
	}

	osLib_returnFromFunction(hCPU, r);
}

void nsysnetExport_getpeername(PPCInterpreter_t* hCPU)
{
	ppcDefineParamS32(s, 0);
	ppcDefineParamStructPtr(name, struct wu_sockaddr, 1);
	ppcDefineParamU32BEPtr(nameLen, 2);

	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	if (vs == NULL)
	{
		// todo: Return correct error
		osLib_returnFromFunction(hCPU, -1);
		return;
	}

	sockaddr saddr;
	socklen_t saddrLen = sizeof(sockaddr);

	if (*nameLen < (uint32be)16)
		assert_dbg();

	sint32 r = getpeername(vs->s, &saddr, &saddrLen);
	r = _translateError(r, GETLASTERR);

	name->sa_family = _swapEndianU16(saddr.sa_family);
	memcpy(name->sa_data, saddr.sa_data, 14);
	*nameLen = 16;

	osLib_returnFromFunction(hCPU, r);
}

typedef struct
{
	MPTR h_name;
	MPTR h_aliases;
	sint32 h_addrType;
	sint32 h_length;
	MPTR h_addr_list;
}wu_hostent;

MPTR _allocString(char* str)
{
	sint32 len = (sint32)strlen(str);
	MPTR strMPTR = coreinit_allocFromSysArea(len+1, 4);
	strcpy((char*)memory_getPointerFromVirtualOffset(strMPTR), str);
	return strMPTR;
}

void nsysnetExport_gethostbyname(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStr(name, 0);

	cemuLog_log(LogType::Socket, "gethostbyname(\"{}\")", name);

	hostent* he = gethostbyname(name);
	if (he == NULL)
	{
		osLib_returnFromFunction(hCPU, MPTR_NULL);
		return;
	}


	MPTR hostentMPTR = coreinit_allocFromSysArea(sizeof(wu_hostent)*1, 4);
	MPTR hostentAddrListMPTR = coreinit_allocFromSysArea(sizeof(MPTR) * 2, 4);
	MPTR hostentAddrListEntriesMPTR = coreinit_allocFromSysArea(sizeof(wu_in_addr) * 1, 4);

	wu_hostent* wuHostent = (wu_hostent*)memory_getPointerFromVirtualOffset(hostentMPTR);
	MPTR* addrList = (MPTR*)memory_getPointerFromVirtualOffset(hostentAddrListMPTR);

	wuHostent->h_addrType = _swapEndianU32((uint32)he->h_addrtype);
	wuHostent->h_length = _swapEndianU32((uint32)he->h_length);

	wuHostent->h_name = _swapEndianU32(_allocString(he->h_name));
	wuHostent->h_addr_list = _swapEndianU32(hostentAddrListMPTR);

	//memory_writeU32(hostentAddrListEntriesMPTR, _swapEndianU32(*(uint32*)he->h_addr_list[0]));
 
	wu_in_addr* addrListEntries = (wu_in_addr*)memory_getPointerFromVirtualOffset(hostentAddrListEntriesMPTR);
	addrListEntries->wu_s_addr = *(uint32*)he->h_addr_list[0]; // address is already in network (big-endian) order

	memory_writeU32(hostentAddrListMPTR + 4 * 0, hostentAddrListEntriesMPTR);
	memory_writeU32(hostentAddrListMPTR + 4 * 1, MPTR_NULL);

	osLib_returnFromFunction(hCPU, hostentMPTR);
	return;
}

SysAllocator<wu_hostent> _staticHostent;
SysAllocator<char, 256> _staticHostentName;
SysAllocator<MPTR, 32> _staticHostentPtrList;
SysAllocator<wu_in_addr> _staticHostentEntries;

void nsysnetExport_gethostbyaddr(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStr(addr, 0);
	ppcDefineParamS32(len, 1);
	ppcDefineParamS32(type, 2);

	sint32 maxNumEntries = 31;

	cemuLog_log(LogType::Socket, "gethostbyaddr(\"{}\", {}, {})", addr, len, type);

	hostent* he = gethostbyaddr(addr, len, type);
	if (he == nullptr)
	{
		cemuLog_log(LogType::Socket, "gethostbyaddr(\"{}\", {}, {}) failed", addr, len, type);
		osLib_returnFromFunction(hCPU, MPTR_NULL);
		return;
	}

#ifdef CEMU_DEBUG_ASSERT
	if (he->h_addrtype != AF_INET)
		assert_dbg();
	if (he->h_length != sizeof(in_addr))
		assert_dbg();
#endif

	wu_hostent* wuHostent = _staticHostent.GetPtr();
	// setup wuHostent->h_name	
	wuHostent->h_name = _swapEndianU32(_staticHostentName.GetMPTR());
	if (he->h_name && strlen(he->h_name) < 255)
	{
		strcpy(_staticHostentName.GetPtr(), he->h_name);
	}
	else
	{
		cemuLog_log(LogType::Force, "he->h_name not set or name too long");
		strcpy(_staticHostentName.GetPtr(), "");
	}
	// setup wuHostent address list
	wuHostent->h_addrType = _swapEndianU32(WU_AF_INET);
	wuHostent->h_addr_list = _swapEndianU32(_staticHostentPtrList.GetMPTR());
	wuHostent->h_length = _swapEndianU32(sizeof(wu_in_addr));
	for (sint32 i = 0; i < maxNumEntries; i++)
	{
		if (he->h_addr_list[i] == nullptr)
		{
			_staticHostentPtrList.GetPtr()[i] = MPTR_NULL;
			break;
		}
		memcpy(_staticHostentEntries.GetPtr() + i, he->h_addr_list[i], sizeof(in_addr));
		_staticHostentPtrList.GetPtr()[i] = _swapEndianU32(memory_getVirtualOffsetFromPointer(_staticHostentEntries.GetPtr() + i));
	}
	_staticHostentPtrList.GetPtr()[31] = MPTR_NULL;
	// aliases are ignored for now
	wuHostent->h_aliases = MPTR_NULL;

	osLib_returnFromFunction(hCPU, _staticHostent.GetMPTR());
	return;
}

void nsysnetExport_getaddrinfo(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStr(nodeName, 0);
	ppcDefineParamStr(serviceName, 1);
	ppcDefineParamStructPtr(hints, struct wu_addrinfo, 2);
	ppcDefineParamMPTR(results, 3);

	cemuLog_log(LogType::Socket, "getaddrinfo(\"{}\",0x{:08x},0x{:08x},0x{:08x})", nodeName, hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6]);
	
	sint32 r = 0;

	// todo1: This is really slow. Make it asynchronous
	// todo2: Should this set the socket last error code? 

	struct addrinfo hint = { 0 };
	if (hints)
	{
		hint.ai_family = _swapEndianU32(hints->ai_family);
		hint.ai_socktype = _swapEndianU32(hints->ai_socktype);
		hint.ai_flags = _swapEndianU32(hints->ai_flags);
		hint.ai_protocol = _swapEndianU32(hints->ai_protocol);
	}
	else
	{
		hint.ai_family = 0;
		hint.ai_socktype = 0;
		hint.ai_flags = 0;
		hint.ai_protocol = 0;
	}
	struct addrinfo* result;
	sint32 hr = getaddrinfo(nodeName, serviceName, &hint, &result);
	if (hr != 0)
	{
		cemuLog_log(LogType::Socket, "getaddrinfo failed with error {}", hr);
		switch (hr)
		{
		case WSAHOST_NOT_FOUND:
			r = WU_SO_ECONNRESET; // todo - verify error code
			break;
		default:
			// unhandled error
			cemu_assert_debug(false);
			cemuLog_log(LogType::Socket, "getaddrinfo unhandled error code");
			r = 1;
			break;
		}
	}
	else
	{
		// count how many results there are
		sint32 resultCount = 0;
		struct addrinfo* currentAddrInfo = result;
		while (currentAddrInfo)
		{
			resultCount++;
			currentAddrInfo = currentAddrInfo->ai_next;
		}
		if (resultCount == 0)
		{
			cemu_assert_debug(false);
		}
		// allocate entries
		MPTR addrInfoMPTR = coreinit_allocFromSysArea(sizeof(wu_addrinfo)*resultCount + sizeof(wu_sockaddr)*resultCount, 4);
		MPTR addrInfoSockAddrMPTR = addrInfoMPTR + sizeof(wu_addrinfo)*resultCount;
		wu_addrinfo* wuAddrInfo = (wu_addrinfo*)memory_getPointerFromVirtualOffset(addrInfoMPTR);
		// fill entries
		currentAddrInfo = result;
		sint32 entryIndex = 0;
		wu_addrinfo* previousWuAddrInfo = NULL;
		while (currentAddrInfo)
		{
			if (currentAddrInfo->ai_addrlen != 16)
			{ 
				// skip this entry (IPv6)
				currentAddrInfo = currentAddrInfo->ai_next;
				continue;
			}

			memset(&wuAddrInfo[entryIndex], 0, sizeof(wu_addrinfo));
			// setup pointers
			wuAddrInfo[entryIndex].ai_addr = _swapEndianU32(addrInfoSockAddrMPTR + sizeof(wu_sockaddr)*entryIndex);
			wuAddrInfo[entryIndex].ai_next = MPTR_NULL;
			wuAddrInfo[entryIndex].ai_canonname = _swapEndianU32(MPTR_NULL);
			// set ai_next for previous element
			if (previousWuAddrInfo)
			{
				previousWuAddrInfo->ai_next = _swapEndianU32(memory_getVirtualOffsetFromPointer(&wuAddrInfo[entryIndex]));
			}
			previousWuAddrInfo = &wuAddrInfo[entryIndex];
			// fill addrinfo struct
			wuAddrInfo[entryIndex].ai_addrlen = _swapEndianU32((uint32)currentAddrInfo->ai_addrlen);
			//wuAddrInfo[entryIndex].ai_canonname; todo
			wuAddrInfo[entryIndex].ai_family = _swapEndianU32(currentAddrInfo->ai_family);
			wuAddrInfo[entryIndex].ai_flags = _swapEndianU32(currentAddrInfo->ai_flags); // todo: These flags might need to be translated
			wuAddrInfo[entryIndex].ai_protocol = _swapEndianU32(currentAddrInfo->ai_protocol);
			wuAddrInfo[entryIndex].ai_socktype = _swapEndianU32(currentAddrInfo->ai_socktype);

			// fill ai_addr
			wu_sockaddr* sockAddr = (wu_sockaddr*)memory_getPointerFromVirtualOffset(_swapEndianU32(wuAddrInfo[entryIndex].ai_addr));
			sockAddr->sa_family = _swapEndianU16(currentAddrInfo->ai_addr->sa_family);
			memcpy(sockAddr->sa_data, currentAddrInfo->ai_addr->sa_data, 14);

			// next
			entryIndex++;
			currentAddrInfo = currentAddrInfo->ai_next;
		}
		if (entryIndex == 0)
			assert_dbg();
		// set results
		memory_writeU32(results, addrInfoMPTR);
	}

	osLib_returnFromFunction(hCPU, r);
}

void nsysnetExport_recvfrom(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "recvfrom({},0x{:08x},{},0x{:x},0x{:x},0x{:x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7], hCPU->gpr[8]);
	ppcDefineParamS32(s, 0);
	ppcDefineParamStr(msg, 1);
	ppcDefineParamS32(len, 2);
	ppcDefineParamS32(flags, 3);
	ppcDefineParamStructPtr(fromAddr, wu_sockaddr, 4);
	ppcDefineParamU32BEPtr(fromLen, 5);


	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	sint32 r = 0;
	if (vs == NULL)
	{
		assert_dbg();
		return;
	}

	int hostFlags = 0;
	bool requestIsNonBlocking = (flags&WU_MSG_DONTWAIT) != 0;
	flags &= ~WU_MSG_DONTWAIT;
	if (flags&WU_MSG_PEEK)
	{
		assert_dbg();
		hostFlags |= MSG_PEEK;
		flags &= ~WU_MSG_PEEK;
	}
	if (vs->isNonBlocking)
		requestIsNonBlocking = vs->isNonBlocking;

	sockaddr fromAddrHost;
	socklen_t fromLenHost = sizeof(fromAddrHost);
	sint32 wsaError = 0;

	while( true )
	{
		// is socket recv shutdown?
		if (vs->isShutdownRecv)
		{
			// return with error
			_setSockError(WU_SO_ESHUTDOWN);
			osLib_returnFromFunction(hCPU, -1);
			return;
		}
		// use select to check for exceptions and read data
		fd_set fd_read;
		fd_set fd_exceptions;
		FD_ZERO(&fd_read);
		FD_ZERO(&fd_exceptions);
		FD_SET(vs->s, &fd_read);
		FD_SET(vs->s, &fd_exceptions);
		timeval t;
		t.tv_sec = 0;
		t.tv_usec = 0;
		int nfds = 0;
#if BOOST_OS_UNIX
		nfds = vs->s + 1;
#endif
		sint32 count = select(nfds, &fd_read, NULL, &fd_exceptions, &t);
		if (count > 0)
		{
			if (FD_ISSET(vs->s, &fd_exceptions))
			{
				assert_dbg(); // exception
			}
			if (FD_ISSET(vs->s, &fd_read))
			{
				// data available
				r = recvfrom(vs->s, msg, len, hostFlags, &fromAddrHost, &fromLenHost);
				wsaError = GETLASTERR;
				if (r < 0)
					cemu_assert_debug(false);
				cemuLog_logDebug(LogType::Force, "recvfrom returned {} bytes", r);

				// fromAddr and fromLen can be NULL
				if (fromAddr && fromLen) {
					*fromLen = fromLenHost;
					fromAddr->sa_family = _swapEndianU16(fromAddrHost.sa_family);
					memcpy(fromAddr->sa_data, fromAddrHost.sa_data, 14);
				}

				_setSockError(0);
				osLib_returnFromFunction(hCPU, r);
				return;
			}
		}
		// nothing to do
		if (requestIsNonBlocking)
		{
			// return with error
			_setSockError(WU_SO_EWOULDBLOCK);
			osLib_returnFromFunction(hCPU, -1);
			return;
		}
		else
		{
			// pause for a while and check again later
			coreinit::OSSleepTicks(ESPRESSO_CORE_CLOCK / 1000); // pause for 1ms
			PPCCore_switchToScheduler();
		}
	}
	assert_dbg(); // should no longer be reached

	if (requestIsNonBlocking == false)
	{
		// blocking
		_setSocketSendRecvNonBlockingMode(vs->s, true);
		while (true)
		{
			r = recvfrom(vs->s, msg, len, hostFlags, &fromAddrHost, &fromLenHost);
			wsaError = GETLASTERR;
			if (r < 0)
			{
				if (wsaError != WSAEWOULDBLOCK)
					break;
				coreinit::OSSleepTicks(ESPRESSO_CORE_CLOCK/100); // pause for 10ms
				PPCCore_switchToScheduler();
				continue;
			}
			assert_dbg();
		}
		_setSocketSendRecvNonBlockingMode(vs->s, vs->isNonBlocking);
	}
	else
	{
		// non blocking
		assert_dbg();
	}

	// fromAddr and fromLen can be NULL
	if (fromAddr && fromLen) {
		*fromLen = fromLenHost;
		fromAddr->sa_family = _swapEndianU16(fromAddrHost.sa_family);
		memcpy(fromAddr->sa_data, fromAddrHost.sa_data, 14);
	}

	_translateError(r <= 0 ? -1 : 0, wsaError);

	osLib_returnFromFunction(hCPU, r);
}


void nsysnetExport_recvfrom_ex(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "recvfrom_ex({},0x{:08x},{},0x{:x},0x{:08x},0x{:08x},0x{:08x},{})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7], hCPU->gpr[8], hCPU->gpr[9], hCPU->gpr[10]);
	ppcDefineParamS32(s, 0);
	ppcDefineParamStr(msg, 1);
	ppcDefineParamS32(len, 2);
	ppcDefineParamS32(flags, 3);
	ppcDefineParamStructPtr(fromAddr, wu_sockaddr, 4);
	ppcDefineParamU32BEPtr(fromLen, 5);
	ppcDefineParamUStr(extraData, 6);
	ppcDefineParamS32(extraDataLen, 7);

	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	sint32 r = 0;
	if (vs == NULL)
	{
		cemu_assert_debug(false);
		return;
	}

	int hostFlags = 0;
	bool requestIsNonBlocking = (flags&WU_MSG_DONTWAIT) != 0;
	flags &= ~WU_MSG_DONTWAIT;
	if (flags&WU_MSG_PEEK)
	{
		cemu_assert_debug(false);
		hostFlags |= MSG_PEEK;
		flags &= ~WU_MSG_PEEK;
	}
	if (flags & 0x40)
	{
		// read TTL
		if (extraDataLen != 1)
		{
			cemu_assert_debug(false);
		}
		extraData[0] = 0x5; // we currently always return 5 as TTL
		flags &= ~0x40;
	}

	if (vs->isNonBlocking)
		requestIsNonBlocking = vs->isNonBlocking;

	socklen_t fromLenHost = *fromLen;
	sockaddr fromAddrHost;
	sint32 wsaError = 0;

	while (true)
	{
		// is socket recv shutdown?
		if (vs->isShutdownRecv)
		{
			// return with error
			_setSockError(WU_SO_ESHUTDOWN);
			osLib_returnFromFunction(hCPU, -1);
			return;
		}
		// use select to check for exceptions and read data
		fd_set fd_read;
		fd_set fd_exceptions;
		FD_ZERO(&fd_read);
		FD_ZERO(&fd_exceptions);
		FD_SET(vs->s, &fd_read);
		FD_SET(vs->s, &fd_exceptions);
		timeval t;
		t.tv_sec = 0;
		t.tv_usec = 0;
		int nfds = 0;
#if BOOST_OS_UNIX
		nfds = vs->s + 1;
#endif
		sint32 count = select(nfds, &fd_read, NULL, &fd_exceptions, &t);
		if (count > 0)
		{
			if (FD_ISSET(vs->s, &fd_exceptions))
			{
				cemu_assert_debug(false); // exception
			}
			if (FD_ISSET(vs->s, &fd_read))
			{
				// data available
				r = recvfrom(vs->s, msg, len, hostFlags, &fromAddrHost, &fromLenHost);
				wsaError = GETLASTERR;
				if (r < 0)
				{
					cemu_assert_debug(false);
				}
				*fromLen = fromLenHost;
				fromAddr->sa_family = _swapEndianU16(fromAddrHost.sa_family);
				memcpy(fromAddr->sa_data, fromAddrHost.sa_data, 14);

				_setSockError(0);
				osLib_returnFromFunction(hCPU, r);
				return;
			}
		}
		// nothing to do
		if (requestIsNonBlocking)
		{
			// return with error
			_setSockError(WU_SO_EWOULDBLOCK);
			osLib_returnFromFunction(hCPU, -1);
			return;
		}
		else
		{
			// pause for a while and check again later
			coreinit::OSSleepTicks(ESPRESSO_CORE_CLOCK / 100); // pause for 10ms
			PPCCore_switchToScheduler();
		}
	}
	cemu_assert_debug(false); // should no longer be reached
}


void _convertSockaddrToHostFormat(wu_sockaddr* sockaddru, sockaddr* sockaddrHost)
{
	sockaddrHost->sa_family = _swapEndianU16(sockaddru->sa_family);
	memcpy(sockaddrHost->sa_data, sockaddru->sa_data, 14);
}


void nsysnetExport_sendto(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "sendto({},0x{:08x},{},0x{:x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6]);
	ppcDefineParamS32(s, 0);
	ppcDefineParamStr(msg, 1);
	ppcDefineParamS32(len, 2);
	ppcDefineParamS32(flags, 3);
	ppcDefineParamStructPtr(toAddr, wu_sockaddr, 4);
	ppcDefineParamU32(toLen, 5);

	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	sint32 r = 0;
	if (vs == NULL)
	{
		assert_dbg();
		return;
	}

	int hostFlags = 0;
	bool requestIsNonBlocking = (flags&WU_MSG_DONTWAIT) != 0;
	flags &= ~WU_MSG_DONTWAIT;
	if (flags&WU_MSG_PEEK)
	{
		assert_dbg();
		hostFlags |= MSG_PEEK;
		flags &= ~WU_MSG_PEEK;
	}
	if (vs->isNonBlocking)
		requestIsNonBlocking = vs->isNonBlocking;

	sockaddr toAddrHost;
	toAddrHost.sa_family = _swapEndianU16(toAddr->sa_family);
	memcpy(toAddrHost.sa_data, toAddr->sa_data, 14);

	sint32 wsaError = 0;
	if (requestIsNonBlocking == false)
	{
		// blocking
		_setSocketSendRecvNonBlockingMode(vs->s, true);
		while (true)
		{
			r = sendto(vs->s, msg, len, hostFlags, &toAddrHost, toLen);
			wsaError = GETLASTERR;
			if (r < 0)
			{
				if (wsaError != WSAEWOULDBLOCK)
					break;
				coreinit::OSSleepTicks(ESPRESSO_CORE_CLOCK / 100); // pause for 10ms
				PPCCore_switchToScheduler();
				continue;
			}
			break;
		}
		_setSocketSendRecvNonBlockingMode(vs->s, vs->isNonBlocking);
	}
	else
	{
		// non blocking
		_setSocketSendRecvNonBlockingMode(vs->s, true);
		r = sendto(vs->s, msg, len, hostFlags, &toAddrHost, toLen);
		wsaError = GETLASTERR;
		_setSocketSendRecvNonBlockingMode(vs->s, vs->isNonBlocking);
	}

	_translateError(r <= 0 ? -1 : 0, wsaError);

	osLib_returnFromFunction(hCPU, r);
}


void nsysnetExport_sendto_multi(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "sendto_multi({},0x{:08x},0x{:08x},{})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6]);
	ppcDefineParamS32(s, 0);
	ppcDefineParamStr(data, 1);
	ppcDefineParamS32(dataLen, 2);
	ppcDefineParamU32(flags, 3);
	ppcDefineParamStructPtr(destinationArray, wu_sockaddr, 4);
	ppcDefineParamU32(destinationCount, 5);

	if (flags != 0)
		assert_dbg();

	// todo - somehow handle non-blocking

	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	if (vs == NULL)
	{
		assert_dbg();
		return;
	}

	for (uint32 i = 0; i < destinationCount; i++)
	{
		sockaddr destinationHost;
		_convertSockaddrToHostFormat(&destinationArray[i], &destinationHost);
		sint32 r = sendto(vs->s, (char*)data, (int)dataLen, 0, &destinationHost, sizeof(sockaddr));
		if (r < dataLen)
			assert_dbg(); // todo
	}
	// success
	_setSockError(0);
	osLib_returnFromFunction(hCPU, dataLen);
}

typedef struct
{
	MEMPTR<uint8>				data;
	uint32be					dataLen;
	MEMPTR<uint32be>			sendLenArray;
	uint32be					sendLenArraySize;
	MEMPTR<wu_sockaddr>			destArray;
	uint32be					destArraySize;
	MEMPTR<uint32be>			resultArray;
	uint32be					resultArrayLen;
}sendtomultiBuffer_t;

void nsysnetExport_sendto_multi_ex(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::Socket, "sendto_multi_ex({},0x{:08x},0x{:08x},{})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6]);
	ppcDefineParamS32(s, 0);
	ppcDefineParamU32(flags, 1);
	ppcDefineParamStructPtr(multiBuf, sendtomultiBuffer_t, 2);
	ppcDefineParamU32(num, 3);

	assert_dbg(); // todo - needs testing

	virtualSocket_t* vs = nsysnet_getVirtualSocketObject(s);
	if (vs == NULL)
	{
		assert_dbg();
		return;
	}

	// todo - lots of validation for multiBuf parameters (pointers must not be null and must be aligned, lens must be padded to alignment too)
	// verify multiBuf
	if ((uint32)multiBuf->sendLenArraySize < num ||
		(uint32)multiBuf->destArraySize < num ||
		(uint32)multiBuf->resultArrayLen < num )
	{
		cemu_assert_debug(false);
	}

	for (uint32 i = 0; i < num; i++)
	{
		multiBuf->resultArray[i] = 0;
	}

	uint8* data = multiBuf->data.GetPtr();
	sint32 sendLenSum = 0;
	for (uint32 i = 0; i < num; i++)
	{
		sockaddr destination;
		_convertSockaddrToHostFormat(&multiBuf->destArray[i], &destination);

		uint32 sendLen = (uint32)(multiBuf->sendLenArray[i]);
		sint32 r = sendto(vs->s, (char*)data, (int)sendLen, 0, &destination, sizeof(sockaddr));
		if (r < (sint32)sendLen)
			cemu_assert_debug(false);
		else
			multiBuf->resultArray[i] = r;
		data += sendLen;
		sendLenSum += sendLenSum;
	}
	osLib_returnFromFunction(hCPU, sendLenSum); // return value correct?
}

namespace nsysnet
{
	std::vector<NSSLInternalState_t> g_nsslInternalStates;

	NSSLInternalState_t* GetNSSLContext(sint32 index)
	{
		if (g_nsslInternalStates.size() <= index)
			return nullptr;

		if (g_nsslInternalStates[index].destroyed)
			cemu_assert_suspicious();
	
		return &g_nsslInternalStates[index];
	}

	void export_NSSLCreateContext(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(version, 0);

		NSSLInternalState_t ssl = {};
		ssl.sslVersion = version;
		g_nsslInternalStates.push_back(ssl);

		uint32 nsslHandle = (uint32)g_nsslInternalStates.size() - 1;

		cemuLog_logDebug(LogType::Force, "NSSLCreateContext(0x{:x}) -> 0x{:x}", version, nsslHandle);

		osLib_returnFromFunction(hCPU, nsslHandle);
	}

	void export_NSSLSetClientPKI(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(nsslHandle, 0);
		ppcDefineParamU32(clientPKI, 1);
		cemuLog_logDebug(LogType::Force, "NSSLSetClientPKI(0x{:x}, 0x{:x})", nsslHandle, clientPKI);

		if (g_nsslInternalStates.size() <= nsslHandle || g_nsslInternalStates[nsslHandle].destroyed)
		{
			osLib_returnFromFunction(hCPU, NSSL_INVALID_CTX);
			return;
		}

		g_nsslInternalStates[nsslHandle].clientPKI = clientPKI;
		osLib_returnFromFunction(hCPU, NSSL_OK);
	}

	void export_NSSLAddServerPKI(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(nsslHandle, 0);
		ppcDefineParamU32(serverPKI, 1);
		cemuLog_logDebug(LogType::Force, "NSSLAddServerPKI(0x{:x}, 0x{:x})", nsslHandle, serverPKI);

		if (g_nsslInternalStates.size() <= nsslHandle || g_nsslInternalStates[nsslHandle].destroyed)
		{
			osLib_returnFromFunction(hCPU, NSSL_INVALID_CTX);
			return;
		}

		g_nsslInternalStates[nsslHandle].serverPKIs.insert(serverPKI);
		osLib_returnFromFunction(hCPU, NSSL_OK);
	}

	void export_NSSLAddServerPKIExternal(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(nsslHandle, 0);
		ppcDefineParamMEMPTR(certData, uint8, 1);
		ppcDefineParamS32(certLen, 2);
		ppcDefineParamS32(certType, 3);

		cemuLog_logDebug(LogType::Force, "NSSLAddServerPKIExternal(0x{:x}, 0x{:08x}, 0x{:x}, {})", nsslHandle, certData.GetMPTR(), certLen, certType);
		if (g_nsslInternalStates.size() <= nsslHandle || g_nsslInternalStates[nsslHandle].destroyed)
		{
			osLib_returnFromFunction(hCPU, NSSL_INVALID_CTX);
			return;
		}

		g_nsslInternalStates[nsslHandle].serverCustomPKIs.push_back(std::vector<uint8>(certData.GetPtr(), certData.GetPtr()+certLen));

		osLib_returnFromFunction(hCPU, NSSL_OK);
	}

	void export_NSSLAddServerPKIGroups(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(nsslHandle, 0);
		ppcDefineParamU32(groupMask, 1);
		ppcDefineParamMEMPTR(validCountOut, sint32, 2);
		ppcDefineParamMEMPTR(invalidCountOut, sint32, 3);
		cemuLog_logDebug(LogType::Force, "NSSLAddServerPKIGroups(0x{:x}, 0x{:x}, 0x{:08x}, 0x{:08x})", nsslHandle, groupMask, validCountOut.GetMPTR(), invalidCountOut.GetMPTR());

		if (g_nsslInternalStates.size() <= nsslHandle || g_nsslInternalStates[nsslHandle].destroyed)
		{
			osLib_returnFromFunction(hCPU, NSSL_INVALID_CTX);
			return;
		}

		if (HAS_FLAG(groupMask, 1))
		{
			g_nsslInternalStates[nsslHandle].serverPKIs.insert({ 100,101,102,103,104,105 });
		}

		if (HAS_FLAG(groupMask, 2))
		{
			g_nsslInternalStates[nsslHandle].serverPKIs.insert({
				1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008, 1009,
				1010, 1011, 1012, 1013, 1014, 1015, 1016, 1017, 1018, 1019,
				1020, 1021, 1022, 1023, 1024, 1025, 1026, 1027, 1028, 1029,
				1030, 1031, 1032, 1033 });
		}

		if (validCountOut)
			*validCountOut = (sint32)g_nsslInternalStates[nsslHandle].serverPKIs.size();

		if (invalidCountOut)
			*invalidCountOut = 0;

		osLib_returnFromFunction(hCPU, NSSL_OK);
	}

	void export_NSSLDestroyContext(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(nsslHandle, 0);
		cemuLog_logDebug(LogType::Force, "NSSLDestroyContext(0x{:x})", nsslHandle);

		if (g_nsslInternalStates.size() <= nsslHandle || g_nsslInternalStates[nsslHandle].destroyed)
		{
			osLib_returnFromFunction(hCPU, NSSL_INVALID_CTX);
			return;
		}

		g_nsslInternalStates[nsslHandle].destroyed = true;
		osLib_returnFromFunction(hCPU, NSSL_OK);
	}

	void export_NSSLExportInternalServerCertificate(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(serverCertId, 0);
		ppcDefineParamUStr(output, 1);
		ppcDefineParamU32BEPtr(outputSize, 2);
		ppcDefineParamU32BEPtr(certType, 3);

		sint32 certificateSize = 0;
		uint8* certificateData = iosuCrypto_getCertificateDataById(serverCertId, &certificateSize);
		if (certificateData == nullptr)
		{
			cemu_assert_debug(false);
		}

		if( output )
			memcpy(output, certificateData, certificateSize);
		*outputSize = (uint32)certificateSize;
		*certType = 0;

		osLib_returnFromFunction(hCPU, NSSL_OK);
	}

	void export_NSSLExportInternalClientCertificate(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(serverCertId, 0);
		ppcDefineParamUStr(output, 1);
		ppcDefineParamU32BEPtr(outputSize, 2);
		ppcDefineParamU32BEPtr(certType, 3);
		ppcDefineParamUStr(pkeyOutput, 4);
		ppcDefineParamU32BEPtr(pkeyOutputSize, 5);
		ppcDefineParamU32BEPtr(pkeyCertType, 6);

		// certificate
		sint32 certificateSize = 0;
		uint8* certificateData = iosuCrypto_getCertificateDataById(serverCertId, &certificateSize);
		if (certificateData == nullptr)
		{
			assert_dbg(); // todo
		}
		if (output)
			memcpy(output, certificateData, certificateSize);
		*outputSize = (uint32)certificateSize;
		*certType = 0;

		// private key
		sint32 pkeySize = 0;
		uint8* pkeyData = iosuCrypto_getCertificatePrivateKeyById(serverCertId, &pkeySize);
		if (pkeyData == nullptr)
		{
			assert_dbg(); // todo
		}
		if (pkeyOutput)
			memcpy(pkeyOutput, pkeyData, pkeySize);
		*pkeyOutputSize = (uint32)pkeySize;
		*pkeyCertType = 0;

		osLib_returnFromFunction(hCPU, NSSL_OK);
	}

	void wuResetFD(struct wu_fd_set* fdset)
	{
		fdset->mask = 0;
	}

	void wuSetFD(struct wu_fd_set* fdset, sint32 fd)
	{
		fdset->mask |= (1 << fd);
	}

}

namespace nsysnet
{
    void Initialize()
    {
        cafeExportRegister("nsysnet", inet_ntop, LogType::Socket);
    }
}

// register nsysnet functions
void nsysnet_load()
{
    nsysnet::Initialize();

    // the below code is the old way of registering API which is deprecated

    osLib_addFunction("nsysnet", "socket_lib_init", nsysnetExport_socket_lib_init);
	osLib_addFunction("nsysnet", "socket_lib_finish", nsysnetExport_socket_lib_finish);
	
	// socket API
	osLib_addFunction("nsysnet", "socket", nsysnetExport_socket);
	osLib_addFunction("nsysnet", "mw_socket", nsysnetExport_mw_socket);
	osLib_addFunction("nsysnet", "shutdown", nsysnetExport_shutdown);
	osLib_addFunction("nsysnet", "socketclose", nsysnetExport_socketclose);
	osLib_addFunction("nsysnet", "setsockopt", nsysnetExport_setsockopt);
	osLib_addFunction("nsysnet", "getsockopt", nsysnetExport_getsockopt);
	osLib_addFunction("nsysnet", "bind", nsysnetExport_bind);
	osLib_addFunction("nsysnet", "listen", nsysnetExport_listen);
	osLib_addFunction("nsysnet", "accept", nsysnetExport_accept);
	osLib_addFunction("nsysnet", "connect", nsysnetExport_connect);
	osLib_addFunction("nsysnet", "send", nsysnetExport_send);
	osLib_addFunction("nsysnet", "recv", nsysnetExport_recv);
	osLib_addFunction("nsysnet", "select", nsysnetExport_select);
	osLib_addFunction("nsysnet", "getsockname", nsysnetExport_getsockname);
	osLib_addFunction("nsysnet", "getpeername", nsysnetExport_getpeername);

	osLib_addFunction("nsysnet", "inet_aton", nsysnetExport_inet_aton);
	osLib_addFunction("nsysnet", "inet_pton", nsysnetExport_inet_pton);
	osLib_addFunction("nsysnet", "inet_ntoa", nsysnetExport_inet_ntoa);
	osLib_addFunction("nsysnet", "htons", nsysnetExport_htons);
	osLib_addFunction("nsysnet", "htonl", nsysnetExport_htonl);
	osLib_addFunction("nsysnet", "ntohs", nsysnetExport_ntohs);
	osLib_addFunction("nsysnet", "ntohl", nsysnetExport_ntohl);
	osLib_addFunction("nsysnet", "gethostbyname", nsysnetExport_gethostbyname);
	osLib_addFunction("nsysnet", "gethostbyaddr", nsysnetExport_gethostbyaddr);
	osLib_addFunction("nsysnet", "getaddrinfo", nsysnetExport_getaddrinfo);

	osLib_addFunction("nsysnet", "socketlasterr", nsysnetExport_socketlasterr);

	// unfinished implementations
	osLib_addFunction("nsysnet", "recvfrom", nsysnetExport_recvfrom);
	osLib_addFunction("nsysnet", "recvfrom_ex", nsysnetExport_recvfrom_ex);
	osLib_addFunction("nsysnet", "sendto", nsysnetExport_sendto);

	osLib_addFunction("nsysnet", "sendto_multi", nsysnetExport_sendto_multi);
	osLib_addFunction("nsysnet", "sendto_multi_ex", nsysnetExport_sendto_multi_ex);


	// NSSL API
	osLib_addFunction("nsysnet", "NSSLCreateContext", nsysnet::export_NSSLCreateContext);
	osLib_addFunction("nsysnet", "NSSLSetClientPKI", nsysnet::export_NSSLSetClientPKI);
	osLib_addFunction("nsysnet", "NSSLAddServerPKI", nsysnet::export_NSSLAddServerPKI);
	osLib_addFunction("nsysnet", "NSSLAddServerPKIExternal", nsysnet::export_NSSLAddServerPKIExternal);
	osLib_addFunction("nsysnet", "NSSLAddServerPKIGroups", nsysnet::export_NSSLAddServerPKIGroups);
	osLib_addFunction("nsysnet", "NSSLDestroyContext", nsysnet::export_NSSLDestroyContext);

	osLib_addFunction("nsysnet", "NSSLExportInternalServerCertificate", nsysnet::export_NSSLExportInternalServerCertificate);
	osLib_addFunction("nsysnet", "NSSLExportInternalClientCertificate", nsysnet::export_NSSLExportInternalClientCertificate);
}
