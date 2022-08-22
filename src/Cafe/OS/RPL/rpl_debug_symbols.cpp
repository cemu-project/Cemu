#include "Cafe/OS/RPL/rpl_debug_symbols.h"

std::map<MPTR, rplDebugSymbolBase*> map_DebugSymbols;

void rplDebugSymbol_createComment(MPTR address, const wchar_t* comment)
{
	auto new_comment = new rplDebugSymbolComment();
	new_comment->type = RplDebugSymbolComment;
	new_comment->comment = comment;
	map_DebugSymbols[address] = new_comment;
}

rplDebugSymbolBase* rplDebugSymbol_getForAddress(MPTR address)
{
	return map_DebugSymbols[address];
}

const std::map<MPTR, rplDebugSymbolBase*>& rplDebugSymbol_getSymbols()
{
	return map_DebugSymbols;
}