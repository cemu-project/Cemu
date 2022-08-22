#pragma once
#include <map>

enum RplDebugSymbolType : uint8
{
	RplDebugSymbolComment = 0,
};

struct rplDebugSymbolBase
{
	RplDebugSymbolType type;
	rplDebugSymbolBase* next;
};

struct rplDebugSymbolComment : rplDebugSymbolBase
{
	std::wstring comment;
};


void rplDebugSymbol_createComment(MPTR address, const wchar_t* comment);
rplDebugSymbolBase* rplDebugSymbol_getForAddress(MPTR address);
const std::map<MPTR, rplDebugSymbolBase*>&  rplDebugSymbol_getSymbols();