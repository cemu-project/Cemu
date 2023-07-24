#include "iosu_act.h"
#include "iosu_ioctl.h"

#include "Cafe/OS/libs/nn_common.h"
#include "gui/CemuApp.h"

#include <algorithm>
#include <mutex>

#include "openssl/evp.h" /* EVP_Digest */
#include "openssl/sha.h" /* SHA256_DIGEST_LENGTH */
#include "Cafe/Account/Account.h"
#include "config/ActiveSettings.h"
#include "util/helpers/helpers.h"

#include "Cemu/napi/napi.h"
#include "Cemu/ncrypto/ncrypto.h"

#include "Cafe/IOSU/kernel/iosu_kernel.h"
#include "Cafe/IOSU/nn/iosu_nn_service.h"

using namespace iosu::kernel;

struct  
{
	bool isInitialized;
}iosuAct = { };

// account manager

typedef struct  
{
	bool isValid;
	// options
	bool isNetworkAccount;
	bool hasParseError; // set if any occurs while parsing account.dat
	// IDs
	uint8 uuid[16];
	uint32 persistentId;
	uint64 transferableIdBase;
	uint32 simpleAddressId;
	uint32 principalId;
	// NNID
	char accountId[64];
	uint8 accountPasswordCache[32];
	// country & language
	uint32 countryIndex;
	char country[8];
	// Mii
	FFLData_t miiData;
	uint16le miiNickname[ACT_NICKNAME_LENGTH];
}actAccountData_t;

#define IOSU_ACT_ACCOUNT_MAX_COUNT (0xC)

actAccountData_t _actAccountData[IOSU_ACT_ACCOUNT_MAX_COUNT] = {};
bool _actAccountDataInitialized = false;

void FillAccountData(const Account& account, const bool online_enabled, int index)
{
	cemu_assert_debug(index < IOSU_ACT_ACCOUNT_MAX_COUNT);
	auto& data = _actAccountData[index];
	data.isValid = true;
	// options
	data.isNetworkAccount = account.IsValidOnlineAccount();
	data.hasParseError = false;
	// IDs
	std::copy(account.GetUuid().cbegin(), account.GetUuid().cend(), data.uuid);
	data.persistentId = account.GetPersistentId();
	data.transferableIdBase = account.GetTransferableIdBase();
	data.simpleAddressId = account.GetSimpleAddressId();
	data.principalId = account.GetPrincipalId();
	// NNID
	std::copy(account.GetAccountId().begin(), account.GetAccountId().end(), data.accountId);
	std::copy(account.GetAccountPasswordCache().begin(), account.GetAccountPasswordCache().end(), data.accountPasswordCache);
	// country & language
	data.countryIndex = account.GetCountry();
	strcpy(data.country, NCrypto::GetCountryAsString(data.countryIndex));
	// Mii
	std::copy(account.GetMiiData().begin(), account.GetMiiData().end(), (uint8*)&data.miiData);
	std::copy(account.GetMiiName().begin(), account.GetMiiName().end(), data.miiNickname);
		
	// if online mode is disabled, make all accounts offline
	if(!online_enabled)
	{
		data.isNetworkAccount = false;
		data.principalId = 0;
		data.simpleAddressId = 0;
		memset(data.accountId, 0x00, sizeof(data.accountId));
	}
}

void iosuAct_loadAccounts()
{
	if (_actAccountDataInitialized)
		return;

	const bool online_enabled = ActiveSettings::IsOnlineEnabled();
	const auto persistent_id = ActiveSettings::GetPersistentId();

	// first account is always our selected one
	int counter = 0;
	const auto& first_acc = Account::GetAccount(persistent_id);
	FillAccountData(first_acc, online_enabled, counter);
	++counter;
	// enable multiple accounts for cafe functions (badly tested)
	//for (const auto& account : Account::GetAccounts())
	//{
	//	if (first_acc.GetPersistentId() != account.GetPersistentId())
	//	{
	//		FillAccountData(account, online_enabled, counter);
	//		++counter;
	//	}
	//}

	cemuLog_log(LogType::Force, L"IOSU_ACT: using account {} in first slot", first_acc.GetMiiName());
	
	_actAccountDataInitialized = true;
}

bool iosuAct_isAccountDataLoaded()
{
	return _actAccountDataInitialized;
}

uint32 iosuAct_acquirePrincipalIdByAccountId(const char* nnid, uint32* pid)
{
	NAPI::AuthInfo authInfo;
	NAPI::NAPI_MakeAuthInfoFromCurrentAccount(authInfo);
	NAPI::ACTConvertNnidToPrincipalIdResult result = NAPI::ACT_ACTConvertNnidToPrincipalId(authInfo, nnid);
	if (result.isValid() && result.isFound)
	{
		*pid = result.principalId;
	}
	else
	{
		*pid = 0;
		return BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_ACT, 0); // what error should we return? The friend list app expects nn_act.AcquirePrincipalIdByAccountId to never return an error
	}
	return 0;
}

sint32 iosuAct_getAccountIndexBySlot(uint8 slot)
{
	if (slot == iosu::act::ACT_SLOT_CURRENT)
		return 0;
	if (slot == 0xFF)
		return 0; // ?
	cemu_assert_debug(slot != 0);
	cemu_assert_debug(slot <= IOSU_ACT_ACCOUNT_MAX_COUNT);
	return slot - 1;
}

uint32 iosuAct_getAccountIdOfCurrentAccount()
{
	cemu_assert_debug(_actAccountData[0].isValid);
	return _actAccountData[0].persistentId;
}

// IOSU act API interface

namespace iosu
{
	namespace act
	{
		uint8 getCurrentAccountSlot()
		{
			return 1;
		}

		bool getPrincipalId(uint8 slot, uint32* principalId)
		{
			sint32 accountIndex = iosuAct_getAccountIndexBySlot(slot);
			if (_actAccountData[accountIndex].isValid == false)
			{
				*principalId = 0;
				return false;
			}
			*principalId = _actAccountData[accountIndex].principalId;
			return true;
		}

		bool getAccountId(uint8 slot, char* accountId)
		{
			sint32 accountIndex = iosuAct_getAccountIndexBySlot(slot);
			if (_actAccountData[accountIndex].isValid == false)
			{
				*accountId = '\0';
				return false;
			}
			strcpy(accountId, _actAccountData[accountIndex].accountId);
			return true;
		}

		bool getMii(uint8 slot, FFLData_t* fflData)
		{
			sint32 accountIndex = iosuAct_getAccountIndexBySlot(slot);
			if (_actAccountData[accountIndex].isValid == false)
			{
				return false;
			}
			memcpy(fflData, &_actAccountData[accountIndex].miiData, sizeof(FFLData_t));
			return true;
		}

		// return screenname in little-endian wide characters
		bool getScreenname(uint8 slot, uint16 screenname[ACT_NICKNAME_LENGTH])
		{
			sint32 accountIndex = iosuAct_getAccountIndexBySlot(slot);
			if (_actAccountData[accountIndex].isValid == false)
			{
				screenname[0] = '\0';
				return false;
			}
			for (sint32 i = 0; i < ACT_NICKNAME_LENGTH; i++)
			{
				screenname[i] = (uint16)_actAccountData[accountIndex].miiNickname[i];
			}
			return true;
		}

		bool getCountryIndex(uint8 slot, uint32* countryIndex)
		{
			sint32 accountIndex = iosuAct_getAccountIndexBySlot(slot);
			if (_actAccountData[accountIndex].isValid == false)
			{
				*countryIndex = 0;
				return false;
			}
			*countryIndex = _actAccountData[accountIndex].countryIndex;
			return true;
		}

		class ActService : public iosu::nn::IPCService
		{
		public:
			ActService() : iosu::nn::IPCService("/dev/act") {}

			nnResult ServiceCall(uint32 serviceId, void* request, void* response) override
			{
				cemuLog_log(LogType::Force, "Unsupported service call to /dev/act");
				cemu_assert_unimplemented();
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_ACT, 0);
			}
		};

		ActService gActService;

		void Initialize()
		{
			gActService.Start();
		}

		void Stop()
		{
			gActService.Stop();
		}
	}
}


// IOSU act IO

typedef struct  
{
	/* +0x00 */ uint32be ukn00;
	/* +0x04 */ uint32be ukn04;
	/* +0x08 */ uint32be ukn08;
	/* +0x0C */ uint32be subcommandCode;
	/* +0x10 */ uint8 ukn10;
	/* +0x11 */ uint8 ukn11;
	/* +0x12 */ uint8 ukn12;
	/* +0x13 */ uint8 accountSlot;
	/* +0x14 */ uint32be unique; // is this command specific?
}cmdActRequest00_t;

typedef struct  
{
	uint32be returnCode;
	uint8 transferableIdBase[8];
}cmdActGetTransferableIDResult_t;

#define ACT_SUBCMD_GET_TRANSFERABLE_ID		4
#define ACT_SUBCMD_INITIALIZE				0x14

#define _cancelIfAccountDoesNotExist() \
if (_actAccountData[accountIndex].isValid == false) \
{ \
	/* account does not exist*/  \
	ioctlReturnValue = 0; \
	actCemuRequest->setACTReturnCode(BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_ACT, NN_ACT_RESULT_ACCOUNT_DOES_NOT_EXIST)); /* 0xA071F480 */ \
	actCemuRequest->resultU64.u64 = 0; \
	iosuIoctl_completeRequest(ioQueueEntry, ioctlReturnValue); \
	continue; \
}

nnResult ServerActErrorCodeToNNResult(NAPI::ACT_ERROR_CODE ec)
{
	switch (ec)
	{
	case (NAPI::ACT_ERROR_CODE)1:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2401);
	case (NAPI::ACT_ERROR_CODE)2:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2402);
	case (NAPI::ACT_ERROR_CODE)3:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2403);
	case (NAPI::ACT_ERROR_CODE)4:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2458);
	case (NAPI::ACT_ERROR_CODE)5:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2642);
	case (NAPI::ACT_ERROR_CODE)6:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2641);
	case (NAPI::ACT_ERROR_CODE)7:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2522);
	case (NAPI::ACT_ERROR_CODE)8:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2534);
	case (NAPI::ACT_ERROR_CODE)9:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2404);
	case (NAPI::ACT_ERROR_CODE)10:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2451);
	case (NAPI::ACT_ERROR_CODE)11:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2511);
	case (NAPI::ACT_ERROR_CODE)12:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2812);
	case (NAPI::ACT_ERROR_CODE)100:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2571);
	case (NAPI::ACT_ERROR_CODE)101:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2572);
	case (NAPI::ACT_ERROR_CODE)103:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2575);
	case (NAPI::ACT_ERROR_CODE)104:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2452);
	case (NAPI::ACT_ERROR_CODE)105:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2592);
	case (NAPI::ACT_ERROR_CODE)106:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2611);
	case (NAPI::ACT_ERROR_CODE)107:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2502);
	case (NAPI::ACT_ERROR_CODE)108:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2802);
	case (NAPI::ACT_ERROR_CODE)109:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2503);
	case (NAPI::ACT_ERROR_CODE)110:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2501);
	case (NAPI::ACT_ERROR_CODE)111:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2632);
	case (NAPI::ACT_ERROR_CODE)112:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2631);
	case (NAPI::ACT_ERROR_CODE)113:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2452);
	case (NAPI::ACT_ERROR_CODE)114:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2593);
	case (NAPI::ACT_ERROR_CODE)115:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2591);
	case (NAPI::ACT_ERROR_CODE)116:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2614);
	case (NAPI::ACT_ERROR_CODE)117:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2651);
	case (NAPI::ACT_ERROR_CODE)118:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2484);
	case (NAPI::ACT_ERROR_CODE)119:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2803);
	case (NAPI::ACT_ERROR_CODE)120:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2813);
	case (NAPI::ACT_ERROR_CODE)121:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2804);
	case (NAPI::ACT_ERROR_CODE)122:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2814);
	case (NAPI::ACT_ERROR_CODE)123:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2882);
	case (NAPI::ACT_ERROR_CODE)124:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2512);
	case (NAPI::ACT_ERROR_CODE)125:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2485);
	case (NAPI::ACT_ERROR_CODE)126:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2805);
	case (NAPI::ACT_ERROR_CODE)127:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2815);
	case (NAPI::ACT_ERROR_CODE)128:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2661);
	case (NAPI::ACT_ERROR_CODE)129:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2615);
	case (NAPI::ACT_ERROR_CODE)130:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2531);
	case (NAPI::ACT_ERROR_CODE)131:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2616);
	case (NAPI::ACT_ERROR_CODE)132:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2822);
	case (NAPI::ACT_ERROR_CODE)133:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2832);
	case (NAPI::ACT_ERROR_CODE)134:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2823);
	case (NAPI::ACT_ERROR_CODE)135:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2833);
	case (NAPI::ACT_ERROR_CODE)136:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2824);
	case (NAPI::ACT_ERROR_CODE)137:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2834);
	case (NAPI::ACT_ERROR_CODE)138:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2825);
	case (NAPI::ACT_ERROR_CODE)139:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2835);
	case (NAPI::ACT_ERROR_CODE)142:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2635);
	case (NAPI::ACT_ERROR_CODE)143:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2634);
	case (NAPI::ACT_ERROR_CODE)1004:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2503);
	case (NAPI::ACT_ERROR_CODE)1006:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2471);
	case (NAPI::ACT_ERROR_CODE)1016:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2532);
	case (NAPI::ACT_ERROR_CODE)1017:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2483);
	case (NAPI::ACT_ERROR_CODE)1018:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2533);
	case (NAPI::ACT_ERROR_CODE)1019:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2481);
	case (NAPI::ACT_ERROR_CODE)1020:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2473);
	case NAPI::ACT_ERROR_CODE::ACT_GAME_SERVER_NOT_FOUND:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2482);
	case (NAPI::ACT_ERROR_CODE)1022:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2472);
	case (NAPI::ACT_ERROR_CODE)1023:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2612);
	case (NAPI::ACT_ERROR_CODE)1024:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2535);
	case (NAPI::ACT_ERROR_CODE)1025:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2536);
	case (NAPI::ACT_ERROR_CODE)1031:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2537);
	case (NAPI::ACT_ERROR_CODE)1032:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2636);
	case (NAPI::ACT_ERROR_CODE)1033:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2662);
	case (NAPI::ACT_ERROR_CODE)1035:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2671);
	case (NAPI::ACT_ERROR_CODE)1036:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2679);
	case (NAPI::ACT_ERROR_CODE)1037:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2672);
	case (NAPI::ACT_ERROR_CODE)1038:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2674);
	case (NAPI::ACT_ERROR_CODE)1039:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2680);
	case (NAPI::ACT_ERROR_CODE)1040:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2675);
	case (NAPI::ACT_ERROR_CODE)1041:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2673);
	case (NAPI::ACT_ERROR_CODE)1042:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2676);
	case (NAPI::ACT_ERROR_CODE)1043:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2681);
	case (NAPI::ACT_ERROR_CODE)1044:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2678);
	case (NAPI::ACT_ERROR_CODE)1045:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2677);
	case (NAPI::ACT_ERROR_CODE)1046:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2596);
	case (NAPI::ACT_ERROR_CODE)1100:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2541);
	case (NAPI::ACT_ERROR_CODE)1101:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2542);
	case (NAPI::ACT_ERROR_CODE)1103:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2594);
	case (NAPI::ACT_ERROR_CODE)1104:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2576);
	case (NAPI::ACT_ERROR_CODE)1105:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2613);
	case (NAPI::ACT_ERROR_CODE)1106:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2633);
	case (NAPI::ACT_ERROR_CODE)1107:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2577);
	case (NAPI::ACT_ERROR_CODE)1111:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2538);
	case (NAPI::ACT_ERROR_CODE)1115:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2597);
	case (NAPI::ACT_ERROR_CODE)1125:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2585);
	case (NAPI::ACT_ERROR_CODE)1126:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2586);
	case (NAPI::ACT_ERROR_CODE)1134:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2587);
	case (NAPI::ACT_ERROR_CODE)1200:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2884);
	case (NAPI::ACT_ERROR_CODE)2001:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2931);
	case (NAPI::ACT_ERROR_CODE)2002:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2881);
	case (NAPI::ACT_ERROR_CODE)2999:
		return nnResultStatus(NN_RESULT_MODULE_NN_ACT, (NN_ERROR_CODE)2883);
	default:
		break;
	}
	cemuLog_log(LogType::Force, "Received unknown ACT error code {}", (uint32)ec);
	return nnResultStatus(NN_RESULT_MODULE_NN_ACT, NN_ERROR_CODE::ACT_UNKNOWN_SERVER_ERROR);
}

int iosuAct_thread()
{
	SetThreadName("iosuAct_thread");
	while (true)
	{
		uint32 ioctlReturnValue = 0;
		ioQueueEntry_t* ioQueueEntry = iosuIoctl_getNextWithWait(IOS_DEVICE_ACT);
		if (ioQueueEntry->request == 0)
		{
			if (ioQueueEntry->countIn != 1 || ioQueueEntry->countOut != 1)
			{
				assert_dbg();
			}
			ioBufferVector_t* vectorsDebug = ioQueueEntry->bufferVectors.GetPtr();

			void* outputBuffer = ioQueueEntry->bufferVectors[0].buffer.GetPtr();
			cmdActRequest00_t* requestCmd = (cmdActRequest00_t*)ioQueueEntry->bufferVectors[0].unknownBuffer.GetPtr();
			if (requestCmd->subcommandCode == ACT_SUBCMD_INITIALIZE)
			{
				// do nothing for now (there is no result?)
			}
			else if (requestCmd->subcommandCode == ACT_SUBCMD_GET_TRANSFERABLE_ID)
			{
				cmdActGetTransferableIDResult_t* cmdResult = (cmdActGetTransferableIDResult_t*)outputBuffer;
				cmdResult->returnCode = 0;
				*(uint64*)cmdResult->transferableIdBase = _swapEndianU64(0x1122334455667788);
			}
			else
				assert_dbg();
		}
		else if (ioQueueEntry->request == IOSU_ACT_REQUEST_CEMU)
		{
			iosuActCemuRequest_t* actCemuRequest = (iosuActCemuRequest_t*)ioQueueEntry->bufferVectors[0].buffer.GetPtr();
			sint32 accountIndex;
			ioctlReturnValue = 0;
			if (actCemuRequest->requestCode == IOSU_ARC_ACCOUNT_ID)
			{
				accountIndex = iosuAct_getAccountIndexBySlot(actCemuRequest->accountSlot);
				_cancelIfAccountDoesNotExist();
				strcpy(actCemuRequest->resultString.strBuffer, _actAccountData[accountIndex].accountId);
				actCemuRequest->setACTReturnCode(0);
			}
			else if (actCemuRequest->requestCode == IOSU_ARC_UUID)
			{
				accountIndex = iosuAct_getAccountIndexBySlot(actCemuRequest->accountSlot);
				if (actCemuRequest->accountSlot == 0xFF)
				{
					// common uuid (placeholder algorithm)
					for (uint32 i = 0; i < 16; i++)
						actCemuRequest->resultBinary.binBuffer[i] = i * 0x74 + i + ~i + i * 133;
				}
				else
				{
					_cancelIfAccountDoesNotExist();
					memcpy(actCemuRequest->resultBinary.binBuffer, _actAccountData[accountIndex].uuid, 16);
				}

				cemu_assert_debug(actCemuRequest->uuidName != -1); // todo
				if (actCemuRequest->uuidName != -1 && actCemuRequest->uuidName != -2)
				{
					// generate name based UUID
					// format:
					// first 10 bytes of UUID + 6 bytes of a hash
					// hash algorithm:
					// sha256 of
					//   4 bytes uuidName (big-endian)
					//   4 bytes 0x3A275E09 (big-endian)
					//   6 bytes from the end of UUID
					// bytes 10-15 are used from the hash and replace the last 6 bytes of the UUID

					EVP_MD_CTX *ctx_sha256 = EVP_MD_CTX_new();
					EVP_DigestInit(ctx_sha256, EVP_sha256());

					uint32 name = (uint32)actCemuRequest->uuidName;
					uint8 tempArray[] = {
						static_cast<uint8>((name >> 24) & 0xFF),
						static_cast<uint8>((name >> 16) & 0xFF),
						static_cast<uint8>((name >> 8) & 0xFF),
						static_cast<uint8>((name >> 0) & 0xFF),
						0x3A,
						0x27,
						0x5E,
						0x09,
					};
					EVP_DigestUpdate(ctx_sha256, tempArray, sizeof(tempArray));
					EVP_DigestUpdate(ctx_sha256, actCemuRequest->resultBinary.binBuffer+10, 6);
					uint8 h[SHA256_DIGEST_LENGTH];
					EVP_DigestFinal_ex(ctx_sha256, h, NULL);
					EVP_MD_CTX_free(ctx_sha256);

					memcpy(actCemuRequest->resultBinary.binBuffer + 0xA, h + 0xA, 6);
				}
				else if (actCemuRequest->uuidName == -2)
				{
					// return account uuid
				}
				else
				{
					cemuLog_logDebug(LogType::Force, "Gen UUID unknown mode {}", actCemuRequest->uuidName);
				}
				actCemuRequest->setACTReturnCode(0);
			}
			else if (actCemuRequest->requestCode == IOSU_ARC_SIMPLEADDRESS)
			{
				accountIndex = iosuAct_getAccountIndexBySlot(actCemuRequest->accountSlot);
				_cancelIfAccountDoesNotExist();
				actCemuRequest->resultU32.u32 = _actAccountData[accountIndex].simpleAddressId;
				actCemuRequest->setACTReturnCode(0);
			}
			else if (actCemuRequest->requestCode == IOSU_ARC_PRINCIPALID)
			{
				accountIndex = iosuAct_getAccountIndexBySlot(actCemuRequest->accountSlot);
				_cancelIfAccountDoesNotExist();
				actCemuRequest->resultU32.u32 = _actAccountData[accountIndex].principalId;
				actCemuRequest->setACTReturnCode(0);
			}
			else if (actCemuRequest->requestCode == IOSU_ARC_TRANSFERABLEID)
			{
				accountIndex = iosuAct_getAccountIndexBySlot(actCemuRequest->accountSlot);
				_cancelIfAccountDoesNotExist();
				actCemuRequest->resultU64.u64 = _actAccountData[accountIndex].transferableIdBase;
				// todo - transferable also contains a unique id
				actCemuRequest->setACTReturnCode(0);
			}
			else if (actCemuRequest->requestCode == IOSU_ARC_PERSISTENTID)
			{
				if(actCemuRequest->accountSlot != 0)
				{
					accountIndex = iosuAct_getAccountIndexBySlot(actCemuRequest->accountSlot);
					_cancelIfAccountDoesNotExist();
					actCemuRequest->resultU32.u32 = _actAccountData[accountIndex].persistentId;
					actCemuRequest->setACTReturnCode(0);
				}
				else
				{
					// F1 Race Stars calls IsSlotOccupied and indirectly GetPersistentId on slot 0 which is not valid
					actCemuRequest->resultU32.u32 = 0;
					actCemuRequest->setACTReturnCode(0);
				}
			}
			else if (actCemuRequest->requestCode == IOSU_ARC_COUNTRY)
			{
				accountIndex = iosuAct_getAccountIndexBySlot(actCemuRequest->accountSlot);
				_cancelIfAccountDoesNotExist();
				strcpy(actCemuRequest->resultString.strBuffer, _actAccountData[accountIndex].country);
				actCemuRequest->setACTReturnCode(0);
			}
			else if (actCemuRequest->requestCode == IOSU_ARC_ISNETWORKACCOUNT)
			{
				accountIndex = iosuAct_getAccountIndexBySlot(actCemuRequest->accountSlot);
				_cancelIfAccountDoesNotExist();
				actCemuRequest->resultU32.u32 = _actAccountData[accountIndex].isNetworkAccount;
				actCemuRequest->setACTReturnCode(0);
			}
			else if (actCemuRequest->requestCode == IOSU_ARC_ACQUIRENEXTOKEN)
			{
				NAPI::AuthInfo authInfo;
				NAPI::NAPI_MakeAuthInfoFromCurrentAccount(authInfo);
				NAPI::ACTGetNexTokenResult nexTokenResult = NAPI::ACT_GetNexToken_WithCache(authInfo, actCemuRequest->titleId, actCemuRequest->titleVersion, actCemuRequest->serverId);
				uint32 returnCode = 0;
				if (nexTokenResult.isValid())
				{
					*(NAPI::ACTNexToken*)actCemuRequest->resultBinary.binBuffer = nexTokenResult.nexToken;
					returnCode = NN_RESULT_SUCCESS;
				}
				else if (nexTokenResult.apiError == NAPI_RESULT::SERVICE_ERROR)
				{
					returnCode = ServerActErrorCodeToNNResult(nexTokenResult.serviceError);
					cemu_assert_debug((returnCode&0x80000000) != 0);
				}
				else
				{
					returnCode = nnResultStatus(NN_RESULT_MODULE_NN_ACT, NN_ERROR_CODE::ACT_UNKNOWN_SERVER_ERROR);
				}				
				actCemuRequest->setACTReturnCode(returnCode);
			}
			else if (actCemuRequest->requestCode == IOSU_ARC_ACQUIREINDEPENDENTTOKEN)
			{
				NAPI::AuthInfo authInfo;
				NAPI::NAPI_MakeAuthInfoFromCurrentAccount(authInfo);
				NAPI::ACTGetIndependentTokenResult tokenResult = NAPI::ACT_GetIndependentToken_WithCache(authInfo, actCemuRequest->titleId, actCemuRequest->titleVersion, actCemuRequest->clientId);

				uint32 returnCode = 0;
				if (tokenResult.isValid())
				{
					for (size_t i = 0; i < std::min(tokenResult.token.size(), (size_t)200); i++)
					{
						actCemuRequest->resultBinary.binBuffer[i] = tokenResult.token[i];
						actCemuRequest->resultBinary.binBuffer[i + 1] = '\0';
					}
					returnCode = 0;
				}
				else
				{
					returnCode = 0x80000000; // todo - proper error codes
				}
				actCemuRequest->setACTReturnCode(returnCode);
			}
			else if (actCemuRequest->requestCode == IOSU_ARC_ACQUIREPIDBYNNID)
			{				
				uint32 returnCode = iosuAct_acquirePrincipalIdByAccountId(actCemuRequest->clientId, &actCemuRequest->resultU32.u32);
				actCemuRequest->setACTReturnCode(returnCode);
			}
			else if (actCemuRequest->requestCode == IOSU_ARC_MIIDATA)
			{

				accountIndex = iosuAct_getAccountIndexBySlot(actCemuRequest->accountSlot);
				_cancelIfAccountDoesNotExist();
				memcpy(actCemuRequest->resultBinary.binBuffer, &_actAccountData[accountIndex].miiData, sizeof(FFLData_t));
				actCemuRequest->setACTReturnCode(0);
			}
			else if (actCemuRequest->requestCode == IOSU_ARC_INIT)
			{
				iosuAct_loadAccounts();
				actCemuRequest->setACTReturnCode(0);
			}
			else
				assert_dbg();
		}
		else
		{
			assert_dbg();
		}
		iosuIoctl_completeRequest(ioQueueEntry, ioctlReturnValue);
	}
	return 0;
}

void iosuAct_init_depr()
{
	if (iosuAct.isInitialized)
		return;
	std::thread t(iosuAct_thread);
	t.detach();
	iosuAct.isInitialized = true;
}

bool iosuAct_isInitialized()
{
	return iosuAct.isInitialized;
}

uint16 FFLCalculateCRC16(uint8* input, sint32 length)
{
	uint16 crc = 0;
	for (sint32 c = 0; c < length; c++)
	{
		for (sint32 f = 0; f < 8; f++)
		{
			if ((crc & 0x8000) != 0)
			{
				uint16 t = crc << 1;
				crc = t ^ 0x1021;
			}
			else
			{
				crc <<= 1;
			}
		}
		crc ^= (uint16)input[c];
	}
	return crc;
}
