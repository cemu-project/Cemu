enum RPLStoredSymbolType : uint32
{
	RPL_STORED_SYMBOL_NONE = 0,
	RPL_STORED_SYMBOL_MAP = 1 << 0,
	RPL_STORED_SYMBOL_PATCH = 1 << 1,
};

struct RPLStoredSymbol
{
	MPTR address;
	void* libName;
	void* symbolName;
	uint32 flags;
	RPLStoredSymbol* previous;
};

void rplSymbolStorage_init();
void rplSymbolStorage_unloadAll();
RPLStoredSymbol* rplSymbolStorage_store(const char* libName, const char* symbolName, MPTR address, uint32 type = RPL_STORED_SYMBOL_NONE);
void rplSymbolStorage_remove(RPLStoredSymbol* storedSymbol);
void rplSymbolStorage_removeRange(MPTR address, sint32 length, uint32 type = RPL_STORED_SYMBOL_NONE);
RPLStoredSymbol* rplSymbolStorage_getByAddress(MPTR address);
RPLStoredSymbol* rplSymbolStorage_getByClosestAddress(MPTR address);
void rplSymbolStorage_createJumpProxySymbol(MPTR jumpAddress, MPTR destAddress);

std::unordered_map<uint32, RPLStoredSymbol*>& rplSymbolStorage_lockSymbolMap();
void rplSymbolStorage_unlockSymbolMap();
