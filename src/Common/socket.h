#pragma once

#if BOOST_OS_WINDOWS

#include <WinSock2.h>
typedef int socklen_t;

#else

#include <sys/socket.h>
#define SOCKET int
#define closesocket close
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1

#endif
