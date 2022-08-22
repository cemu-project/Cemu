#pragma once

struct SysProdSettings
{
	uint8 ukn00;			// 0x00
	uint8 ukn01;			// 0x01
	uint8 ukn02;			// 0x02
	uint8 platformRegion;	// 0x03
	uint8 ukn04;
	uint8 ukn05;
	uint8 ukn06;
	uint8 ukn07;
	uint8 prevBlock;
	uint8 ukn09;
	uint8 ukn0A;
	uint8 gameRegion;		// game region?
	uint8 nextBlock;
	uint8 ukn0D;
	uint8 ukn0E;
	uint8 ukn0F;
	uint8 ukn10;
	uint8 ukn11;
	uint8 ukn12;
	uint8 ukn13;
	uint8 ukn14[4];
	uint8 ukn18[4];
	uint8 ukn1C[4];
	uint8 ukn20[4];
	uint8 ukn24[4];
	uint8 ukn28[4];
	uint8 ukn2C[4];
	uint8 ukn30[4];
	uint8 ukn34[4];
	uint8 ukn38[4];
	uint8 ukn3C[4];
	uint8 ukn40[4];
	uint8 ukn44;
	uint8 ukn45;
};

static_assert(sizeof(SysProdSettings) == 0x46);

typedef uint32 MCPHANDLE;

MCPHANDLE MCP_Open();
void MCP_Close(MCPHANDLE mcpHandle);
sint32 MCP_GetSysProdSettings(MCPHANDLE mcpHandle, SysProdSettings* sysProdSettings);

void coreinitExport_UCReadSysConfig(PPCInterpreter_t* hCPU);

namespace coreinit
{
	void InitializeMCP();
}