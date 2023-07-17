
#pragma pack(1)

struct MCPTitleInfo
{

	/* +0x00 */ uint32be titleIdHigh; // app.xml
	/* +0x04 */ uint32be titleIdLow;
	/* +0x08 */ uint32be appGroup;
	/* +0x0C */ char appPath[0x38]; // size guessed
	/* +0x44 */ uint32be appType; // app.xml
	/* +0x48 */ uint16be titleVersion; // app.xml
	// everything below is uncertain
	/* +0x4A */ uint64be osVersion; // app.xml
	/* +0x52 */ uint32be sdkVersion; // app.xml
	/* +0x56 */ uint8 deviceName[10];
	/* +0x60 */ uint8 uknPadding; // possibly the index of the device?
	//move this and the stuff below
};

static_assert(sizeof(MCPTitleInfo) == 0x61);

#pragma pack()

// custom dev/mcp protocol (Cemu only)
#define IOSU_MCP_REQUEST_CEMU (0xEE)

typedef struct
{
	uint32 requestCode;
	struct  
	{
		MEMPTR<MCPTitleInfo> titleList;
		uint32be titleCount;
		uint32be titleListBufferSize;
		uint64 titleId;
		// filters
		uint32be appType;
	}titleListRequest;

	// output
	uint32 returnCode; // ACP return value
	union
	{
		//struct
		//{
		//	uint64 u64;
		//}resultU64;
		struct
		{
			uint32 u32;
		}resultU32;
		//struct
		//{
		//	char strBuffer[1024];
		//}resultString;
		//struct
		//{
		//	uint8 binBuffer[1024];
		//}resultBinary;
	};
}iosuMcpCemuRequest_t;

// MCP request Cemu subcodes
#define IOSU_MCP_GET_TITLE_LIST								0x01
#define IOSU_MCP_GET_TITLE_LIST_BY_APP_TYPE					0x02
#define IOSU_MCP_GET_TITLE_INFO								0x03	// get title list by titleId
#define IOSU_MCP_GET_TITLE_COUNT							0x04

namespace iosu
{
	sint32 mcpGetTitleCount();
	sint32 mcpGetTitleList(MCPTitleInfo* titleList, uint32 titleListBufferSize, uint32be* titleCount);

	void iosuMcp_init();

	namespace mcp
	{
		void Init();
		void Shutdown();
	};
}
