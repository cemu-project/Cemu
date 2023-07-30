struct RPLStoredSymbol
{
	MPTR address;
	void* libName;
	void* symbolName;
	uint32 flags;
};

void rplSymbolStorage_init();
void rplSymbolStorage_unloadAll();
RPLStoredSymbol* rplSymbolStorage_store(const char* libName, const char* symbolName, MPTR address);
void rplSymbolStorage_remove(RPLStoredSymbol* storedSymbol);
void rplSymbolStorage_removeRange(MPTR address, sint32 length);
RPLStoredSymbol* rplSymbolStorage_getByAddress(MPTR address);
RPLStoredSymbol* rplSymbolStorage_getByClosestAddress(MPTR address);
void rplSymbolStorage_createJumpProxySymbol(MPTR jumpAddress, MPTR destAddress);

std::unordered_map<uint32, RPLStoredSymbol*>& rplSymbolStorage_lockSymbolMap();
void rplSymbolStorage_unlockSymbolMap();