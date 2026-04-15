#pragma once

#if BOOST_OS_WINDOWS

#include <WinSock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;

#define GETLASTERR WSAGetLastError()

#elif BOOST_OS_UNIX

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAEINPROGRESS EINPROGRESS
#define WSAESHUTDOWN ESHUTDOWN
#define WSAECONNABORTED ECONNABORTED
#define WSAHOST_NOT_FOUND EAI_NONAME
#define WSAENOTCONN ENOTCONN

#define GETLASTERR errno

#define SOCKET int
#define closesocket close
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1

#endif // BOOST_OS_UNIX
