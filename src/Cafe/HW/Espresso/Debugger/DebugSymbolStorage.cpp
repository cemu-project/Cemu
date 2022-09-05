#include "DebugSymbolStorage.h"
#include "Common/precompiled.h"

FSpinlock DebugSymbolStorage::s_lock;
std::unordered_map<MPTR, DEBUG_SYMBOL_TYPE> DebugSymbolStorage::s_typeStorage;
