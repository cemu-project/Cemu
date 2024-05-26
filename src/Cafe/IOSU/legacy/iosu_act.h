#pragma once

void iosuAct_init_depr();
bool iosuAct_isInitialized();

#define ACT_ACCOUNTID_LENGTH 	(17) // includes '\0'

// Mii

#define MII_FFL_STORAGE_SIZE	(96)

#define MII_FFL_NAME_LENGTH		(10) // counted in wchar_t elements (16-bit unicode)

#define ACT_NICKNAME_LENGTH	(10) // aka Mii nickname
#define ACT_NICKNAME_SIZE	(11)

typedef struct  
{
	uint32be high;
	uint32be low;
}FFLDataID_t;

typedef struct  
{
	/* +0x00 */ uint32		uknFlags;
	/* +0x04 */ FFLDataID_t miiId; // bytes 8 and 9 are part of the CRC? (miiId is based on account transferable id?)
	/* +0x0C */ uint8		ukn0C[0xA];
	/* +0x16 */ uint8		ukn16[2];
	/* +0x18 */ uint16		ukn18;
	/* +0x1A */ uint16le	miiName[MII_FFL_NAME_LENGTH];
	/* +0x2E */ uint16		ukn2E;
	/* +0x30 */ uint8		ukn30[MII_FFL_STORAGE_SIZE-0x30];
}FFLData_t;

static_assert(sizeof(FFLData_t) == MII_FFL_STORAGE_SIZE, "FFLData_t size invalid");
static_assert(offsetof(FFLData_t, miiId) == 0x04, "FFLData->miiId offset invalid");
static_assert(offsetof(FFLData_t, miiName) == 0x1A, "FFLData->miiName offset invalid");
static_assert(offsetof(FFLData_t, ukn2E) == 0x2E, "FFLData->ukn2E offset invalid");

static uint16 FFLCalculateCRC16(uint8* input, sint32 length);

namespace iosu
{
	namespace act
	{
		uint8 getCurrentAccountSlot();
		bool getPrincipalId(uint8 slot, uint32* principalId);
		bool getAccountId(uint8 slot, char* accountId);
		bool getMii(uint8 slot, FFLData_t* fflData);
		bool getScreenname(uint8 slot, uint16 screenname[ACT_NICKNAME_LENGTH]);
		bool getCountryIndex(uint8 slot, uint32* countryIndex);
		bool GetPersistentId(uint8 slot, uint32* persistentId);

		std::string getAccountId2(uint8 slot);

		static constexpr uint8 ACT_SLOT_CURRENT = 0xFE;

		void Initialize();
		void Stop();
	}
}

// custom dev/act/ protocol (Cemu only)
#define IOSU_ACT_REQUEST_CEMU (0xEE)

struct iosuActCemuRequest_t
{
	uint32 requestCode;
	// input
	uint8 accountSlot;
	uint32 unique;
	sint32 uuidName;
	uint64 titleId;
	uint32 titleVersion;
	uint32 serverId;
	char clientId[64];
	uint32 expiresIn;
	// output
	uint32 returnCode;
	union
	{
		struct
		{
			uint64 u64;
		}resultU64;
		struct
		{
			uint32 u32;
		}resultU32;
		struct
		{
			char strBuffer[1024];
		}resultString;
		struct  
		{
			uint8 binBuffer[1024];
		}resultBinary;
	};

	void setACTReturnCode(uint32 code)
	{
		returnCode = code;
	}
};

// Act Request Cemu subcodes
#define IOSU_ARC_INIT					0x00
#define IOSU_ARC_ACCOUNT_ID				0x01
#define IOSU_ARC_TRANSFERABLEID			0x02
#define IOSU_ARC_PERSISTENTID			0x03
#define IOSU_ARC_UUID					0x04
#define IOSU_ARC_SIMPLEADDRESS			0x05
#define IOSU_ARC_PRINCIPALID			0x06
#define IOSU_ARC_COUNTRY				0x07
#define IOSU_ARC_ISNETWORKACCOUNT		0x08
#define IOSU_ARC_ACQUIRENEXTOKEN		0x09
#define IOSU_ARC_MIIDATA				0x0A
#define IOSU_ARC_ACQUIREINDEPENDENTTOKEN 0x0B
#define IOSU_ARC_ACQUIREPIDBYNNID		0x0C

uint32 iosuAct_getAccountIdOfCurrentAccount();

bool iosuAct_isAccountDataLoaded();