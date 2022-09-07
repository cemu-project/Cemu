#pragma once
#include "Common/precompiled.h"

#ifdef _WIN32
#include "Common/windows/FileStream_win32.h"
#else
#include "Common/unix/FileStream_unix.h"
#endif
