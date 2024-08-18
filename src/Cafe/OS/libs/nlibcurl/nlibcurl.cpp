#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/common/OSUtil.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "nlibcurl.h"

#include "openssl/bn.h"
#include "openssl/x509.h"
#include "openssl/ssl.h"

#define CURL_STRICTER

#include "curl/curl.h"
#include <unordered_map>
#include <atomic>

#include "Cafe/IOSU/legacy/iosu_crypto.h"
#include "Cafe/OS/libs/nsysnet/nsysnet.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM.h"

#include "util/helpers/ConcurrentQueue.h"
#include "Cafe/OS/common/PPCConcurrentQueue.h"
#include "Common/FileStream.h"
#include "config/ActiveSettings.h"

namespace nlibcurl
{
#define CURL_MULTI_HANDLE		(0x000bab1e)

#define NSSL_VERIFY_NONE		(0x0)
#define NSSL_VERIFY_PEER		(1<<0)
#define NSSL_VERIFY_HOSTNAME	(1<<1)
#define NSSL_VERIFY_DATE		(1<<2)

	enum QueueOrder
	{
		QueueOrder_None,

		QueueOrder_Result,
		QueueOrder_CBDone,

		QueueOrder_HeaderCB,
		QueueOrder_ReadCB,
		QueueOrder_WriteCB,
		QueueOrder_ProgressCB,

		QueueOrder_Perform,
		QueueOrder_Pause,
	};

	struct QueueMsg_t
	{
		QueueOrder order;
		union
		{
			uint32 result;

			struct
			{
				char* buffer;
				uint32 size;
				uint32 nitems;
			} header_cb;

			struct
			{
				char* buffer;
				uint32 size;
				uint32 nitems;
			} read_cb;

			struct
			{
				char* buffer;
				uint32 size;
				uint32 nmemb;
			} write_cb;

			struct
			{
				uint32 clientp;
				double dltotal;
				double dlnow;
				double ultotal;
				double ulnow;
			} progress_cb;

			struct
			{
				sint32 bitmask;
			} pause;

		};

	};

struct MEMPTRHash_t
{
	size_t operator()(const MEMPTR<void>& p) const
	{
		return p.GetMPTR();
	}
};

struct WU_curl_slist
{
	MEMPTR<char> data;
	MEMPTR<WU_curl_slist> next;
};

enum class WU_CURLcode
{
	placeholder = 0,
};

struct
{
	sint32 initialized;

	MEMPTR<void> proxyConfig;
	MEMPTR<curl_malloc_callback> malloc;
	MEMPTR<curl_free_callback> free;
	MEMPTR<curl_realloc_callback> realloc;
	MEMPTR<curl_strdup_callback> strdup;
	MEMPTR<curl_calloc_callback> calloc;
} g_nlibcurl = {};

using WU_CURL_off_t = uint64be;

enum class WU_HTTPREQ : uint32
{
	HTTPREQ_GET = 0x1,
	HTTPREQ_POST = 0x2,
	UKN_3 = 0x3,
};

struct WU_UserDefined
{
	// starting at 0xD8 (probably) in CURL_t
	/* 0x0D8 / +0x00 */ uint32be ukn0D8;
	/* 0x0DC / +0x04 */ uint32be ukn0DC;
	/* 0x0E0 / +0x08 */ MEMPTR<WU_curl_slist> headers;
	/* 0x0E4 / +0x0C */ uint32be ukn0E4;
	/* 0x0E8 / +0x10 */ uint32be ukn0E8;
	/* 0x0EC / +0x14 */ uint32be ukn0EC;
	/* 0x0F0 / +0x18 */ uint32be ukn0F0[4];
	/* 0x100 / +0x28 */ uint32be ukn100[4];
	/* 0x110 / +0x38 */ uint32be ukn110[4]; // +0x40 -> WU_CURL_off_t postfieldsize ?
	/* 0x120 / +0x48 */ uint32be ukn120[4];
	/* 0x130 / +0x58 */ uint32be ukn130[4];
	/* 0x140 / +0x68 */ uint32be ukn140[4];
	/* 0x150 / +0x78 */ uint32be ukn150[4];
	/* 0x160 / +0x88 */ uint32be ukn160[4];
	/* 0x170 / +0x98 */ uint32be ukn170[4];
	/* 0x180 / +0xA8 */ uint32be ukn180[4];
	/* 0x190 / +0xB0 */ sint64be infilesize_190{0};
	/* 0x198 / +0xB8 */ uint32be ukn198;
	/* 0x19C / +0xBC */ uint32be ukn19C;
	/* 0x1A0 / +0xC8 */ uint32be ukn1A0[4];
	/* 0x1B0 / +0xD8 */ uint32be ukn1B0[4];
	/* 0x1C0 / +0xE8 */ uint32be ukn1C0[4];
	/* 0x1D0 / +0xF8 */ uint32be ukn1D0[4];
	/* 0x1E0 / +0x108 */ uint32be ukn1E0;
	/* 0x1E4 / +0x108 */ uint32be ukn1E4;
	/* 0x1E8 / +0x108 */ uint32be ukn1E8;
	/* 0x1EC / +0x108 */ betype<WU_HTTPREQ> httpreq_1EC;
	/* 0x1F0 / +0x118 */ uint32be ukn1F0[4];

	void SetToDefault()
	{
		memset(this, 0, sizeof(WU_UserDefined));
		httpreq_1EC = WU_HTTPREQ::HTTPREQ_GET;
	}
};

struct CURL_t
{
	CURL* curl;

	uint32be hNSSL;
	uint32be nsslVerifyOptions;
	MEMPTR<void> out; // CURLOPT_WRITEDATA
	MEMPTR<void> in_set; // CURLOPT_READDATA
	MEMPTR<void> writeheader; // CURLOPT_HEADERDATA

	MEMPTR<void> fwrite_func; // CURLOPT_WRITEFUNCTION
	MEMPTR<void> fwrite_header; // CURLOPT_HEADERFUNCTION
	MEMPTR<void> fread_func_set; // CURLOPT_READFUNCTION

	MEMPTR<void> progress_client; // CURLOPT_PROGRESSDATA
	MEMPTR<void> fprogress; // CURLOPT_PROGRESSFUNCTION

	MEMPTR<void> fsockopt; // CURLOPT_SOCKOPTFUNCTION
	MEMPTR<void> sockopt_client; // CURLOPT_SOCKOPTDATA

	// Cemu specific
	OSThread_t* curlThread;
	MEMPTR<char> info_redirectUrl; // stores CURLINFO_REDIRECT_URL ptr
	MEMPTR<char> info_contentType; // stores CURLINFO_CONTENT_TYPE ptr
	bool isDirty{true};

	// debug
	struct  
	{
		uint32 activeRequestIndex{};
		uint32 responseRequestIndex{}; // set to activeRequestIndex once perform is called
		bool hasDumpedResultInfo{}; // set once the response info has been dumped (reset when next request is performed)
		FileStream* file_requestParam{};
		FileStream* file_responseHeaders{};
		FileStream* file_responseRaw{};
	}debug;

	// fields below match the actual memory layout, above still needs refactoring
	/* 0x78 */ uint32be ukn078;
	/* 0x7C */ uint32be ukn07C;
	/* 0x80 */ uint32be ukn080;
	/* 0x84 */ uint32be ukn084;
	/* 0x88 */ uint32be ukn088;
	/* 0x8C */ uint32be ukn08C;
	/* 0x90 */ uint32be ukn090[4];
	/* 0xA0 */ uint32be ukn0A0[4];
	/* 0xB0 */ uint32be ukn0B0[4];
	/* 0xC0 */ uint32be ukn0C0[4];
	/* 0xD0 */ uint32be ukn0D0;
	/* 0xD4 */ uint32be ukn0D4;
	/* 0xD8 */ WU_UserDefined set;
	/* 0x200 */ uint32be ukn200[4];
	/* 0x210 */ uint32be ukn210[4];
	/* 0x220 */ uint32be ukn220[4];
	/* 0x230 */ uint32be ukn230[4];
	/* 0x240 */ uint32be ukn240[4];
	/* 0x250 */ uint32be ukn250[4];
	/* 0x260 */ uint32be ukn260[4];
	/* 0x270 */ uint32be ukn270[4];
	/* 0x280 */ uint8be ukn280;
	/* 0x281 */ uint8be opt_no_body_281;
	/* 0x282 */ uint8be ukn282;
	/* 0x283 */ uint8be upload_283;
};
static_assert(sizeof(CURL_t) <= 0x8698);
static_assert(offsetof(CURL_t, ukn078) == 0x78);
static_assert(offsetof(CURL_t, set) == 0xD8);
static_assert(offsetof(CURL_t, set) + offsetof(WU_UserDefined, headers) == 0xE0);
static_assert(offsetof(CURL_t, set) + offsetof(WU_UserDefined, infilesize_190) == 0x190);
static_assert(offsetof(CURL_t, set) + offsetof(WU_UserDefined, httpreq_1EC) == 0x1EC);
static_assert(offsetof(CURL_t, opt_no_body_281) == 0x281);
typedef MEMPTR<CURL_t> CURLPtr;

#pragma pack(1) // may affect structs below, we can probably remove this but lets keep it for now as the code below is fragile

typedef struct
{
	//uint32be specifier; // 0x00
	//uint32be dirty; // 0x04
	MEMPTR<void> lockfunc; // 0x08
	MEMPTR<void> unlockfunc; // 0x0C
	MEMPTR<void> data; // 0x10
	//MEMPTR<void> uk14; // 0x14
	//MEMPTR<void> uk18; // 0x18

	CURLSH* curlsh;
	MEMPTR <CURL_t> curl;
	uint32 uk1;
}CURLSH_t; // SCURL
static_assert(sizeof(CURLSH_t) <= 0x1C);
typedef MEMPTR<CURLSH_t> CURLSHPtr;

typedef struct
{
	CURLM* curlm;
	std::vector<MEMPTR<CURL_t>> curl;
}CURLM_t;
static_assert(sizeof(CURLM_t) <= 0x80, "sizeof(CURLM_t)");
typedef MEMPTR<CURLM_t> CURLMPtr;

static_assert(sizeof(WU_curl_slist) <= 0x8, "sizeof(curl_slist_t)");

struct CURLMsg_t
{
	uint32be msg;
	MEMPTR<CURL_t> curl;
	uint32be result; // CURLcode
};

static_assert(sizeof(CURLMsg_t) <= 0xC, "sizeof(CURLMsg_t)");

#pragma pack()

#include "nlibcurlDebug.hpp"

size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata);

thread_local PPCConcurrentQueue<QueueMsg_t>* g_callerQueue;
thread_local ConcurrentQueue<QueueMsg_t>* g_threadQueue;
void CurlWorkerThread(CURL_t* curl, PPCConcurrentQueue<QueueMsg_t>* callerQueue, ConcurrentQueue<QueueMsg_t>* threadQueue)
{
	g_callerQueue = callerQueue;
	g_threadQueue = threadQueue;

	const QueueMsg_t msg = threadQueue->pop();

	QueueMsg_t resultMsg = {};
	resultMsg.order = QueueOrder_Result;

	if (msg.order == QueueOrder_Perform)
		resultMsg.result = ::curl_easy_perform(curl->curl);
	else if(msg.order == QueueOrder_Pause)
		resultMsg.result = ::curl_easy_pause(curl->curl, msg.pause.bitmask);
	else
		assert_dbg();

	callerQueue->push(resultMsg, curl->curlThread);
}

uint32 SendOrderToWorker(CURL_t* curl, QueueOrder order, uint32 arg1 = 0)
{
	OSThread_t* currentThread = coreinit::OSGetCurrentThread();
	curl->curlThread = currentThread;

	// cemuLog_logDebug(LogType::Force, "CURRENTTHREAD: 0x{} -> {}",currentThread, order)

	PPCConcurrentQueue<QueueMsg_t> callerQueue;
	ConcurrentQueue<QueueMsg_t> threadQueue;
	std::thread worker(CurlWorkerThread, curl, &callerQueue, &threadQueue);
	worker.detach();

	QueueMsg_t orderMsg = {};
	orderMsg.order = order;

	if (order == QueueOrder_Pause)
		orderMsg.pause.bitmask = arg1;

	threadQueue.push(orderMsg);

	uint32 result;
	while (true)
	{
		const QueueMsg_t msg = callerQueue.pop(currentThread);
		if (msg.order == QueueOrder_ProgressCB)
		{
			QueueMsg_t sendMsg = {};
			sendMsg.order = QueueOrder_CBDone;
			sendMsg.result = PPCCoreCallback(curl->fprogress.GetMPTR(), curl->progress_client.GetMPTR(), msg.progress_cb.dltotal, msg.progress_cb.dlnow, msg.progress_cb.ultotal, msg.progress_cb.ulnow);
			threadQueue.push(sendMsg);
		}
		else if (msg.order == QueueOrder_HeaderCB)
		{
			StackAllocator<char> tmp(msg.header_cb.size * msg.header_cb.nitems);
			memcpy(tmp.GetPointer(), msg.header_cb.buffer, msg.header_cb.size * msg.header_cb.nitems);

			QueueMsg_t sendMsg = {};
			sendMsg.order = QueueOrder_CBDone;
			sendMsg.result = PPCCoreCallback(curl->fwrite_header.GetMPTR(), tmp.GetMPTR(), msg.header_cb.size, msg.header_cb.nitems, curl->writeheader.GetMPTR());
			threadQueue.push(sendMsg);
		}
		else if (msg.order == QueueOrder_ReadCB)
		{
			StackAllocator<char> tmp(msg.read_cb.size * msg.read_cb.nitems);

			QueueMsg_t sendMsg = {};
			sendMsg.order = QueueOrder_CBDone;
			cemuLog_logDebug(LogType::Force, "QueueOrder_ReadCB size: {} nitems: {}", msg.read_cb.size, msg.read_cb.nitems);
			sendMsg.result = PPCCoreCallback(curl->fread_func_set.GetMPTR(), tmp.GetMPTR(), msg.read_cb.size, msg.read_cb.nitems, curl->in_set.GetMPTR());
			cemuLog_logDebug(LogType::Force, "readcb size: {}", (sint32)sendMsg.result);
			if (sendMsg.result > 0)
				memcpy(msg.read_cb.buffer, tmp.GetPointer(), sendMsg.result);

			threadQueue.push(sendMsg);
		}
		else if (msg.order == QueueOrder_WriteCB)
		{
			StackAllocator<char> tmp(msg.write_cb.size * msg.write_cb.nmemb);
			memcpy(tmp.GetPointer(), msg.write_cb.buffer, msg.write_cb.size * msg.write_cb.nmemb);

			QueueMsg_t sendMsg = {};
			sendMsg.order = QueueOrder_CBDone;
			sendMsg.result = PPCCoreCallback(curl->fwrite_func.GetMPTR(), tmp.GetMPTR(), msg.write_cb.size, msg.write_cb.nmemb, curl->out.GetMPTR());
			threadQueue.push(sendMsg);
		}
		else if (msg.order == QueueOrder_Result)
		{
			result = msg.result;
			break;
		}
	}

	return result;
}

static int curl_closesocket(void *clientp, curl_socket_t item)
{
	nsysnet_notifyCloseSharedSocket((SOCKET)item);
	closesocket(item);
	return 0;
}

void _curl_set_default_parameters(CURL_t* curl)
{
	curl->set.SetToDefault();

	// default parameters
	curl_easy_setopt(curl->curl, CURLOPT_HEADERFUNCTION, header_callback);
	curl_easy_setopt(curl->curl, CURLOPT_HEADERDATA, curl);

	curl_easy_setopt(curl->curl, CURLOPT_CLOSESOCKETFUNCTION, curl_closesocket);
	curl_easy_setopt(curl->curl, CURLOPT_CLOSESOCKETDATA, nullptr);
}

void _curl_sync_parameters(CURL_t* curl)
{
	// sync ppc curl to actual curl state
	// not all parameters are covered yet, many are still set directly in easy_setopt
	bool isPost = curl->set.httpreq_1EC == WU_HTTPREQ::HTTPREQ_POST;
	// http request type
	if(curl->set.httpreq_1EC == WU_HTTPREQ::HTTPREQ_GET)
	{
		::curl_easy_setopt(curl->curl, CURLOPT_HTTPGET, 1);
		cemu_assert_debug(curl->opt_no_body_281 == 0);
		cemu_assert_debug(curl->upload_283 == 0);
	}
	else if(curl->set.httpreq_1EC == WU_HTTPREQ::HTTPREQ_POST)
	{
		::curl_easy_setopt(curl->curl, CURLOPT_POST, 1);
		cemu_assert_debug(curl->upload_283 == 0);
		::curl_easy_setopt(curl->curl, CURLOPT_NOBODY, curl->opt_no_body_281 ? 1 : 0);
	}
	else
	{
		cemu_assert_unimplemented();
	}

	// CURLOPT_HTTPHEADER
	std::optional<uint64> manualHeaderContentLength;
	if (curl->set.headers)
	{
		struct curl_slist* list = nullptr;
		WU_curl_slist* ppcList = curl->set.headers;
		while(ppcList)
		{
			if(isPost)
			{
				// for recent libcurl manually adding Content-Length header is undefined behavior. Instead CURLOPT_INFILESIZE(_LARGE) should be set
				// here we remove Content-Length and instead substitute it with CURLOPT_INFILESIZE (NEX DataStore in Super Mario Maker requires this)
				if(strncmp(ppcList->data.GetPtr(), "Content-Length:", 15) == 0)
				{
					manualHeaderContentLength = std::stoull(ppcList->data.GetPtr() + 15);
					ppcList = ppcList->next;
					continue;
				}
			}

			cemuLog_logDebug(LogType::Force, "curl_slist_append: {}", ppcList->data.GetPtr());
			curlDebug_logEasySetOptStr(curl, "CURLOPT_HTTPHEADER", (const char*)ppcList->data.GetPtr());
			list = ::curl_slist_append(list, ppcList->data.GetPtr());
			ppcList = ppcList->next;
		}
		::curl_easy_setopt(curl->curl, CURLOPT_HTTPHEADER, list);
		// todo - prevent leaking of list (maybe store in host curl object, similar to how our zlib implementation does stuff)
	}
	else
		::curl_easy_setopt(curl->curl, CURLOPT_HTTPHEADER, nullptr);

	// infile size (post data size)
	if (curl->set.infilesize_190)
	{
		cemu_assert_debug(manualHeaderContentLength == 0); // should not have both?
		::curl_easy_setopt(curl->curl, CURLOPT_INFILESIZE_LARGE, curl->set.infilesize_190);
	}
	else
	{
		if(isPost && manualHeaderContentLength > 0)
			::curl_easy_setopt(curl->curl, CURLOPT_INFILESIZE_LARGE, manualHeaderContentLength);
		else
			::curl_easy_setopt(curl->curl, CURLOPT_INFILESIZE_LARGE, 0);
	}
}

void export_malloc(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(size, 0);
	MPTR memAddr = PPCCoreCallback(gCoreinitData->MEMAllocFromDefaultHeap, size);
	osLib_returnFromFunction(hCPU, memAddr);
}

void export_calloc(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(count, 0);
	ppcDefineParamU32(size, 1);

	MPTR memAddr = PPCCoreCallback(gCoreinitData->MEMAllocFromDefaultHeap, count*size);

	osLib_returnFromFunction(hCPU, memAddr);
}
void export_free(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(mem, void, 0);
	PPCCoreCallback(gCoreinitData->MEMFreeToDefaultHeap, mem.GetMPTR());

	osLib_returnFromFunction(hCPU, 0);
}
void export_strdup(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(str, const char, 0);
	int len = (int)strlen(str.GetPtr()) + 1;
	MEMPTR<char> result = (char*)coreinit::default_MEMAllocFromDefaultHeap(len);
	strcpy(result.GetPtr(), str.GetPtr());
	osLib_returnFromFunction(hCPU, result.GetMPTR());
}

void export_realloc(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(mem, void, 0);
	ppcDefineParamU32(size, 1);
	MEMPTR<void> result = coreinit::default_MEMAllocFromDefaultHeap(size);
	memcpy(result.GetPtr(), mem.GetPtr(), size);
	coreinit::default_MEMFreeToDefaultHeap(mem.GetPtr());
	osLib_returnFromFunction(hCPU, result.GetMPTR());
}

CURLcode curl_global_init(uint32 flags)
{
	if (g_nlibcurl.initialized++)
	{
		return CURLE_OK;
	}

	g_nlibcurl.malloc = PPCInterpreter_makeCallableExportDepr(export_malloc);
	g_nlibcurl.calloc = PPCInterpreter_makeCallableExportDepr(export_calloc);
	g_nlibcurl.free = PPCInterpreter_makeCallableExportDepr(export_free);
	g_nlibcurl.strdup = PPCInterpreter_makeCallableExportDepr(export_strdup);
	g_nlibcurl.realloc = PPCInterpreter_makeCallableExportDepr(export_realloc);

	// curl_read_default_proxy_config()

	return ::curl_global_init(flags);
}

void export_curl_multi_init(PPCInterpreter_t* hCPU)
{
	CURLMPtr result{ PPCCoreCallback(g_nlibcurl.calloc, 1, ppcsizeof<CURLM_t>()) };
	cemuLog_logDebug(LogType::Force, "curl_multi_init() -> 0x{:08x}", result.GetMPTR());
	if (result)
	{
		memset(result.GetPtr(), 0, sizeof(CURLM_t));
		*result = {};
		result->curlm = curl_multi_init();
	}

	osLib_returnFromFunction(hCPU, result.GetMPTR());
}

void export_curl_multi_add_handle(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(curlm, CURLM_t, 0);
	ppcDefineParamMEMPTR(curl, CURL_t, 1);

	curlDebug_markActiveRequest(curl.GetPtr());
	curlDebug_notifySubmitRequest(curl.GetPtr());

	CURLMcode result = ::curl_multi_add_handle(curlm->curlm, curl->curl);
	if (result == CURLM_OK)
		curlm->curl.push_back(curl.GetPtr());

	cemuLog_logDebug(LogType::Force, "curl_multi_add_handle(0x{:08x}, 0x{:08x}) -> 0x{:x}", curlm.GetMPTR(), curl.GetMPTR(), result);
	osLib_returnFromFunction(hCPU, result);
}

void export_curl_multi_remove_handle(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(curlm, CURLM_t, 0);
	ppcDefineParamMEMPTR(curl, CURL_t, 1);

	CURLMcode result = curl_multi_remove_handle(curlm->curlm, curl->curl);

	if (result == CURLM_OK)
	{
		curlm->curl.erase(std::remove(curlm->curl.begin(), curlm->curl.end(), (void*)curl.GetPtr()), curlm->curl.end());
	}

	cemuLog_logDebug(LogType::Force, "curl_multi_remove_handle(0x{:08x}, 0x{:08x}) -> 0x{:x}", curlm.GetMPTR(), curl.GetMPTR(), result);
	osLib_returnFromFunction(hCPU, result);
}

void export_curl_multi_timeout(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(curlm, CURLM_t, 0);
	ppcDefineParamMEMPTR(timeoParam, uint32be, 1);

	long timeoutLE = (long)(uint32)*timeoParam;

	CURLMcode result = ::curl_multi_timeout(curlm->curlm, &timeoutLE);
	*timeoParam = (uint32)timeoutLE;

	cemuLog_logDebug(LogType::Force, "curl_multi_timeout(0x{:08x}, 0x{:08x} [{}]) -> 0x{:x}", curlm.GetMPTR(), timeoParam.GetMPTR(), timeoutLE, result);
	osLib_returnFromFunction(hCPU, result);
}

void export_curl_multi_cleanup(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(curlm, CURLM_t, 0);
	CURLMcode result = ::curl_multi_cleanup(curlm->curlm);
	cemuLog_logDebug(LogType::Force, "curl_multi_cleanup(0x{:08x}) -> 0x{:x}", curlm.GetMPTR(), result);
	curlm->curl.clear();
	PPCCoreCallback(g_nlibcurl.free.GetMPTR(), curlm.GetMPTR());
	osLib_returnFromFunction(hCPU, result);
}

void export_curl_multi_perform(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(curlm, CURLM_t, 0);
	ppcDefineParamMEMPTR(runningHandles, uint32be, 1);

	//cemuLog_logDebug(LogType::Force, "curl_multi_perform(0x{:08x}, 0x{:08x})", curlm.GetMPTR(), runningHandles.GetMPTR());

	//curl_multi_get_handles(curlm->curlm);

	for(auto _curl : curlm->curl)
	{
		CURL_t* curl = (CURL_t*)_curl.GetPtr();
		if(curl->isDirty)
		{
			curl->isDirty = false;
			_curl_sync_parameters(curl);
		}
	}

	//g_callerQueue = curlm->callerQueue;
	//g_threadQueue = curlm->threadQueue;
	int tempRunningHandles = 0;
	CURLMcode result = curl_multi_perform(curlm->curlm, &tempRunningHandles);
	*(runningHandles.GetPtr()) = tempRunningHandles;
	cemuLog_logDebug(LogType::Force, "curl_multi_perform(0x{:08x}, 0x{:08x}) -> 0x{:x} (running handles: {})", curlm.GetMPTR(), runningHandles.GetMPTR(), result, tempRunningHandles);
	//const uint32 result = SendOrderToWorker(curlm.GetPtr(), QueueOrder_MultiPerform);

	osLib_returnFromFunction(hCPU, result);
}

void export_curl_multi_fdset(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(curlm, CURLM_t, 0);
	ppcDefineParamMEMPTR(readFd, wu_fd_set, 1);
	ppcDefineParamMEMPTR(writeFd, wu_fd_set, 2);
	ppcDefineParamMEMPTR(exceptionFd, wu_fd_set, 3);
	ppcDefineParamU32BEPtr(maxFd, 4);

	fd_set h_readFd;
	fd_set h_writeFd;
	fd_set h_exceptionFd;
	FD_ZERO(&h_readFd);
	FD_ZERO(&h_writeFd);
	FD_ZERO(&h_exceptionFd);
	sint32 h_maxFd = 0;

	CURLMcode result = curl_multi_fdset(curlm->curlm, &h_readFd, &h_writeFd, &h_exceptionFd, &h_maxFd);

	nsysnet::wuResetFD(readFd.GetPtr());
	nsysnet::wuResetFD(writeFd.GetPtr());
	nsysnet::wuResetFD(exceptionFd.GetPtr());

	sint32 c_maxFD = -1;

	auto hostFdSet = [&](SOCKET s, wu_fd_set* fds)
	{
		sint32 wuSocket = nsysnet_getVirtualSocketHandleFromHostHandle(s);
		if (wuSocket < 0)
			wuSocket = nsysnet_createVirtualSocketFromExistingSocket(s);
		if (wuSocket >= 0)
		{
			c_maxFD = std::max(wuSocket, c_maxFD);
			nsysnet::wuSetFD(fds, wuSocket);
		}
	};

#if BOOST_OS_UNIX
	for (int s = 0; s < h_maxFd + 1; s++) 
	{
		if(FD_ISSET(s, &h_readFd))
			hostFdSet(s, readFd.GetPtr());

		if(FD_ISSET(s, &h_writeFd))
			hostFdSet(s, writeFd.GetPtr());

		if(FD_ISSET(s, &h_exceptionFd))
			hostFdSet(s, exceptionFd.GetPtr());
	}
#else
	// fd read set
	for (uint32 i = 0; i < h_readFd.fd_count; i++)
	{
		hostFdSet(h_readFd.fd_array[i], readFd.GetPtr());
	}
	// fd write set
	for (uint32 i = 0; i < h_writeFd.fd_count; i++)
	{
		hostFdSet(h_writeFd.fd_array[i], writeFd.GetPtr());
	}
	// fd exception set
	for (uint32 i = 0; i < h_exceptionFd.fd_count; i++)
	{
		cemu_assert_debug(false);
	}
#endif

	*maxFd = c_maxFD;
	osLib_returnFromFunction(hCPU, result);
}

void export_curl_multi_setopt(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(curlm, CURLM_t, 0);
	ppcDefineParamU32(option, 1);
	ppcDefineParamMEMPTR(parameter, void, 2);
	ppcDefineParamU64(parameterU64, 2);

	CURLMcode result = CURLM_OK;
	switch (option)
	{
	case CURLMOPT_MAXCONNECTS:
		result = ::curl_multi_setopt(curlm->curlm, (CURLMoption)option, parameter.GetMPTR());
		break;
	default:
		cemuLog_logDebug(LogType::Force, "curl_multi_setopt(0x{:08x}, {}, 0x{:08x}) unsupported option", curlm.GetMPTR(), option, parameter.GetMPTR());
	}

	cemuLog_logDebug(LogType::Force, "curl_multi_setopt(0x{:08x}, {}, 0x{:08x}) -> 0x{:x}", curlm.GetMPTR(), option, parameter.GetMPTR(), result);

	osLib_returnFromFunction(hCPU, result);
}

void export_curl_multi_info_read(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(curlm, CURLM_t, 0);
	ppcDefineParamMEMPTR(msgsInQueue, int, 1);

	CURLMsg* msg = ::curl_multi_info_read(curlm->curlm, msgsInQueue.GetPtr());

	cemuLog_logDebug(LogType::Force, "curl_multi_info_read - todo");
	if (msg)
	{
		MEMPTR<CURLMsg_t> result{ PPCCoreCallback(g_nlibcurl.malloc.GetMPTR(), ppcsizeof<CURLMsg_t>()) };
		result->msg = msg->msg;
		result->result = msg->data.result;
		if (msg->easy_handle)
		{
			const auto it = find_if(curlm->curl.cbegin(), curlm->curl.cend(),
				[msg](const MEMPTR<CURL_t>& curl)
			{
				const MEMPTR<CURL_t> _curl{ curl };
				return _curl->curl = msg->easy_handle;
			});

			if (it != curlm->curl.cend())
				result->curl = (CURL_t*)(*it).GetPtr();

		}
		else
			result->curl = nullptr;

		cemuLog_logDebug(LogType::Force, "curl_multi_info_read(0x{:08x}, 0x{:08x} [{}]) -> 0x{:08x} (0x{:x}, 0x{:08x}, 0x{:x})", curlm.GetMPTR(), msgsInQueue.GetMPTR(), *msgsInQueue.GetPtr(),			
			result.GetMPTR(), msg->msg, result->curl.GetMPTR(), msg->data.result);
		osLib_returnFromFunction(hCPU, result.GetMPTR());
	}
	else
	{
		osLib_returnFromFunction(hCPU, 0);
	}
}

void lock_function(CURL* handle, curl_lock_data data, curl_lock_access access, void* userptr)
{
	CURLSH_t* share = (CURLSH_t*)userptr;
	PPCCoreCallback(share->lockfunc.GetMPTR(), share->curl.GetMPTR(), (uint32)data, (uint32)access, share->data.GetMPTR());
}

void unlock_function(CURL* handle, curl_lock_data data, void* userptr)
{
	CURLSH_t* share = (CURLSH_t*)userptr;
	PPCCoreCallback(share->unlockfunc.GetMPTR(), share->curl.GetMPTR(), (uint32)data, share->data.GetMPTR());
}

void export_curl_share_setopt(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(share, CURLSH_t, 0);
	ppcDefineParamU32(option, 1);
	ppcDefineParamMEMPTR(parameter, void, 2);

	/*if(share->dirty)
	{
		osLib_returnFromFunction(hCPU, CURLSHE_IN_USE);
		return;
	}*/

	CURLSH* curlSh = share->curlsh;

	CURLSHcode result = CURLSHE_OK;
	switch (option)
	{
		case CURLSHOPT_USERDATA:
			share->data = parameter;
			result = curl_share_setopt(curlSh, CURLSHOPT_USERDATA, share.GetPtr());
			break;
		case CURLSHOPT_LOCKFUNC:
			share->lockfunc = parameter;
			result = curl_share_setopt(curlSh, CURLSHOPT_LOCKFUNC, lock_function);
			break;
		case CURLSHOPT_UNLOCKFUNC:
			share->unlockfunc = parameter;
			result = curl_share_setopt(curlSh, CURLSHOPT_UNLOCKFUNC, unlock_function);
			break;
		case CURLSHOPT_SHARE:
		{
			const sint32 type = parameter.GetMPTR();
			result = curl_share_setopt(curlSh, CURLSHOPT_SHARE, type);
			break;
		}
		case CURLSHOPT_UNSHARE:
		{
			const sint32 type = parameter.GetMPTR();
			result = curl_share_setopt(curlSh, CURLSHOPT_UNSHARE, type);
			break;
		}
		default:
			cemu_assert_unimplemented();
	}
	cemuLog_logDebug(LogType::Force, "curl_share_setopt(0x{:08x}, 0x{:x}, 0x{:08x}) -> 0x{:x}", share.GetMPTR(), option, parameter.GetMPTR(), result);
	osLib_returnFromFunction(hCPU, result);
}

void export_curl_share_init(PPCInterpreter_t* hCPU)
{
	CURLSHPtr result{ PPCCoreCallback(g_nlibcurl.calloc.GetMPTR(), (uint32)1, ppcsizeof<CURLSH_t>()) };
	cemuLog_logDebug(LogType::Force, "curl_share_init() -> 0x{:08x}", result.GetMPTR());
	if (result)
	{
		memset(result.GetPtr(), 0, sizeof(CURLSH_t));
		*result = {};
		result->curlsh = curl_share_init();
	}

	osLib_returnFromFunction(hCPU, result.GetMPTR());
}

void export_curl_share_cleanup(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(curlsh, CURLSH_t, 0);
	uint32 result = ::curl_share_cleanup(curlsh->curlsh);
	cemuLog_logDebug(LogType::Force, "curl_share_cleanup(0x{:08x}) -> 0x{:x}", curlsh.GetMPTR(), result);
	PPCCoreCallback(g_nlibcurl.free.GetMPTR(), curlsh.GetMPTR());
	osLib_returnFromFunction(hCPU, 0);
}

CURL_t* curl_easy_init()
{
	if (g_nlibcurl.initialized == 0)
	{
		if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
		{
			return nullptr;
		}
	}

	// Curl_open
	MEMPTR<CURL_t> result{ PPCCoreCallback(g_nlibcurl.calloc.GetMPTR(), (uint32)1, ppcsizeof<CURL_t>()) };
	cemuLog_logDebug(LogType::Force, "curl_easy_init() -> 0x{:08x}", result.GetMPTR());
	if (result)
	{
		memset(result.GetPtr(), 0, sizeof(CURL_t));
		*result = {};
		result->curl = ::curl_easy_init();
		result->curlThread = coreinit::OSGetCurrentThread();

		result->info_contentType = nullptr;
		result->info_redirectUrl = nullptr;

		_curl_set_default_parameters(result.GetPtr());

		if (g_nlibcurl.proxyConfig)
		{
			// todo
		}
	}

	return result;
}

CURL_t* mw_curl_easy_init()
{
	return curl_easy_init();
}

void export_curl_easy_pause(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(curl, CURL_t, 0);
	ppcDefineParamS32(bitmask, 1);
	cemuLog_logDebug(LogType::Force, "curl_easy_pause(0x{:08x}, 0x{:x})", curl.GetMPTR(), bitmask);

	//const CURLcode result = ::curl_easy_pause(curl->curl, bitmask);
	const uint32 result = SendOrderToWorker(curl.GetPtr(), QueueOrder_Pause, bitmask);
	cemuLog_logDebug(LogType::Force, "curl_easy_pause(0x{:08x}, 0x{:x}) DONE", curl.GetMPTR(), bitmask);
	osLib_returnFromFunction(hCPU, result);
}

void export_curl_easy_cleanup(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(curl, CURL_t, 0);
	cemuLog_logDebug(LogType::Force, "curl_easy_cleanup(0x{:08x})", curl.GetMPTR());

	curlDebug_cleanup(curl.GetPtr());

	::curl_easy_cleanup(curl->curl);
	PPCCoreCallback(g_nlibcurl.free.GetMPTR(), curl.GetMPTR());

	if (curl->info_contentType.IsNull() == false)
		PPCCoreCallback(g_nlibcurl.free.GetMPTR(), curl->info_contentType.GetMPTR());
	if (curl->info_redirectUrl.IsNull() == false)
		PPCCoreCallback(g_nlibcurl.free.GetMPTR(), curl->info_redirectUrl.GetMPTR());

	osLib_returnFromFunction(hCPU, 0);
}

void export_curl_easy_reset(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(curl, CURL_t, 0);
	cemuLog_logDebug(LogType::Force, "curl_easy_reset(0x{:08x})", curl.GetMPTR());
	// curl_apply_default_proxy_config();
	::curl_easy_reset(curl->curl);
	osLib_returnFromFunction(hCPU, 0);
}

int ssl_verify_callback(int preverify_ok, X509_STORE_CTX* ctx)
{
	if (preverify_ok) return preverify_ok;
	int err = X509_STORE_CTX_get_error(ctx);
	if(err != 0)
		cemuLog_logDebug(LogType::Force, "ssl_verify_callback: Error {} but allow certificate anyway", err);
	X509_STORE_CTX_set_error(ctx, 0);
	return 1;
}

CURLcode ssl_ctx_callback(CURL* curl, void* sslctx, void* param)
{
	//peterBreak();
	CURL_t* ppcCurl = (CURL_t*)param;
	nsysnet::NSSLInternalState_t* nssl = nsysnet::GetNSSLContext((uint32)ppcCurl->hNSSL);

	for (uint32 i : nssl->serverPKIs)
	{
		if (iosuCrypto_addCACertificate(sslctx, i) == false)
		{
			cemu_assert_suspicious();
			return CURLE_SSL_CACERT;
		}
	}
	for (auto& customPKI : nssl->serverCustomPKIs)
	{
		if (iosuCrypto_addCustomCACertificate(sslctx, &customPKI[0], (sint32)customPKI.size()) == false)
		{
			cemu_assert_suspicious();
			return CURLE_SSL_CACERT;
		}
	}

	if (nssl->clientPKI != 0 && iosuCrypto_addClientCertificate(sslctx, nssl->clientPKI) == false)
	{
		cemu_assert_suspicious();
		return CURLE_SSL_CERTPROBLEM;
	}

	uint32 flags = ppcCurl->nsslVerifyOptions;
	if (HAS_FLAG(flags, NSSL_VERIFY_PEER))
	{
		::SSL_CTX_set_cipher_list((SSL_CTX*)sslctx, "AES256-SHA"); // TLS_RSA_WITH_AES_256_CBC_SHA (in CURL it's called rsa_aes_256_sha)
		::SSL_CTX_set_mode((SSL_CTX*)sslctx, SSL_MODE_AUTO_RETRY);
		::SSL_CTX_set_verify_depth((SSL_CTX*)sslctx, 2);
		::SSL_CTX_set_verify((SSL_CTX*)sslctx, SSL_VERIFY_PEER, ssl_verify_callback);
	}
	else
	{
		::SSL_CTX_set_verify((SSL_CTX*)sslctx, SSL_VERIFY_NONE, nullptr);
	}
	
	return CURLE_OK;
}

size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata)
{
	//peterBreak();
	CURL_t* curl = (CURL_t*)userdata;

	curlDebug_headerWrite(curl, buffer, size, nitems);

	if (curl->fwrite_header.IsNull())
	{
		return size * nitems;
	}

	if (g_callerQueue == nullptr || g_threadQueue == nullptr)
	{
		StackAllocator<char> tmp((uint32)(size * nitems));
		memcpy(tmp.GetPointer(), buffer, size * nitems);
		return PPCCoreCallback(curl->fwrite_header.GetMPTR(), tmp.GetMPTR(), (uint32)size, (uint32)nitems, curl->writeheader.GetMPTR());
	}

	QueueMsg_t msg = {};
	msg.order = QueueOrder_HeaderCB;
	msg.header_cb.buffer = buffer;
	msg.header_cb.size = (uint32)size;
	msg.header_cb.nitems = (uint32)nitems;
	g_callerQueue->push(msg, curl->curlThread);

	msg = g_threadQueue->pop();
	if (msg.order != QueueOrder_CBDone)
		cemu_assert_suspicious();

#ifdef CEMU_DEBUG_ASSERT
	char debug[500];
	cemu_assert_debug((size*nitems) < 500);
	memcpy(debug, buffer, size*nitems);
	debug[size*nitems] = 0;
	cemuLog_logDebug(LogType::Force, "header_callback(0x{}, 0x{:x}, 0x{:x}, 0x{:08x}) [{}]", (void*)buffer, size, nitems, curl->writeheader.GetMPTR(), debug);
#endif
	return msg.result;
}

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	CURL_t* curl = (CURL_t*)userdata;

	curlDebug_resultWrite(curl, ptr, size, nmemb);
	//StackAllocator<char> tmp(size * nmemb);
	//memcpy(tmp.GetPointer(), ptr, size * nmemb);

	cemuLog_logDebug(LogType::Force, "write_callback(0x{} 0x{:x}, 0x{:x}, 0x{:08x})", (void*)ptr, size, nmemb, curl->out.GetMPTR());

	if (g_callerQueue == nullptr || g_threadQueue == nullptr)
	{
		StackAllocator<char> tmp((uint32)(size * nmemb));
		memcpy(tmp.GetPointer(), ptr, size * nmemb);
		int r = PPCCoreCallback(curl->fwrite_func.GetMPTR(), tmp.GetMPTR(), (uint32)size, (uint32)nmemb, curl->out.GetMPTR());
		return r;
	}

	QueueMsg_t msg = {};
	msg.order = QueueOrder_WriteCB;
	msg.write_cb.buffer = ptr;
	msg.write_cb.size = (uint32)size;
	msg.write_cb.nmemb = (uint32)nmemb;
	g_callerQueue->push(msg, curl->curlThread);

	msg = g_threadQueue->pop();
	if (msg.order != QueueOrder_CBDone)
		cemu_assert_suspicious();

	return msg.result;
}

int sockopt_callback(void* clientp, curl_socket_t curlfd, curlsocktype purpose)
{
	CURL_t* curl = (CURL_t*)clientp;

	cemuLog_logDebug(LogType::Force, "sockopt_callback called!");

	sint32 r = 0;

	return r;
}

size_t read_callback(char* buffer, size_t size, size_t nitems, void* instream)
{	
	nitems = std::min<uint32>(nitems, 0x4000);
	CURL_t* curl = (CURL_t*)instream;

	cemuLog_logDebug(LogType::Force, "read_callback(0x{}, 0x{:x}, 0x{:x}, 0x{:08x}) [func: 0x{:x}]", (void*)buffer, size, nitems, curl->in_set.GetMPTR(), curl->fread_func_set.GetMPTR());

	if (g_callerQueue == nullptr || g_threadQueue == nullptr)
	{
		StackAllocator<char> tmp((uint32)(size * nitems));
		const sint32 result = PPCCoreCallback(curl->fread_func_set.GetMPTR(), tmp.GetMPTR(), (uint32)size, (uint32)nitems, curl->in_set.GetMPTR());
		memcpy(buffer, tmp.GetPointer(), result);
		return result;
	}

	QueueMsg_t msg = {};
	msg.order = QueueOrder_ReadCB;
	msg.read_cb.buffer = buffer;
	msg.read_cb.size = (uint32)size;
	msg.read_cb.nitems = (uint32)std::min<uint32>(nitems, 0x4000); // limit this to 16KB which is the limit in nlibcurl.rpl (Super Mario Maker crashes on level upload if the size is too big)

	cemuLog_logDebug(LogType::Force, "read_callback(0x{}, 0x{:x}, 0x{:x}, 0x{:08x}) [func: 0x{:x}] PUSH", (void*)buffer, size, nitems, curl->in_set.GetMPTR(), curl->fread_func_set.GetMPTR());
	g_callerQueue->push(msg, curl->curlThread);

	msg = g_threadQueue->pop();
	if (msg.order != QueueOrder_CBDone)
		cemu_assert_suspicious();

	cemuLog_logDebug(LogType::Force, "read_callback(0x{}, 0x{:x}, 0x{:x}, 0x{:08x}) DONE Result: {}", (void*)buffer, size, nitems, curl->in_set.GetMPTR(), msg.result);

	return msg.result;
}


int progress_callback(void* clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	//peterBreak();
	CURL_t* curl = (CURL_t*)clientp;
	//int result = PPCCoreCallback(curl->fprogress.GetMPTR(), curl->progress_client.GetMPTR(), dltotal, dlnow, ultotal, ulnow);
	if(g_callerQueue == nullptr || g_threadQueue == nullptr)
		return PPCCoreCallback(curl->fprogress.GetMPTR(), curl->progress_client.GetMPTR(), dltotal, dlnow, ultotal, ulnow);

	QueueMsg_t msg = {};
	msg.order = QueueOrder_ProgressCB;
	msg.progress_cb.dltotal = dltotal;
	msg.progress_cb.dlnow = dlnow;
	msg.progress_cb.ultotal = ultotal;
	msg.progress_cb.ulnow = ulnow;
	g_callerQueue->push(msg, curl->curlThread);

	msg = g_threadQueue->pop();
	if (msg.order != QueueOrder_CBDone)
		cemu_assert_suspicious();
	
	cemuLog_logDebug(LogType::Force, "progress_callback({:.02}, {:.02}, {:.02}, {:.02}) -> {}", dltotal, dlnow, ultotal, ulnow, msg.result);
	return msg.result;
}

void export_curl_easy_setopt(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(curl, CURL_t, 0);
	ppcDefineParamU32(option, 1);
	ppcDefineParamMEMPTR(parameter, void, 2);
	ppcDefineParamU64(parameterU64, 2);

	CURL* curlObj = curl->curl;
	curl->isDirty = true;

	CURLcode result = CURLE_OK;
	switch (option)
	{
		case CURLOPT_POST:
		{
			if(parameter)
			{
				curl->set.httpreq_1EC = WU_HTTPREQ::HTTPREQ_POST;
				curl->opt_no_body_281 = 0;
			}
			else
				curl->set.httpreq_1EC = WU_HTTPREQ::HTTPREQ_GET;
			break;
		}
		case CURLOPT_HTTPGET:
		{
			if (parameter)
			{
				curl->set.httpreq_1EC = WU_HTTPREQ::HTTPREQ_GET;
				curl->opt_no_body_281 = 0;
				curl->upload_283 = 0;
			}
			break;
		}
		case CURLOPT_INFILESIZE:
		{
			curl->set.infilesize_190 = (sint64)(sint32)(uint32)parameter.GetBEValue();
			break;
		}
		case CURLOPT_INFILESIZE_LARGE:
		{
			curl->set.infilesize_190 = (sint64)(uint64)parameterU64;
			break;
		}
		case CURLOPT_NOSIGNAL:
		case CURLOPT_FOLLOWLOCATION:
		case CURLOPT_BUFFERSIZE:
		case CURLOPT_TIMEOUT:
		case CURLOPT_CONNECTTIMEOUT_MS:
		case CURLOPT_NOPROGRESS:
		case CURLOPT_LOW_SPEED_LIMIT:
		case CURLOPT_LOW_SPEED_TIME:
		case CURLOPT_CONNECTTIMEOUT:
		{
			result = ::curl_easy_setopt(curlObj, (CURLoption)option, parameter.GetMPTR());
			break;
		}
		case CURLOPT_URL:
		{
			curlDebug_logEasySetOptStr(curl.GetPtr(), "CURLOPT_URL", (const char*)parameter.GetPtr());
			cemuLog_logDebug(LogType::Force, "curl_easy_setopt({}) [{}]", option, parameter.GetPtr());
			result = ::curl_easy_setopt(curlObj, (CURLoption)option, parameter.GetPtr());
			break;
		}

		case CURLOPT_PROXY:
		case CURLOPT_USERAGENT:
		{
			cemuLog_logDebug(LogType::Force, "curl_easy_setopt({}) [{}]", option, parameter.GetPtr());
			result = ::curl_easy_setopt(curlObj, (CURLoption)option, parameter.GetPtr());
			break;
		}

		case CURLOPT_POSTFIELDS:
		{
			curlDebug_logEasySetOptStr(curl.GetPtr(), "CURLOPT_POSTFIELDS", (const char*)parameter.GetPtr());
			cemuLog_logDebug(LogType::Force, "curl_easy_setopt({}) [{}]", option, parameter.GetPtr());
			result = ::curl_easy_setopt(curlObj, (CURLoption)option, parameter.GetPtr());
			break;
		}

		case CURLOPT_MAX_SEND_SPEED_LARGE:
		case CURLOPT_MAX_RECV_SPEED_LARGE:
		case CURLOPT_POSTFIELDSIZE_LARGE:
		{
			result = ::curl_easy_setopt(curlObj, (CURLoption)option, parameterU64);
			break;
		}
		case 211: // verifyOpt
		{
			uint32 flags = parameter.GetMPTR();
			curl->nsslVerifyOptions = flags;

			if (HAS_FLAG(flags, NSSL_VERIFY_PEER))
				::curl_easy_setopt(curlObj, CURLOPT_SSL_VERIFYPEER, 1);
			else
				::curl_easy_setopt(curlObj, CURLOPT_SSL_VERIFYPEER, 0);

			if (HAS_FLAG(flags, NSSL_VERIFY_HOSTNAME))
				::curl_easy_setopt(curlObj, CURLOPT_SSL_VERIFYHOST, 2);
			else
				::curl_easy_setopt(curlObj, CURLOPT_SSL_VERIFYHOST, 0);

			break;
		}
		case 210: // SSL_CONTEXT -> set context created with NSSLCreateContext before
		{
			const uint32 nsslIndex = parameter.GetMPTR();

			nsysnet::NSSLInternalState_t* nssl = nsysnet::GetNSSLContext(nsslIndex);
			if (nssl)
			{
				curl->hNSSL = nsslIndex;
				result = ::curl_easy_setopt(curlObj, CURLOPT_SSL_CTX_FUNCTION, ssl_ctx_callback);
				::curl_easy_setopt(curlObj, CURLOPT_SSL_CTX_DATA, curl.GetPtr());

				if (nssl->sslVersion == 2)
					::curl_easy_setopt(curlObj, CURLOPT_SSLVERSION, CURL_SSLVERSION_SSLv3);
				else // auto = highest = 0 || 2 for v3
					::curl_easy_setopt(curlObj, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
			}
			else
				cemu_assert_suspicious();
			break;
		}
		case CURLOPT_SHARE:
		{
			CURLSH* shObj = nullptr;
			if (parameter)
			{
				auto curlSh = (MEMPTR<CURLSH_t>)parameter;
				curlSh->curl = curl;
				shObj = curlSh->curlsh;
			}
			result = ::curl_easy_setopt(curlObj, CURLOPT_SHARE, shObj);
			break;
		}
		case CURLOPT_HEADERFUNCTION:
		{
			curlDebug_logEasySetOptPtr(curl.GetPtr(), "CURLOPT_HEADERFUNCTION", parameter.GetMPTR());
			curl->fwrite_header = parameter;
			break;
		}
		case CURLOPT_HEADERDATA:
		{
			curlDebug_logEasySetOptPtr(curl.GetPtr(), "CURLOPT_HEADERDATA", parameter.GetMPTR());
			curl->writeheader = parameter;
			break;
		}
		case CURLOPT_WRITEFUNCTION:
		{
			curlDebug_logEasySetOptPtr(curl.GetPtr(), "CURLOPT_WRITEFUNCTION", parameter.GetMPTR());
			curl->fwrite_func = parameter;
			result = ::curl_easy_setopt(curlObj, CURLOPT_WRITEFUNCTION, write_callback);
			::curl_easy_setopt(curlObj, CURLOPT_WRITEDATA, curl.GetPtr());
			break;
		}
		case CURLOPT_WRITEDATA: // aka CURLOPT_FILE
		{
			curlDebug_logEasySetOptPtr(curl.GetPtr(), "CURLOPT_WRITEDATA", parameter.GetMPTR());
			curl->out = parameter;
			break;
		}
		case CURLOPT_HTTPHEADER:
		{
			curl->set.headers = (WU_curl_slist*)parameter.GetPtr();
			result = CURLE_OK;
			break;
		}
		case CURLOPT_SOCKOPTFUNCTION:
		{
			curl->fsockopt = parameter;
			result = ::curl_easy_setopt(curlObj, CURLOPT_SOCKOPTFUNCTION, sockopt_callback);
			::curl_easy_setopt(curlObj, CURLOPT_SOCKOPTDATA, curl.GetPtr());
			break;
		}
		case CURLOPT_SOCKOPTDATA:
		{
			curl->sockopt_client = parameter;
			break;
		}
		case CURLOPT_READFUNCTION:
		{
			curlDebug_logEasySetOptPtr(curl.GetPtr(), "CURLOPT_READFUNCTION", parameter.GetMPTR());
			curl->fread_func_set = parameter;
			result = ::curl_easy_setopt(curlObj, CURLOPT_READFUNCTION, read_callback);
			::curl_easy_setopt(curlObj, CURLOPT_READDATA, curl.GetPtr());
			break;
		}
		case CURLOPT_READDATA:
		{
			curlDebug_logEasySetOptPtr(curl.GetPtr(), "CURLOPT_READDATA", parameter.GetMPTR());
			curl->in_set = parameter;
			break;
		}
		case CURLOPT_PROGRESSFUNCTION:
		{
			curlDebug_logEasySetOptPtr(curl.GetPtr(), "CURLOPT_PROGRESSFUNCTION", parameter.GetMPTR());
			curl->fprogress = parameter;
			result = ::curl_easy_setopt(curlObj, CURLOPT_PROGRESSFUNCTION, progress_callback);
			::curl_easy_setopt(curlObj, CURLOPT_PROGRESSDATA, curl.GetPtr());
			break;
		}
		case CURLOPT_PROGRESSDATA:
		{
			curlDebug_logEasySetOptPtr(curl.GetPtr(), "CURLOPT_PROGRESSDATA", parameter.GetMPTR());
			curl->progress_client = parameter;
			break;
		}
		default:
			cemuLog_logDebug(LogType::Force, "curl_easy_setopt(0x{:08x}, {}, 0x{:08x}) unsupported option", curl.GetMPTR(), option, parameter.GetMPTR());
	}

	cemuLog_logDebug(LogType::Force, "curl_easy_setopt(0x{:08x}, {}, 0x{:08x}) -> 0x{:x}", curl.GetMPTR(), option, parameter.GetMPTR(), result);

	osLib_returnFromFunction(hCPU, result);
}

WU_CURLcode curl_easy_perform(CURL_t* curl)
{
	curlDebug_markActiveRequest(curl);
	curlDebug_notifySubmitRequest(curl);

	if(curl->isDirty)
	{
		curl->isDirty = false;
		_curl_sync_parameters(curl);
	}
	const uint32 result = SendOrderToWorker(curl, QueueOrder_Perform);
	return static_cast<WU_CURLcode>(result);
}

void _updateGuestString(CURL_t* curl, MEMPTR<char>& ppcStr, char* hostStr)
{
	// free current string
	if (ppcStr.IsNull() == false)
	{
		PPCCoreCallback(g_nlibcurl.free.GetMPTR(), ppcStr);
		ppcStr = nullptr;
	}
	if (hostStr == nullptr)
		return;
	sint32 length = (sint32)strlen(hostStr);
	ppcStr = PPCCoreCallback(g_nlibcurl.malloc.GetMPTR(), length+1);
	memcpy(ppcStr.GetPtr(), hostStr, length + 1);
}

void export_curl_easy_getinfo(PPCInterpreter_t* hCPU)
{
	ppcDefineParamMEMPTR(curl, CURL_t, 0);
	ppcDefineParamU32(info, 1);
	ppcDefineParamMEMPTR(parameter, void, 2);

	CURL* curlObj = curl->curl;

	CURLcode result = CURLE_OK;
	switch (info)
	{
		case CURLINFO_SIZE_DOWNLOAD:
		case CURLINFO_SPEED_DOWNLOAD:
		case CURLINFO_SIZE_UPLOAD:
		case CURLINFO_SPEED_UPLOAD:
		case CURLINFO_CONTENT_LENGTH_DOWNLOAD:
		{
			double tempDouble = 0.0;
			result = curl_easy_getinfo(curlObj, (CURLINFO)info, &tempDouble);
			*(uint64*)parameter.GetPtr() = _swapEndianU64(*(uint64*)&tempDouble);
			break;
		}
		case CURLINFO_RESPONSE_CODE:
		case CURLINFO_SSL_VERIFYRESULT:
		{
			long tempLong = 0;
			result = curl_easy_getinfo(curlObj, (CURLINFO)info, &tempLong);
			*(uint32*)parameter.GetPtr() = _swapEndianU32((uint32)tempLong);
			break;
		}
		case CURLINFO_CONTENT_TYPE:
		{
			char* contentType = nullptr;
			result = curl_easy_getinfo(curlObj, CURLINFO_REDIRECT_URL, &contentType);
			_updateGuestString(curl.GetPtr(), curl->info_contentType, contentType);
			*(MEMPTR<char>*)parameter.GetPtr() = curl->info_contentType;
			break;
		}
		case CURLINFO_REDIRECT_URL:
		{
			char* redirectUrl = nullptr;
			result = curl_easy_getinfo(curlObj, CURLINFO_REDIRECT_URL, &redirectUrl);
			_updateGuestString(curl.GetPtr(), curl->info_redirectUrl, redirectUrl);
			*(MEMPTR<char>*)parameter.GetPtr() = curl->info_redirectUrl;
			break;
		}
		default:
			cemu_assert_unimplemented();
			result = curl_easy_getinfo(curlObj, (CURLINFO)info, (double*)parameter.GetPtr());
	}

	cemuLog_logDebug(LogType::Force, "curl_easy_getinfo(0x{:08x}, 0x{:x}, 0x{:08x}) -> 0x{:x}", curl.GetMPTR(), info, parameter.GetMPTR(), result);
	osLib_returnFromFunction(hCPU, result);
}

void export_curl_easy_strerror(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(code, 0);
	MEMPTR<char> result;

	const char* error = curl_easy_strerror((CURLcode)code);
	if (error)
	{
		cemuLog_logDebug(LogType::Force, "curl_easy_strerror: {}", error);
		int len = (int)strlen(error) + 1;
		result = coreinit_allocFromSysArea(len, 4);
		memcpy(result.GetPtr(), error, len);
	}
	osLib_returnFromFunction(hCPU, result.GetMPTR());
}

WU_curl_slist* curl_slist_append(WU_curl_slist* list, const char* data)
{
	MEMPTR<char> dupdata{ PPCCoreCallback(g_nlibcurl.strdup.GetMPTR(), data) };
	if (!dupdata)
	{
		cemuLog_logDebug(LogType::Force, "curl_slist_append(): Failed to duplicate string");
		return nullptr;
	}

	MEMPTR<WU_curl_slist> result{ PPCCoreCallback(g_nlibcurl.malloc.GetMPTR(), ppcsizeof<WU_curl_slist>()) };
	if (result)
	{
		result->data = dupdata;
		result->next = nullptr;

		// update last obj of list
		if (list)
		{
			MEMPTR<WU_curl_slist> tmp = list;
			while (tmp->next)
			{
				tmp = tmp->next;
			}

			tmp->next = result;
		}
	}
	else
	{
		cemuLog_logDebug(LogType::Force, "curl_slist_append(): Failed to allocate memory");
		PPCCoreCallback(g_nlibcurl.free.GetMPTR(), dupdata.GetMPTR());
	}
	if(list)
		return list;
	return result;
}
	
void curl_slist_free_all(WU_curl_slist* list)
{
	cemuLog_logDebug(LogType::Force, "export_curl_slist_free_all: TODO");
}

CURLcode curl_global_init_mem(uint32 flags, MEMPTR<curl_malloc_callback> malloc_callback, MEMPTR<curl_free_callback> free_callback, MEMPTR<curl_realloc_callback> realloc_callback, MEMPTR<curl_strdup_callback> strdup_callback, MEMPTR<curl_calloc_callback> calloc_callback)
{
	if(!malloc_callback || !free_callback || !realloc_callback || !strdup_callback || !calloc_callback)
		return CURLE_FAILED_INIT;

	CURLcode result = CURLE_OK;
	if (g_nlibcurl.initialized == 0)
	{
		result = curl_global_init(flags);
		if (result == CURLE_OK)
		{
			g_nlibcurl.malloc = malloc_callback;
			g_nlibcurl.free = free_callback;
			g_nlibcurl.realloc = realloc_callback;
			g_nlibcurl.strdup = strdup_callback;
			g_nlibcurl.calloc = calloc_callback;
		}
	}
	return result;
}

void load()
{
	cafeExportRegister("nlibcurl", curl_global_init_mem, LogType::nlibcurl);
	cafeExportRegister("nlibcurl", curl_global_init, LogType::nlibcurl);

	cafeExportRegister("nlibcurl", curl_slist_append, LogType::nlibcurl);
	cafeExportRegister("nlibcurl", curl_slist_free_all, LogType::nlibcurl);
	osLib_addFunction("nlibcurl", "curl_easy_strerror", export_curl_easy_strerror);

	osLib_addFunction("nlibcurl", "curl_share_init", export_curl_share_init);
	osLib_addFunction("nlibcurl", "curl_share_setopt", export_curl_share_setopt);
	osLib_addFunction("nlibcurl", "curl_share_cleanup", export_curl_share_cleanup);

	cafeExportRegister("nlibcurl", mw_curl_easy_init, LogType::nlibcurl);
	osLib_addFunction("nlibcurl", "curl_multi_init", export_curl_multi_init);
	osLib_addFunction("nlibcurl", "curl_multi_add_handle", export_curl_multi_add_handle);
	osLib_addFunction("nlibcurl", "curl_multi_perform", export_curl_multi_perform);
	osLib_addFunction("nlibcurl", "curl_multi_info_read", export_curl_multi_info_read);
	osLib_addFunction("nlibcurl", "curl_multi_remove_handle", export_curl_multi_remove_handle);
	osLib_addFunction("nlibcurl", "curl_multi_setopt", export_curl_multi_setopt);
	osLib_addFunction("nlibcurl", "curl_multi_fdset", export_curl_multi_fdset);
	osLib_addFunction("nlibcurl", "curl_multi_cleanup", export_curl_multi_cleanup);
	osLib_addFunction("nlibcurl", "curl_multi_timeout", export_curl_multi_timeout);

	cafeExportRegister("nlibcurl", curl_easy_init, LogType::nlibcurl);
	osLib_addFunction("nlibcurl", "curl_easy_reset", export_curl_easy_reset);
	osLib_addFunction("nlibcurl", "curl_easy_setopt", export_curl_easy_setopt);
	osLib_addFunction("nlibcurl", "curl_easy_getinfo", export_curl_easy_getinfo);
	cafeExportRegister("nlibcurl", curl_easy_perform, LogType::nlibcurl);



	osLib_addFunction("nlibcurl", "curl_easy_cleanup", export_curl_easy_cleanup);
	osLib_addFunction("nlibcurl", "curl_easy_pause", export_curl_easy_pause);
}
}