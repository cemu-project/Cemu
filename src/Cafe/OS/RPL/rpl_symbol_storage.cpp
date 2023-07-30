#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/RPL/rpl_symbol_storage.h"

struct rplSymbolLib_t
{
	char* libName;
	rplSymbolLib_t* next;
};

struct  
{
	rplSymbolLib_t* libs;
	std::mutex m_symbolStorageMutex;
	std::unordered_map<uint32, RPLStoredSymbol*> map_symbolByAddress;
	// allocator for strings
	char* strAllocatorBlock;
	sint32 strAllocatorOffset;
	std::vector<void*> list_strAllocatedBlocks;
}rplSymbolStorage = { 0 };

#define STR_ALLOC_BLOCK_SIZE	(128*1024) // allocate 128KB blocks at once

char* rplSymbolStorage_allocDupString(const char* str)
{
	sint32 len = (sint32)strlen(str);
	if (rplSymbolStorage.strAllocatorBlock == nullptr || (rplSymbolStorage.strAllocatorOffset + len + 1) >= STR_ALLOC_BLOCK_SIZE)
	{
		// allocate new block
		rplSymbolStorage.strAllocatorBlock = (char*)malloc(STR_ALLOC_BLOCK_SIZE);
		rplSymbolStorage.strAllocatorOffset = 0;
		rplSymbolStorage.list_strAllocatedBlocks.emplace_back(rplSymbolStorage.strAllocatorBlock);
	}
	cemu_assert_debug((rplSymbolStorage.strAllocatorOffset + len + 1) <= STR_ALLOC_BLOCK_SIZE);
	char* allocatedStr = rplSymbolStorage.strAllocatorBlock + rplSymbolStorage.strAllocatorOffset;
	rplSymbolStorage.strAllocatorOffset += len + 1;
	strcpy(allocatedStr, str);
	return allocatedStr;
}

char* rplSymbolStorage_storeLibname(const char* libName)
{
	if (rplSymbolStorage.libs == NULL)
	{
		rplSymbolLib_t* libEntry = new rplSymbolLib_t();
		libEntry->libName = rplSymbolStorage_allocDupString(libName);
		libEntry->next = NULL;
		rplSymbolStorage.libs = libEntry;
		return libEntry->libName;
	}
	rplSymbolLib_t* libItr = rplSymbolStorage.libs;
	while (libItr)
	{
		if (boost::iequals(libItr->libName, libName))
			return libItr->libName;
		// next
		libItr = libItr->next;
	}
	// create new entry
	rplSymbolLib_t* libEntry = new rplSymbolLib_t();
	libEntry->libName = rplSymbolStorage_allocDupString(libName);
	libEntry->next = rplSymbolStorage.libs;
	rplSymbolStorage.libs = libEntry;
	return libEntry->libName;
}

RPLStoredSymbol* rplSymbolStorage_store(const char* libName, const char* symbolName, MPTR address)
{
	std::unique_lock<std::mutex> lck(rplSymbolStorage.m_symbolStorageMutex);
	char* libNameStorage = rplSymbolStorage_storeLibname(libName);
	char* symbolNameStorage = rplSymbolStorage_allocDupString(symbolName);
	RPLStoredSymbol* storedSymbol = new RPLStoredSymbol();
	storedSymbol->address = address;
	storedSymbol->libName = libNameStorage;
	storedSymbol->symbolName = symbolNameStorage;
	storedSymbol->flags = 0;
	rplSymbolStorage.map_symbolByAddress[address] = storedSymbol;
	return storedSymbol;
}

RPLStoredSymbol* rplSymbolStorage_getByAddress(MPTR address)
{
	std::unique_lock<std::mutex> lck(rplSymbolStorage.m_symbolStorageMutex);
	return rplSymbolStorage.map_symbolByAddress[address];
}

RPLStoredSymbol* rplSymbolStorage_getByClosestAddress(MPTR address)
{
    // highly inefficient but doesn't matter for now
    std::unique_lock<std::mutex> lck(rplSymbolStorage.m_symbolStorageMutex);
    for(uint32 i=0; i<4096; i++)
    {
        RPLStoredSymbol* symbol = rplSymbolStorage.map_symbolByAddress[address];
        if(symbol)
            return symbol;
        address -= 4;
    }
    return nullptr;
}

void rplSymbolStorage_remove(RPLStoredSymbol* storedSymbol)
{
	std::unique_lock<std::mutex> lck(rplSymbolStorage.m_symbolStorageMutex);
	if (rplSymbolStorage.map_symbolByAddress[storedSymbol->address] == storedSymbol)
		rplSymbolStorage.map_symbolByAddress[storedSymbol->address] = nullptr;
	delete storedSymbol;
}

void rplSymbolStorage_removeRange(MPTR address, sint32 length)
{
	while (length > 0)
	{
		RPLStoredSymbol* symbol = rplSymbolStorage_getByAddress(address);
		if (symbol)
			rplSymbolStorage_remove(symbol);
		address += 4;
		length -= 4;
	}
}

void rplSymbolStorage_createJumpProxySymbol(MPTR jumpAddress, MPTR destAddress)
{
	RPLStoredSymbol* destSymbol = rplSymbolStorage_getByAddress(destAddress);
	if (destSymbol)
		rplSymbolStorage_store((char*)destSymbol->libName, (char*)destSymbol->symbolName, jumpAddress);
}

std::unordered_map<uint32, RPLStoredSymbol*>& rplSymbolStorage_lockSymbolMap()
{
	rplSymbolStorage.m_symbolStorageMutex.lock();
	return rplSymbolStorage.map_symbolByAddress;
}

void rplSymbolStorage_unlockSymbolMap()
{
	rplSymbolStorage.m_symbolStorageMutex.unlock();
}

void rplSymbolStorage_init()
{
	cemu_assert_debug(rplSymbolStorage.map_symbolByAddress.empty());
	cemu_assert_debug(rplSymbolStorage.strAllocatorBlock == nullptr);
}

void rplSymbolStorage_unloadAll()
{
	// free symbols
	for (auto& it : rplSymbolStorage.map_symbolByAddress)
		delete it.second;
	rplSymbolStorage.map_symbolByAddress.clear();
	// free libs
	rplSymbolLib_t* lib = rplSymbolStorage.libs;
	while (lib)
	{
		rplSymbolLib_t* next = lib->next;
		delete lib;
		lib = next;
	}
	rplSymbolStorage.libs = nullptr;
	// free strings
	for (auto it : rplSymbolStorage.list_strAllocatedBlocks)
		free(it);
    rplSymbolStorage.list_strAllocatedBlocks.clear();
	rplSymbolStorage.strAllocatorBlock = nullptr;
	rplSymbolStorage.strAllocatorOffset = 0;
}
