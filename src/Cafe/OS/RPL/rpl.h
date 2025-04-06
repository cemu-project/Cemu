#pragma once

struct RPLModule;

#define RPL_INVALID_HANDLE		0xFFFFFFFF

void RPLLoader_InitState();
void RPLLoader_ResetState();

uint8* RPLLoader_AllocateTrampolineCodeSpace(sint32 size);

MPTR RPLLoader_AllocateCodeSpace(uint32 size, uint32 alignment);

uint32 RPLLoader_GetMaxCodeOffset();
uint32 RPLLoader_GetDataAllocatorAddr();

RPLModule* RPLLoader_LoadFromMemory(uint8* rplData, sint32 size, char* name);
uint32 rpl_mapHLEImport(RPLModule* rplLoaderContext, const char* rplName, const char* funcName, bool functionMustExist);
void RPLLoader_Link();

MPTR RPLLoader_FindRPLExport(RPLModule* rplLoaderContext, const char* symbolName, bool isData);
uint32 RPLLoader_GetModuleEntrypoint(RPLModule* rplLoaderContext);

void RPLLoader_SetMainModule(RPLModule* rplLoaderContext);
uint32 RPLLoader_GetMainModuleHandle();

void RPLLoader_CallEntrypoints();
void RPLLoader_NotifyControlPassedToApplication();

void RPLLoader_AddDependency(const char* name);
void RPLLoader_RemoveDependency(uint32 handle);
bool RPLLoader_HasDependency(std::string_view name);
void RPLLoader_UpdateDependencies();

uint32 RPLLoader_GetHandleByModuleName(const char* name);
uint32 RPLLoader_GetMaxTLSModuleIndex();
bool RPLLoader_GetTLSDataByTLSIndex(sint16 tlsModuleIndex, uint8** tlsData, sint32* tlsSize);

uint32 RPLLoader_FindModuleOrHLEExport(uint32 moduleHandle, bool isData, const char* exportName);

uint32 RPLLoader_GetSDA1Base();
uint32 RPLLoader_GetSDA2Base();

sint32 RPLLoader_GetModuleCount();
RPLModule** RPLLoader_GetModuleList();

MEMPTR<void> RPLLoader_AllocateCodeCaveMem(uint32 alignment, uint32 size);
void RPLLoader_ReleaseCodeCaveMem(MEMPTR<void> addr);

// exports

uint32 RPLLoader_MakePPCCallable(void(*ppcCallableExport)(struct PPCInterpreter_t* hCPU));

// elf loader

uint32 ELF_LoadFromMemory(uint8* elfData, sint32 size, const char* name);