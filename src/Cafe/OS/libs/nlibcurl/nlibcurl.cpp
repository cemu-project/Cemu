#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/common/OSUtil.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "nlibcurl.h"

#include "openssl/bn.h"
#include "openssl/x509.h"
#include "openssl/ssl.h"

#define CURL_STRICTER

#include "curl/curl.h"
#include "Cafe/IOSU/legacy/iosu_crypto.h"
#include "Cafe/OS/libs/nsysnet/nsysnet.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM.h"

#include "Cafe/OS/common/PPCConcurrentQueue.h"

namespace nlibcurl
{
#define CURL_MULTI_HANDLE		(0x000bab1e)

#define NSSL_VERIFY_NONE		(0x0)
#define NSSL_VERIFY_PEER		(1<<0)
#define NSSL_VERIFY_HOSTNAME	(1<<1)
#define NSSL_VERIFY_DATE		(1<<2)

	struct
	{
		sint32 initialized;

		MEMPTR<curl_malloc_callback> malloc;
		MEMPTR<curl_free_callback> free;
		MEMPTR<curl_strdup_callback> strdup;
		MEMPTR<curl_calloc_callback> calloc;
	} g_nlibcurl = {};

	// option and info IDs as used by nlibcurl
	enum class WU_CURLOPT : uint32
	{
		WRITEDATA = 10001,
		URL = 10002,
		PROXY = 10004,
		READDATA = 10009,
		WRITEFUNCTION = 20011,
		READFUNCTION = 20012,
		TIMEOUT = 13,
		INFILESIZE = 14,
		POSTFIELDS = 10015,
		USERAGENT = 10018,
		LOW_SPEED_LIMIT = 19,
		LOW_SPEED_TIME = 20,
		HTTPHEADER = 10023,
		HEADERDATA = 10029,
		NOPROGRESS = 43,
		UPLOAD = 46,
		POST = 47,
		FOLLOWLOCATION = 52,
		PUT = 54,
		PROGRESSFUNCTION = 20056,
		PROGRESSDATA = 10057,
		CONNECTTIMEOUT = 78,
		HEADERFUNCTION = 20079,
		HTTPGET = 80,
		NOSIGNAL = 99,
		SHARE = 10100,
		BUFFERSIZE = 98,
		INFILESIZE_LARGE = 30115,
		POSTFIELDSIZE_LARGE = 30120,
		MAX_SEND_SPEED_LARGE = 30145,
		MAX_RECV_SPEED_LARGE = 30146,
		SOCKOPTFUNCTION = 20148,
		SOCKOPTDATA = 10149,
		CONNECTTIMEOUT_MS = 156,
		NSSL_CONTEXT = 210, // set context created with NSSLCreateContext
		NSSL_VERIFY_OPTIONS = 211,
	};

	enum class WU_CURLINFO : uint32
	{
		RESPONSE_CODE = 0x200002,
		SIZE_UPLOAD = 0x300007,
		SIZE_DOWNLOAD = 0x300008,
		SPEED_DOWNLOAD = 0x300009,
		SPEED_UPLOAD = 0x30000A,
		SSL_VERIFYRESULT = 0x20000D,
		CONTENT_LENGTH_DOWNLOAD = 0x30000F,
		CONTENT_TYPE = 0x100012,
		REDIRECT_URL = 0x10001F,
	};

	enum class WU_CURLcode
	{
		placeholder = 0,
	};

	enum class WU_HTTPREQ : uint32
	{
		HTTPREQ_NONE = 0,
		HTTPREQ_GET = 1,
		HTTPREQ_POST = 2,
		HTTPREQ_POST_FORM = 3,
		HTTPREQ_PUT = 4,
		HTTPREQ_HEAD = 5,
		HTTPREQ_CUSTOM = 6
	};

	// indices into WU_UserDefined::str
	enum WU_StringIndex : uint32
	{
		WU_STRING_PROXY = 16,
		WU_STRING_URL = 19,
		WU_STRING_USERAGENT = 25,
		WU_STRING_LAST = 37
	};

#pragma pack(push, 1)

	struct WU_curl_slist
	{
		MEMPTR<char> data;
		MEMPTR<WU_curl_slist> next;
	};
	static_assert(sizeof(WU_curl_slist) == 0x8);

	struct WU_UserDefined
	{
		/* +0x000 */ uint8 ukn000[0x10];
		/* +0x010 */ MEMPTR<void> out; // CURLOPT_WRITEDATA
		/* +0x014 */ MEMPTR<void> in; // CURLOPT_READDATA
		/* +0x018 */ MEMPTR<void> writeheader; // CURLOPT_HEADERDATA
		/* +0x01C */ uint8 ukn01C[0x1C];
		/* +0x038 */ MEMPTR<void> postfields;
		/* +0x03C */ uint8 ukn03C[0x4];
		/* +0x040 */ sint64be postfieldsize;
		/* +0x048 */ uint8 ukn048[0x8];
		/* +0x050 */ MEMPTR<void> fwrite_func;
		/* +0x054 */ MEMPTR<void> fwrite_header;
		/* +0x058 */ uint8 ukn058[0x4];
		/* +0x05C */ MEMPTR<void> fread_func;
		/* +0x060 */ uint8 ukn060[0x8];
		/* +0x068 */ MEMPTR<void> fprogress;
		/* +0x06C */ uint8 ukn06C[0x8];
		/* +0x074 */ MEMPTR<void> fsockopt;
		/* +0x078 */ MEMPTR<void> sockopt_client;
		/* +0x07C */ uint8 ukn07C[0x20];
		/* +0x09C */ MEMPTR<void> progress_client;
		/* +0x0A0 */ uint8 ukn0A0[0x4];
		/* +0x0A4 */ sint32be timeout; // in ms
		/* +0x0A8 */ sint32be connecttimeout; // in ms
		/* +0x0AC */ uint8 ukn0AC[0xC];
		/* +0x0B8 */ sint64be infilesize;
		/* +0x0C0 */ sint32be low_speed_limit;
		/* +0x0C4 */ sint32be low_speed_time;
		/* +0x0C8 */ sint64be max_send_speed;
		/* +0x0D0 */ sint64be max_recv_speed;
		/* +0x0D8 */ uint8 ukn0D8[0x8];
		/* +0x0E0 */ MEMPTR<WU_curl_slist> headers;
		/* +0x0E4 */ uint8 ukn0E4[0x30];
		/* +0x114 */ betype<WU_HTTPREQ> httpreq;
		/* +0x118 */ uint8 ukn118[0x40];
		/* +0x158 */ sint32be hNSSL; // -1 if no NSSL context is set
		/* +0x15C */ uint32be nsslVerifyOptions;
		/* +0x160 */ uint8 ukn160[0x8];
		/* +0x168 */ sint32be buffer_size;
		/* +0x16C */ uint8 ukn16C[0x33];
		/* +0x19F */ uint8 hide_progress;
		/* +0x1A0 */ uint8 ukn1A0[0x1];
		/* +0x1A1 */ uint8 http_follow_location;
		/* +0x1A2 */ uint8 ukn1A2[0x7];
		/* +0x1A9 */ uint8 opt_no_body;
		/* +0x1AA */ uint8 ukn1AA[0x1];
		/* +0x1AB */ uint8 upload;
		/* +0x1AC */ uint8 ukn1AC[0x18];
		/* +0x1C4 */ uint8 no_signal;
		/* +0x1C5 */ uint8 ukn1C5[0x1F];
		/* +0x1E4 */ MEMPTR<char> str[WU_STRING_LAST];
		/* +0x278 */ uint8 ukn278[0x30];

		void SetToDefault()
		{
			memset(this, 0, sizeof(WU_UserDefined));
			postfieldsize = -1;
			infilesize = -1;
			httpreq = WU_HTTPREQ::HTTPREQ_GET;
			hNSSL = -1;
			nsslVerifyOptions = NSSL_VERIFY_PEER | NSSL_VERIFY_HOSTNAME;
			hide_progress = 1;
		}
	};
	static_assert(sizeof(WU_UserDefined) == 0x2A8);

	struct CURLSH_t
	{
		/* +0x00 */ uint32be specifier;
		/* +0x04 */ uint32be dirty; // number of easy handles using this share
		/* +0x08 */ MEMPTR<void> lockfunc;
		/* +0x0C */ MEMPTR<void> unlockfunc;
		/* +0x10 */ MEMPTR<void> data;
		/* +0x14 */ uint8 ukn14[0x8];
	};
	static_assert(sizeof(CURLSH_t) == 0x1C);

	struct CURL_t
	{
		/* +0x0000 */ uint8 ukn0000[0x10];
		/* +0x0010 */ MEMPTR<CURLSH_t> share;
		/* +0x0014 */ uint8 ukn0014[0xC4];
		/* +0x00D8 */ WU_UserDefined set;
		/* +0x0380 */ uint8 ukn0380[0x829C];
		/* +0x861C */ MEMPTR<char> info_contentType; // CURLINFO_CONTENT_TYPE
		/* +0x8620 */ MEMPTR<char> info_redirectUrl; // CURLINFO_REDIRECT_URL
		/* +0x8624 */ uint8 ukn8624[0x74];
	};
	static_assert(sizeof(CURL_t) == 0x8698);
	static_assert(offsetof(CURL_t, set) == 0xD8);
	static_assert(offsetof(CURL_t, set) + offsetof(WU_UserDefined, infilesize) == 0x190);
	static_assert(offsetof(CURL_t, set) + offsetof(WU_UserDefined, httpreq) == 0x1EC);
	static_assert(offsetof(CURL_t, set) + offsetof(WU_UserDefined, hNSSL) == 0x230);
	static_assert(offsetof(CURL_t, set) + offsetof(WU_UserDefined, opt_no_body) == 0x281);

	struct CURLM_t
	{
		/* +0x00 */ uint32be magic; // CURL_MULTI_HANDLE
		/* +0x04 */ uint8 ukn04[0x7C];
	};
	static_assert(sizeof(CURLM_t) == 0x80);

	struct CURLMsg_t
	{
		uint32be msg;
		MEMPTR<CURL_t> curl;
		uint32be result; // CURLcode
	};
	static_assert(sizeof(CURLMsg_t) == 0xC);

#pragma pack(pop)

	// curl_easy_perform() blocks and curl_multi_perform() can take a while to return, so we delegate these to separate worker threads
	struct CurlWorker
	{
		OSThread_t* ppcThread;
		PPCConcurrentQueue<const std::function<uint32()>*> ppcQueue; // nullptr signals that the worker should be shutdown
		std::optional<uint32> callbackResult;
		uint32 result;
	};

	// handle wrapper to guard against races on the host CURL object. Only one PPC thread can access a handle at a time
	struct CurlHostHandle
	{
		CurlWorker worker;
		OSThread_t* usingThread{};
		uint32 useCount{};

		bool IsAccessible(bool exclusive) const
		{
			return useCount == 0 || (!exclusive && usingThread == coreinit::OSGetCurrentThread());
		}
	};

	struct CURLMHostState;

	struct CURLHostState : CurlHostHandle
	{
		CURLHostState(CURL_t* ppcCurl) : ppcCurl(ppcCurl), curl(::curl_easy_init()) {}

		~CURLHostState()
		{
			::curl_easy_cleanup(curl);
			::curl_slist_free_all(headers);
		}

		CURL_t* ppcCurl;
		CURL* curl;
		bool isDirty{true}; // set when the options of the guest handle changed and need to be synced
		curl_slist* headers{};
		// like nlibcurl we keep using the read callback that was set when the transfer started
		MEMPTR<void> readFunc;
		MEMPTR<void> readData;
		sint32 pauseMask{}; // set via curl_easy_pause
		bool progressReset{}; // set by curl_easy_reset until the next transfer starts
		CURLMHostState* multi{};
		bool multiTransferStarted{};
		MEMPTR<CURLMsg_t> multiMsg{}; // returned by curl_multi_info_read
	};

	struct CURLMHostState : CurlHostHandle
	{
		CURLMHostState() : curlm(::curl_multi_init()) {}

		~CURLMHostState()
		{
			if (curlm)
				::curl_multi_cleanup(curlm);
		}

		CURLM* curlm;
		std::vector<CURLHostState*> easyHandles;
		int runningHandles{};
	};

	std::mutex s_curlMutex;
	std::condition_variable s_workerCondVar;
	std::atomic_uint32_t s_activeWorkerCount;
	std::atomic_bool s_abortWorkers; // set when nlibcurl is unmapped
	thread_local CurlWorker* t_curlWorker{};

	// host state is looked up via the address of the guest handle
	// avoid storing host pointers in guest memory where they could be manipulated
	std::unordered_map<CURL_t*, CURLHostState*> s_curlHandles;
	std::unordered_map<CURLM_t*, CURLMHostState*> s_multiHandles;

	// an easy handle is also in use while its multi handle is in use and vice versa. Requires s_curlMutex
	bool CanAccess(CURLHostState* hostState, bool exclusive)
	{
		return hostState->IsAccessible(exclusive) && (!hostState->multi || hostState->multi->IsAccessible(exclusive));
	}

	bool CanAccess(CURLMHostState* multiHostState, bool exclusive)
	{
		if (!multiHostState->IsAccessible(exclusive))
			return false;
		for (CURLHostState* hostState : multiHostState->easyHandles)
		{
			if (!hostState->IsAccessible(exclusive))
				return false;
		}
		return true;
	}

	// looks up the host state of a guest handle and marks it as used by the current PPC thread for the lifetime of the guard
	template<typename TGuest, typename THost>
	class HostStateGuard
	{
	public:
		HostStateGuard(std::unordered_map<TGuest*, THost*>& handleMap, TGuest* handle, bool exclusive = false)
		{
			std::unique_lock _l(s_curlMutex);
			auto it = handleMap.find(handle);
			if (it == handleMap.end() || !CanAccess(it->second, exclusive))
				return;
			m_hostState = it->second;
			m_hostState->usingThread = coreinit::OSGetCurrentThread();
			m_hostState->useCount++;
		}

		~HostStateGuard()
		{
			if (!m_hostState)
				return;
			std::unique_lock _l(s_curlMutex);
			if (--m_hostState->useCount == 0)
				m_hostState->usingThread = nullptr;
		}

		HostStateGuard(const HostStateGuard&) = delete;
		HostStateGuard& operator=(const HostStateGuard&) = delete;

		explicit operator bool() const { return m_hostState != nullptr; }
		THost* operator->() const { return m_hostState; }
		THost* Get() const { return m_hostState; }

	private:
		THost* m_hostState{};
	};

	// removes the host state from the lookup and keeps it marked as in use, so related handles stay blocked until it's destroyed
	// the host side cleanup has to happen without holding s_curlMutex since curl can trigger guest callbacks during it
	template<typename TGuest, typename THost>
	THost* TakeHostState(std::unordered_map<TGuest*, THost*>& handleMap, TGuest* handle)
	{
		std::unique_lock _l(s_curlMutex);
		auto it = handleMap.find(handle);
		if (it == handleMap.end() || !CanAccess(it->second, true))
			return nullptr;
		THost* hostState = it->second;
		handleMap.erase(it);
		hostState->usingThread = coreinit::OSGetCurrentThread();
		hostState->useCount = 1;
		return hostState;
	}

	uint32 RunOnWorkerThread(CurlWorker& worker, std::function<uint32()> job)
	{
		worker.ppcThread = coreinit::OSGetCurrentThread();
		s_activeWorkerCount++;
		std::thread([&worker, job = std::move(job)]()
		{
			t_curlWorker = &worker;
			worker.result = job();
			worker.ppcQueue.push(nullptr, worker.ppcThread);
			s_activeWorkerCount--;
		}).detach();

		while (const std::function<uint32()>* callback = worker.ppcQueue.pop(worker.ppcThread))
		{
			uint32 callbackResult = (*callback)();
			std::unique_lock _l(s_curlMutex);
			worker.callbackResult = callbackResult;
			s_workerCondVar.notify_all();
		}
		return worker.result;
	}

	// guest code can only run on the PPC thread, so when called from a worker thread we forward it to the waiting PPC thread
	uint32 CallOnPPCThread(const std::function<uint32()>& func, uint32 abortResult)
	{
		if (s_abortWorkers)
			return abortResult;
		CurlWorker* worker = t_curlWorker;
		if (!worker)
			return func();
		worker->callbackResult.reset();
		worker->ppcQueue.push(&func, worker->ppcThread);
		std::unique_lock _l(s_curlMutex);
		s_workerCondVar.wait(_l, [&]() { return worker->callbackResult.has_value() || s_abortWorkers; });
		return worker->callbackResult.value_or(abortResult);
	}

	void GuestFree(void* mem)
	{
		if (mem)
			PPCCoreCallback(g_nlibcurl.free.GetMPTR(), mem);
	}

	// copy a string into guest memory
	MEMPTR<char> GuestStrdup(const char* str)
	{
		if (!str)
			return nullptr;
		uint32 size = (uint32)strlen(str) + 1;
		MEMPTR<char> ppcStr{ PPCCoreCallback(g_nlibcurl.malloc.GetMPTR(), size) };
		if (ppcStr)
			memcpy(ppcStr.GetPtr(), str, size);
		return ppcStr;
	}

	MEMPTR<char> UpdateGuestString(MEMPTR<char> ppcStr, const char* str)
	{
		if (ppcStr && str && strcmp(ppcStr.GetPtr(), str) == 0)
			return ppcStr;
		GuestFree(ppcStr);
		return GuestStrdup(str);
	}

	void SetGuestString(CURL_t* curl, WU_StringIndex index, const char* str)
	{
		GuestFree(curl->set.str[index]);
		curl->set.str[index] = GuestStrdup(str);
	}

	void FreeGuestStrings(CURL_t* curl)
	{
		for (uint32 i = 0; i < WU_STRING_LAST; i++)
			SetGuestString(curl, (WU_StringIndex)i, nullptr);
	}

	// sharing data between handles is not implemented, we only keep track of which handles use a share
	void SetShare(CURL_t* curl, CURLSH_t* share)
	{
		if (curl->share)
			curl->share->dirty = curl->share->dirty - 1;
		curl->share = share;
		if (share)
			share->dirty = share->dirty + 1;
	}

	int CloseSocketCallback(void* clientp, curl_socket_t item)
	{
		nsysnet_notifyCloseSharedSocket((SOCKET)item);
		closesocket(item);
		return 0;
	}

	size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata)
	{
		CURL_t* curl = ((CURLHostState*)userdata)->ppcCurl;
		if (curl->set.fwrite_header.IsNull())
			return size * nitems;
		return CallOnPPCThread([&]() -> uint32
		{
			StackAllocator<char> tmp((uint32)(size * nitems));
			memcpy(tmp.GetPointer(), buffer, size * nitems);
			return PPCCoreCallback(curl->set.fwrite_header.GetMPTR(), tmp.GetMPTR(), (uint32)size, (uint32)nitems, curl->set.writeheader);
		}, 0);
	}

	size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
	{
		CURLHostState* hostState = (CURLHostState*)userdata;
		CURL_t* curl = hostState->ppcCurl;
		cemuLog_logDebug(LogType::Force, "WriteCallback(0x{} 0x{:x}, 0x{:x}, 0x{:08x})", (void*)ptr, size, nmemb, curl->set.out.GetMPTR());
		// curl may still deliver data after receiving was paused via curl_easy_pause, so we let curl buffer it until unpaused
		if (HAS_FLAG(hostState->pauseMask, CURLPAUSE_RECV))
			return CURL_WRITEFUNC_PAUSE;
		if (curl->set.fwrite_func.IsNull())
			return size * nmemb;
		return CallOnPPCThread([&]() -> uint32
		{
			StackAllocator<char> tmp((uint32)(size * nmemb));
			memcpy(tmp.GetPointer(), ptr, size * nmemb);
			return PPCCoreCallback(curl->set.fwrite_func.GetMPTR(), tmp.GetMPTR(), (uint32)size, (uint32)nmemb, curl->set.out);
		}, 0);
	}

	size_t ReadCallback(char* buffer, size_t size, size_t nitems, void* instream)
	{
		CURLHostState* hostState = (CURLHostState*)instream;
		if (hostState->readFunc.IsNull())
			return 0;
		nitems = std::min<size_t>(nitems, 0x4000); // limit this to 16KB which is the limit in nlibcurl.rpl (Super Mario Maker crashes on level upload if the size is too big)
		cemuLog_logDebug(LogType::Force, "ReadCallback(0x{}, 0x{:x}, 0x{:x}, 0x{:08x}) [func: 0x{:x}]", (void*)buffer, size, nitems, hostState->readData.GetMPTR(), hostState->readFunc.GetMPTR());
		return CallOnPPCThread([&]() -> uint32
		{
			StackAllocator<char> tmp((uint32)(size * nitems));
			uint32 result = PPCCoreCallback(hostState->readFunc.GetMPTR(), tmp.GetMPTR(), (uint32)size, (uint32)nitems, hostState->readData);
			if (result <= size * nitems) // CURL_READFUNC_ABORT and CURL_READFUNC_PAUSE are not byte counts
				memcpy(buffer, tmp.GetPointer(), result);
			return result;
		}, CURL_READFUNC_ABORT);
	}

	int SockoptCallback(void* clientp, curl_socket_t curlfd, curlsocktype purpose)
	{
		CURL_t* curl = ((CURLHostState*)clientp)->ppcCurl;
		if (curl->set.fsockopt.IsNull())
			return CURL_SOCKOPT_OK;
		return (int)CallOnPPCThread([&]() -> uint32
		{
			WUSOCKET s = nsysnet_getVirtualSocketHandleFromHostHandle((SOCKET)curlfd);
			if (s < 0)
				s = nsysnet_createVirtualSocketFromExistingSocket((SOCKET)curlfd, true);
			return PPCCoreCallback(curl->set.fsockopt.GetMPTR(), curl->set.sockopt_client, s, (uint32)purpose);
		}, CURL_SOCKOPT_ERROR);
	}

	int ProgressCallback(void* clientp, double dltotal, double dlnow, double ultotal, double ulnow)
	{
		if (s_abortWorkers)
			return 1;
		CURL_t* curl = ((CURLHostState*)clientp)->ppcCurl;
		if (curl->set.hide_progress || curl->set.fprogress.IsNull())
			return 0;
		return (int)CallOnPPCThread([&]() -> uint32
		{
			return PPCCoreCallback(curl->set.fprogress.GetMPTR(), curl->set.progress_client, dltotal, dlnow, ultotal, ulnow);
		}, 1);
	}

	// does not call into guest code, so we can run this directly on the worker thread
	CURLcode SSLContextCallback(CURL* curl, void* sslctx, void* param)
	{
		CURL_t* ppcCurl = ((CURLHostState*)param)->ppcCurl;
		std::lock_guard lock(nsysnet::s_nsslContextMutex);
		nsysnet::NSSLInternalState_t* nssl = nsysnet::GetNSSLContext(ppcCurl->set.hNSSL);
		if (!nssl)
		{
			cemuLog_log(LogType::Force, "CURL invalid NSSL context");
			return CURLE_SSL_CACERT;
		}

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
			if (iosuCrypto_addCustomCACertificate(sslctx, customPKI.data(), (sint32)customPKI.size()) == false)
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

		uint32 flags = ppcCurl->set.nsslVerifyOptions;
		// certificate dates are only checked if requested
		X509_VERIFY_PARAM* verifyParam = ::SSL_CTX_get0_param((SSL_CTX*)sslctx);
		if (HAS_FLAG(flags, NSSL_VERIFY_DATE))
			::X509_VERIFY_PARAM_clear_flags(verifyParam, X509_V_FLAG_NO_CHECK_TIME);
		else
			::X509_VERIFY_PARAM_set_flags(verifyParam, X509_V_FLAG_NO_CHECK_TIME);
		if (HAS_FLAG(flags, NSSL_VERIFY_PEER))
		{
			::SSL_CTX_set_cipher_list((SSL_CTX*)sslctx, "AES256-SHA"); // TLS_RSA_WITH_AES_256_CBC_SHA (in CURL it's called rsa_aes_256_sha)
			::SSL_CTX_set_mode((SSL_CTX*)sslctx, SSL_MODE_AUTO_RETRY);
			::SSL_CTX_set_verify_depth((SSL_CTX*)sslctx, 2);
			::SSL_CTX_set_verify((SSL_CTX*)sslctx, SSL_VERIFY_PEER, nullptr); // the handshake fails if the certificate can't be verified
		}
		else
		{
			::SSL_CTX_set_verify((SSL_CTX*)sslctx, SSL_VERIFY_NONE, nullptr);
		}

		return CURLE_OK;
	}

	// options are tracked in the PPC CURL object and only synced to the host CURL object right before easy_perform/multi_perform
	bool SyncHostParameters(CURLHostState* hostState)
	{
		if (!hostState->isDirty)
			return true;
		CURL_t* curl = hostState->ppcCurl;
		CURL* hostCurl = hostState->curl;
		WU_UserDefined& set = curl->set;
		const bool isPost = !set.upload && set.httpreq == WU_HTTPREQ::HTTPREQ_POST;

		// CURLOPT_HTTPHEADER
		// for more recent libcurl manually adding Content-Length header is not supported, we remove it from the header list and instead set CURLOPT_POSTFIELDSIZE/CURLOPT_INFILESIZE
		sint64 manualContentLength = -1;
		curl_slist* headers = nullptr;
		for (WU_curl_slist* ppcList = set.headers; ppcList; ppcList = ppcList->next)
		{
			if (isPost && strncmp(ppcList->data.GetPtr(), "Content-Length:", 15) == 0)
			{
				manualContentLength = std::strtoll(ppcList->data.GetPtr() + 15, nullptr, 10);
				continue;
			}
			headers = ::curl_slist_append(headers, ppcList->data.GetPtr());
		}
		sint64 postFieldSize = manualContentLength >= 0 ? manualContentLength : (sint64)set.postfieldsize;
		sint64 inFileSize = set.infilesize != -1 ? (sint64)set.infilesize : manualContentLength;
		// curl reads the POST data directly from guest memory, make sure it stays within it
		cemu_assert(manualContentLength < 0 || (uint64)set.postfields.GetMPTR() + (uint64)postFieldSize <= 0x100000000ULL);
		::curl_easy_setopt(hostCurl, CURLOPT_HTTPHEADER, headers);
		::curl_slist_free_all(hostState->headers);
		hostState->headers = headers;

		// callbacks
		hostState->readFunc = set.fread_func;
		hostState->readData = set.in;
		::curl_easy_setopt(hostCurl, CURLOPT_PRIVATE, hostState);
		::curl_easy_setopt(hostCurl, CURLOPT_HEADERFUNCTION, HeaderCallback);
		::curl_easy_setopt(hostCurl, CURLOPT_HEADERDATA, hostState);
		::curl_easy_setopt(hostCurl, CURLOPT_WRITEFUNCTION, WriteCallback);
		::curl_easy_setopt(hostCurl, CURLOPT_WRITEDATA, hostState);
		::curl_easy_setopt(hostCurl, CURLOPT_READFUNCTION, ReadCallback);
		::curl_easy_setopt(hostCurl, CURLOPT_READDATA, hostState);
		::curl_easy_setopt(hostCurl, CURLOPT_SOCKOPTFUNCTION, SockoptCallback);
		::curl_easy_setopt(hostCurl, CURLOPT_SOCKOPTDATA, hostState);
		::curl_easy_setopt(hostCurl, CURLOPT_CLOSESOCKETFUNCTION, CloseSocketCallback);
		::curl_easy_setopt(hostCurl, CURLOPT_CLOSESOCKETDATA, nullptr);
		// progress callback is always enabled (used to sync curl pause state)
		::curl_easy_setopt(hostCurl, CURLOPT_NOPROGRESS, 0L);
		::curl_easy_setopt(hostCurl, CURLOPT_PROGRESSFUNCTION, ProgressCallback);
		::curl_easy_setopt(hostCurl, CURLOPT_PROGRESSDATA, hostState);

		::curl_easy_setopt(hostCurl, CURLOPT_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
		::curl_easy_setopt(hostCurl, CURLOPT_REDIR_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
		::curl_easy_setopt(hostCurl, CURLOPT_URL, set.str[WU_STRING_URL].GetPtr());
		::curl_easy_setopt(hostCurl, CURLOPT_PROXY, set.str[WU_STRING_PROXY].GetPtr());
		::curl_easy_setopt(hostCurl, CURLOPT_USERAGENT, set.str[WU_STRING_USERAGENT].GetPtr());
		::curl_easy_setopt(hostCurl, CURLOPT_NOSIGNAL, (long)set.no_signal);
		::curl_easy_setopt(hostCurl, CURLOPT_FOLLOWLOCATION, (long)set.http_follow_location);
		::curl_easy_setopt(hostCurl, CURLOPT_BUFFERSIZE, (long)set.buffer_size);
		::curl_easy_setopt(hostCurl, CURLOPT_TIMEOUT_MS, (long)set.timeout);
		::curl_easy_setopt(hostCurl, CURLOPT_CONNECTTIMEOUT_MS, (long)set.connecttimeout);
		::curl_easy_setopt(hostCurl, CURLOPT_LOW_SPEED_LIMIT, (long)set.low_speed_limit);
		::curl_easy_setopt(hostCurl, CURLOPT_LOW_SPEED_TIME, (long)set.low_speed_time);
		::curl_easy_setopt(hostCurl, CURLOPT_MAX_SEND_SPEED_LARGE, (curl_off_t)set.max_send_speed);
		::curl_easy_setopt(hostCurl, CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t)set.max_recv_speed);

		// nssl
		uint32 verifyOptions = set.nsslVerifyOptions;
		::curl_easy_setopt(hostCurl, CURLOPT_SSL_VERIFYPEER, HAS_FLAG(verifyOptions, NSSL_VERIFY_PEER) ? 1L : 0L);
		::curl_easy_setopt(hostCurl, CURLOPT_SSL_VERIFYHOST, HAS_FLAG(verifyOptions, NSSL_VERIFY_HOSTNAME) ? 2L : 0L);
		if ((sint32)set.hNSSL != -1)
		{
			::curl_easy_setopt(hostCurl, CURLOPT_SSL_CTX_FUNCTION, SSLContextCallback);
			::curl_easy_setopt(hostCurl, CURLOPT_SSL_CTX_DATA, hostState);
			std::lock_guard lock(nsysnet::s_nsslContextMutex);
			nsysnet::NSSLInternalState_t* nssl = nsysnet::GetNSSLContext(set.hNSSL);
			if (nssl && nssl->sslVersion == 2)
				::curl_easy_setopt(hostCurl, CURLOPT_SSLVERSION, (long)CURL_SSLVERSION_SSLv3);
			else // auto = highest = 0 || 2 for v3
				::curl_easy_setopt(hostCurl, CURLOPT_SSLVERSION, (long)CURL_SSLVERSION_TLSv1_2);
		}
		else
		{
			::curl_easy_setopt(hostCurl, CURLOPT_SSL_CTX_FUNCTION, nullptr);
			::curl_easy_setopt(hostCurl, CURLOPT_SSLVERSION, (long)CURL_SSLVERSION_DEFAULT);
		}

		// http request type
		::curl_easy_setopt(hostCurl, CURLOPT_POSTFIELDS, set.postfields.GetPtr());
		::curl_easy_setopt(hostCurl, CURLOPT_UPLOAD, (long)set.upload);
		if (!set.upload)
		{
			if (set.httpreq == WU_HTTPREQ::HTTPREQ_GET)
				::curl_easy_setopt(hostCurl, CURLOPT_HTTPGET, 1L);
			else if (isPost)
				::curl_easy_setopt(hostCurl, CURLOPT_POST, 1L);
			else
				cemu_assert_unimplemented();
		}
		::curl_easy_setopt(hostCurl, CURLOPT_NOBODY, (long)set.opt_no_body);
		::curl_easy_setopt(hostCurl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)postFieldSize);
		::curl_easy_setopt(hostCurl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)inFileSize);

		hostState->isDirty = false;
		return true;
	}

	void* DefaultMalloc(uint32 size)
	{
		return coreinit::_weak_MEMAllocFromDefaultHeap(size);
	}

	void* DefaultCalloc(uint32 count, uint32 size)
	{
		void* mem = coreinit::_weak_MEMAllocFromDefaultHeap(count * size);
		if (mem)
			memset(mem, 0, count * size);
		return mem;
	}

	void DefaultFree(void* mem)
	{
		coreinit::_weak_MEMFreeToDefaultHeap(mem);
	}

	char* DefaultStrdup(const char* str)
	{
		uint32 size = (uint32)strlen(str) + 1;
		char* mem = (char*)coreinit::_weak_MEMAllocFromDefaultHeap(size);
		if (mem)
			memcpy(mem, str, size);
		return mem;
	}

	CURLcode curl_global_init(uint32 flags)
	{
		if (g_nlibcurl.initialized++)
			return CURLE_OK;

		g_nlibcurl.malloc = makeCallableExport<DefaultMalloc>();
		g_nlibcurl.calloc = makeCallableExport<DefaultCalloc>();
		g_nlibcurl.free = makeCallableExport<DefaultFree>();
		g_nlibcurl.strdup = makeCallableExport<DefaultStrdup>();

		return ::curl_global_init(flags);
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
				g_nlibcurl.strdup = strdup_callback;
				g_nlibcurl.calloc = calloc_callback;
			}
		}
		return result;
	}

	CURLM_t* curl_multi_init()
	{
		if (g_nlibcurl.initialized == 0 && curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
			return nullptr;

		MEMPTR<CURLM_t> curlm{ PPCCoreCallback(g_nlibcurl.calloc.GetMPTR(), 1, ppcsizeof<CURLM_t>()) };
		if (curlm)
		{
			curlm->magic = CURL_MULTI_HANDLE;
			std::unique_lock _l(s_curlMutex);
			cemu_assert(!s_multiHandles.contains(curlm.GetPtr())); // previous handle with same address still live, should not happen as it requires memory to be double-allocated
			s_multiHandles.emplace(curlm.GetPtr(), new CURLMHostState());
		}
		return curlm;
	}

	CURLMcode curl_multi_add_handle(CURLM_t* curlm, CURL_t* curl)
	{
		HostStateGuard multiHostState(s_multiHandles, curlm, true);
		if (!multiHostState)
			return CURLM_BAD_HANDLE;
		HostStateGuard hostState(s_curlHandles, curl, true);
		if (!hostState || hostState->multi)
			return CURLM_BAD_EASY_HANDLE;
		CURLMcode result = ::curl_multi_add_handle(multiHostState->curlm, hostState->curl);
		if (result == CURLM_OK)
		{
			std::unique_lock _l(s_curlMutex);
			multiHostState->easyHandles.push_back(hostState.Get());
			hostState->multi = multiHostState.Get();
			hostState->multiTransferStarted = false;
		}
		return result;
	}

	CURLMcode curl_multi_remove_handle(CURLM_t* curlm, CURL_t* curl)
	{
		HostStateGuard multiHostState(s_multiHandles, curlm, true);
		if (!multiHostState)
			return CURLM_BAD_HANDLE;
		HostStateGuard hostState(s_curlHandles, curl);
		if (!hostState || hostState->multi != multiHostState.Get())
			return CURLM_BAD_EASY_HANDLE;
		CURLMcode result = ::curl_multi_remove_handle(multiHostState->curlm, hostState->curl);
		if (result == CURLM_OK)
		{
			std::unique_lock _l(s_curlMutex);
			std::erase(multiHostState->easyHandles, hostState.Get());
			hostState->multi = nullptr;
		}
		return result;
	}

	CURLMcode curl_multi_timeout(CURLM_t* curlm, sint32be* timeout)
	{
		HostStateGuard multiHostState(s_multiHandles, curlm, true);
		if (!multiHostState)
			return CURLM_BAD_HANDLE;
		long hostTimeout = -1;
		CURLMcode result = ::curl_multi_timeout(multiHostState->curlm, &hostTimeout);
		*timeout = (sint32)hostTimeout;
		return result;
	}

	CURLMcode curl_multi_cleanup(CURLM_t* curlm)
	{
		CURLMHostState* multiHostState = TakeHostState(s_multiHandles, curlm);
		if (!multiHostState)
			return CURLM_BAD_HANDLE;
		// the attached easy handles are blocked until they are detached
		::curl_multi_cleanup(multiHostState->curlm);
		multiHostState->curlm = nullptr;
		{
			std::unique_lock _l(s_curlMutex);
			for (CURLHostState* hostState : multiHostState->easyHandles)
				hostState->multi = nullptr;
		}
		delete multiHostState;
		GuestFree(curlm);
		return CURLM_OK;
	}

	CURLMcode curl_multi_perform(CURLM_t* curlm, sint32be* runningHandles)
	{
		HostStateGuard multiHostState(s_multiHandles, curlm, true);
		if (!multiHostState)
			return CURLM_BAD_HANDLE;
		for (CURLHostState* hostState : multiHostState->easyHandles)
		{
			if (hostState->multiTransferStarted)
				continue;
			if (!SyncHostParameters(hostState))
				return CURLM_INTERNAL_ERROR;
			hostState->pauseMask = 0;
			hostState->progressReset = false;
			hostState->multiTransferStarted = true;
		}
		CURLMcode result = (CURLMcode)RunOnWorkerThread(multiHostState->worker, [multiHost = multiHostState.Get()]() -> uint32
		{
			return ::curl_multi_perform(multiHost->curlm, &multiHost->runningHandles);
		});
		*runningHandles = multiHostState->runningHandles;
		return result;
	}

	CURLMcode curl_multi_fdset(CURLM_t* curlm, wu_fd_set* readFd, wu_fd_set* writeFd, wu_fd_set* exceptionFd, sint32be* maxFd)
	{
		HostStateGuard multiHostState(s_multiHandles, curlm, true);
		if (!multiHostState)
			return CURLM_BAD_HANDLE;

		fd_set hostReadFd;
		fd_set hostWriteFd;
		fd_set hostExceptionFd;
		FD_ZERO(&hostReadFd);
		FD_ZERO(&hostWriteFd);
		FD_ZERO(&hostExceptionFd);
		int hostMaxFd = 0;

		CURLMcode result = ::curl_multi_fdset(multiHostState->curlm, &hostReadFd, &hostWriteFd, &hostExceptionFd, &hostMaxFd);

		nsysnet::wuResetFD(readFd);
		nsysnet::wuResetFD(writeFd);
		nsysnet::wuResetFD(exceptionFd);

		sint32 guestMaxFd = -1;
		auto hostFdSet = [&](SOCKET s, wu_fd_set* fds)
		{
			sint32 wuSocket = nsysnet_getVirtualSocketHandleFromHostHandle(s);
			if (wuSocket < 0)
				wuSocket = nsysnet_createVirtualSocketFromExistingSocket(s, true);
			if (wuSocket >= 0)
			{
				guestMaxFd = std::max(wuSocket, guestMaxFd);
				nsysnet::wuSetFD(fds, wuSocket);
			}
		};

#if BOOST_OS_UNIX
		for (int s = 0; s < hostMaxFd + 1; s++)
		{
			if (FD_ISSET(s, &hostReadFd))
				hostFdSet(s, readFd);
			if (FD_ISSET(s, &hostWriteFd))
				hostFdSet(s, writeFd);
			if (FD_ISSET(s, &hostExceptionFd))
				hostFdSet(s, exceptionFd);
		}
#else
		for (uint32 i = 0; i < hostReadFd.fd_count; i++)
			hostFdSet(hostReadFd.fd_array[i], readFd);
		for (uint32 i = 0; i < hostWriteFd.fd_count; i++)
			hostFdSet(hostWriteFd.fd_array[i], writeFd);
		for (uint32 i = 0; i < hostExceptionFd.fd_count; i++)
			hostFdSet(hostExceptionFd.fd_array[i], exceptionFd);
#endif

		*maxFd = guestMaxFd;
		return result;
	}

	CURLMcode curl_multi_setopt(CURLM_t* curlm, uint32 option, uint32 parameter)
	{
		HostStateGuard multiHostState(s_multiHandles, curlm, true);
		if (!multiHostState)
			return CURLM_BAD_HANDLE;
		if (option == CURLMOPT_MAXCONNECTS)
			return ::curl_multi_setopt(multiHostState->curlm, CURLMOPT_MAXCONNECTS, (long)parameter);
		cemuLog_logDebug(LogType::Force, "curl_multi_setopt(): Unsupported option {}", option);
		return CURLM_OK;
	}

	CURLMsg_t* curl_multi_info_read(CURLM_t* curlm, sint32be* msgsInQueue)
	{
		HostStateGuard multiHostState(s_multiHandles, curlm, true);
		if (!multiHostState)
			return nullptr;
		int hostMsgsInQueue = 0;
		CURLMsg* msg = ::curl_multi_info_read(multiHostState->curlm, &hostMsgsInQueue);
		*msgsInQueue = hostMsgsInQueue;
		if (!msg)
			return nullptr;

		CURLHostState* hostState = nullptr;
		::curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &hostState);
		// like in nlibcurl the message is stored per easy handle and stays valid until the handle is destroyed
		if (!hostState->multiMsg)
			hostState->multiMsg = PPCCoreCallback(g_nlibcurl.malloc.GetMPTR(), ppcsizeof<CURLMsg_t>());
		hostState->multiMsg->msg = msg->msg;
		hostState->multiMsg->curl = hostState->ppcCurl;
		hostState->multiMsg->result = msg->data.result;
		return hostState->multiMsg;
	}

	CURLSH_t* curl_share_init()
	{
		MEMPTR<CURLSH_t> share{ PPCCoreCallback(g_nlibcurl.calloc.GetMPTR(), 1, ppcsizeof<CURLSH_t>()) };
		if (share)
			share->specifier = 1u << CURL_LOCK_DATA_SHARE;
		return share;
	}

	CURLSHcode curl_share_setopt(CURLSH_t* share, uint32 option, uint32 parameter)
	{
		if (!share)
			return CURLSHE_INVALID;
		if (share->dirty)
			return CURLSHE_IN_USE;

		switch (option)
		{
			case CURLSHOPT_USERDATA:
				share->data = parameter;
				break;
			case CURLSHOPT_LOCKFUNC:
				share->lockfunc = parameter;
				break;
			case CURLSHOPT_UNLOCKFUNC:
				share->unlockfunc = parameter;
				break;
			case CURLSHOPT_SHARE:
				if (parameter >= 32)
					return CURLSHE_BAD_OPTION;
				share->specifier = share->specifier | (1u << parameter);
				if (parameter != CURL_LOCK_DATA_COOKIE && parameter != CURL_LOCK_DATA_DNS)
					return CURLSHE_BAD_OPTION;
				break;
			case CURLSHOPT_UNSHARE:
				if (parameter >= 32)
					return CURLSHE_BAD_OPTION;
				share->specifier = share->specifier & ~(1u << parameter);
				if (parameter < CURL_LOCK_DATA_COOKIE || parameter > CURL_LOCK_DATA_CONNECT)
					return CURLSHE_BAD_OPTION;
				break;
			default:
				return CURLSHE_BAD_OPTION;
		}
		return CURLSHE_OK;
	}

	CURLSHcode curl_share_cleanup(CURLSH_t* share)
	{
		if (!share)
			return CURLSHE_INVALID;
		if (share->dirty)
			return CURLSHE_IN_USE;
		GuestFree(share);
		return CURLSHE_OK;
	}

	CURL_t* curl_easy_init()
	{
		if (g_nlibcurl.initialized == 0 && curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
			return nullptr;

		// Curl_open
		MEMPTR<CURL_t> curl{ PPCCoreCallback(g_nlibcurl.calloc.GetMPTR(), 1, ppcsizeof<CURL_t>()) };
		if (curl)
		{
			curl->set.SetToDefault();
			std::unique_lock _l(s_curlMutex);
			if (s_curlHandles.contains(curl.GetPtr()))
				return nullptr; // address still belongs to another easy handle
			s_curlHandles.emplace(curl.GetPtr(), new CURLHostState(curl));
		}
		return curl;
	}

	CURL_t* mw_curl_easy_init()
	{
		return curl_easy_init();
	}

	void curl_easy_cleanup(CURL_t* curl)
	{
		CURLHostState* hostState = TakeHostState(s_curlHandles, curl);
		if (!hostState)
			return;
		// the multi handle stays blocked while this handle is still attached to it
		if (CURLMHostState* multiHostState = hostState->multi)
		{
			::curl_multi_remove_handle(multiHostState->curlm, hostState->curl);
			std::unique_lock _l(s_curlMutex);
			std::erase(multiHostState->easyHandles, hostState);
		}
		GuestFree(hostState->multiMsg);
		delete hostState;

		SetShare(curl, nullptr);
		FreeGuestStrings(curl);
		GuestFree(curl->info_contentType);
		GuestFree(curl->info_redirectUrl);
		GuestFree(curl);
	}

	void curl_easy_reset(CURL_t* curl)
	{
		HostStateGuard hostState(s_curlHandles, curl, true);
		if (!hostState)
			return;
		// curl_apply_default_proxy_config();
		FreeGuestStrings(curl);
		curl->set.SetToDefault();
		// the host handle is not reset, since nlibcurl keeps the info of the last transfer (response code etc) after a reset
		// only the progress is cleared. All options we use are applied again when the next transfer starts
		hostState->isDirty = true;
		hostState->progressReset = true;
	}

	CURLcode curl_easy_pause(CURL_t* curl, sint32 bitmask)
	{
		// the handle is either idle or in use by this thread, in which case we are inside a curl callback and the worker thread is blocked until it returns
		HostStateGuard hostState(s_curlHandles, curl);
		if (!hostState)
			return CURLE_BAD_FUNCTION_ARGUMENT;
		// update the mask first, since unpausing can replay buffered data to the write callback right away
		sint32 previousMask = hostState->pauseMask;
		hostState->pauseMask = bitmask;
		CURLcode result = ::curl_easy_pause(hostState->curl, bitmask);
		if (result != CURLE_OK)
			hostState->pauseMask = previousMask;
		return result;
	}

	// note: curl_easy_setopt is a macro in curl header, so we avoid reusing the same name for the HLE function
	CURLcode nlibcurl_easy_setopt(MEMPTR<CURL_t> curl, uint32 option)
	{
		ppc_define_va_list(2, 0);

		HostStateGuard hostState(s_curlHandles, curl.GetPtr());
		if (!hostState)
			return CURLE_BAD_FUNCTION_ARGUMENT;
		// 64bit arguments are in 30000-39999 range
		uint64 value64 = 0;
		if (option >= 30000 && option < 39999)
			value64 = *(uint64be*)_ppc_va_arg(&vargs, ppc_va_type::INT64);
		else
			value64 = (uint32)*(uint32be*)_ppc_va_arg(&vargs, ppc_va_type::INT32);
		uint32 value32 = (uint32)value64;
		MEMPTR<void> valuePtr{value32};
		WU_UserDefined& set = curl->set;
		hostState->isDirty = true;

		switch ((WU_CURLOPT)option)
		{
			case WU_CURLOPT::UPLOAD:
			case WU_CURLOPT::PUT:
			{
				set.upload = value32 != 0;
				if (value32)
				{
					set.httpreq = WU_HTTPREQ::HTTPREQ_PUT;
					set.opt_no_body = 0;
				}
				else
					set.httpreq = WU_HTTPREQ::HTTPREQ_GET;
				break;
			}
			case WU_CURLOPT::POST:
			{
				if (value32)
				{
					set.httpreq = WU_HTTPREQ::HTTPREQ_POST;
					set.opt_no_body = 0;
				}
				else
					set.httpreq = WU_HTTPREQ::HTTPREQ_GET;
				break;
			}
			case WU_CURLOPT::HTTPGET:
			{
				if (value32)
				{
					set.httpreq = WU_HTTPREQ::HTTPREQ_GET;
					set.opt_no_body = 0;
					set.upload = 0;
				}
				break;
			}
			case WU_CURLOPT::INFILESIZE:
				set.infilesize = (sint64)(sint32)value32;
				break;
			case WU_CURLOPT::INFILESIZE_LARGE:
				set.infilesize = (sint64)value64;
				break;
			case WU_CURLOPT::NOSIGNAL:
				set.no_signal = value32 != 0;
				break;
			case WU_CURLOPT::FOLLOWLOCATION:
				set.http_follow_location = value32 != 0;
				break;
			case WU_CURLOPT::BUFFERSIZE:
				set.buffer_size = (value32 >= 1 && value32 < 0x4000) ? value32 : 0; // 0 selects the default size of 16KB
				break;
			case WU_CURLOPT::TIMEOUT:
				set.timeout = value32 * 1000;
				break;
			case WU_CURLOPT::CONNECTTIMEOUT:
				set.connecttimeout = value32 * 1000;
				break;
			case WU_CURLOPT::CONNECTTIMEOUT_MS:
				set.connecttimeout = value32;
				break;
			case WU_CURLOPT::NOPROGRESS:
				set.hide_progress = value32 != 0;
				break;
			case WU_CURLOPT::LOW_SPEED_LIMIT:
				set.low_speed_limit = value32;
				break;
			case WU_CURLOPT::LOW_SPEED_TIME:
				set.low_speed_time = value32;
				break;
			case WU_CURLOPT::MAX_SEND_SPEED_LARGE:
				set.max_send_speed = (sint64)value64;
				break;
			case WU_CURLOPT::MAX_RECV_SPEED_LARGE:
				set.max_recv_speed = (sint64)value64;
				break;
			case WU_CURLOPT::URL:
				cemuLog_logDebug(LogType::Force, "curl_easy_setopt(URL) [{}]", (const char*)valuePtr.GetPtr());
				SetGuestString(curl, WU_STRING_URL, (const char*)valuePtr.GetPtr());
				break;
			case WU_CURLOPT::PROXY:
				SetGuestString(curl, WU_STRING_PROXY, (const char*)valuePtr.GetPtr());
				break;
			case WU_CURLOPT::USERAGENT:
				SetGuestString(curl, WU_STRING_USERAGENT, (const char*)valuePtr.GetPtr());
				break;
			case WU_CURLOPT::POSTFIELDS:
				set.postfields = valuePtr;
				set.httpreq = WU_HTTPREQ::HTTPREQ_POST;
				break;
			case WU_CURLOPT::POSTFIELDSIZE_LARGE:
				set.postfieldsize = (sint64)value64;
				break;
			case WU_CURLOPT::NSSL_CONTEXT:
				set.hNSSL = value32;
				break;
			case WU_CURLOPT::NSSL_VERIFY_OPTIONS:
				set.nsslVerifyOptions = value32;
				break;
			case WU_CURLOPT::SHARE:
				SetShare(curl, MEMPTR<CURLSH_t>(value32));
				break;
			case WU_CURLOPT::HEADERFUNCTION:
				set.fwrite_header = valuePtr;
				break;
			case WU_CURLOPT::HEADERDATA:
				set.writeheader = valuePtr;
				break;
			case WU_CURLOPT::WRITEFUNCTION:
				set.fwrite_func = valuePtr;
				break;
			case WU_CURLOPT::WRITEDATA: // aka CURLOPT_FILE
				set.out = valuePtr;
				break;
			case WU_CURLOPT::HTTPHEADER:
				set.headers = (WU_curl_slist*)valuePtr.GetPtr();
				break;
			case WU_CURLOPT::SOCKOPTFUNCTION:
				set.fsockopt = valuePtr;
				break;
			case WU_CURLOPT::SOCKOPTDATA:
				set.sockopt_client = valuePtr;
				break;
			case WU_CURLOPT::READFUNCTION:
				set.fread_func = valuePtr;
				break;
			case WU_CURLOPT::READDATA:
				set.in = valuePtr;
				break;
			case WU_CURLOPT::PROGRESSFUNCTION:
				set.fprogress = valuePtr;
				break;
			case WU_CURLOPT::PROGRESSDATA:
				set.progress_client = valuePtr;
				break;
			default:
				cemuLog_logDebug(LogType::Force, "curl_easy_setopt(0x{:08x}, {}, 0x{:08x}) unsupported option", curl.GetMPTR(), option, value32);
		}

		cemuLog_logDebug(LogType::Force, "curl_easy_setopt(0x{:08x}, {}, 0x{:08x})", curl.GetMPTR(), option, value32);
		return CURLE_OK;
	}

	WU_CURLcode curl_easy_perform(CURL_t* curl)
	{
		HostStateGuard hostState(s_curlHandles, curl, true);
		if (!hostState || !SyncHostParameters(hostState.Get()))
			return static_cast<WU_CURLcode>(CURLE_BAD_FUNCTION_ARGUMENT);
		hostState->pauseMask = 0;
		hostState->progressReset = false;
		CURL* hostCurl = hostState->curl;
		const uint32 result = RunOnWorkerThread(hostState->worker, [hostCurl]() -> uint32
		{
			return ::curl_easy_perform(hostCurl);
		});
		return static_cast<WU_CURLcode>(result);
	}

	CURLcode curl_easy_getinfo(CURL_t* curl, uint32 info, void* parameter)
	{
		HostStateGuard hostState(s_curlHandles, curl);
		if (!hostState || !parameter)
			return CURLE_BAD_FUNCTION_ARGUMENT;
		CURL* hostCurl = hostState->curl;
		CURLcode result;
		switch ((WU_CURLINFO)info)
		{
			case WU_CURLINFO::SIZE_DOWNLOAD:
			case WU_CURLINFO::SPEED_DOWNLOAD:
			case WU_CURLINFO::SIZE_UPLOAD:
			case WU_CURLINFO::SPEED_UPLOAD:
			case WU_CURLINFO::CONTENT_LENGTH_DOWNLOAD:
			{
				double tempDouble = 0.0;
				result = ::curl_easy_getinfo(hostCurl, (CURLINFO)info, &tempDouble);
				if (hostState->progressReset)
					tempDouble = (WU_CURLINFO)info == WU_CURLINFO::CONTENT_LENGTH_DOWNLOAD ? -1.0 : 0.0;
				*(float64be*)parameter = tempDouble;
				break;
			}
			case WU_CURLINFO::RESPONSE_CODE:
			case WU_CURLINFO::SSL_VERIFYRESULT:
			{
				long tempLong = 0;
				result = ::curl_easy_getinfo(hostCurl, (CURLINFO)info, &tempLong);
				*(sint32be*)parameter = (sint32)tempLong;
				break;
			}
			case WU_CURLINFO::CONTENT_TYPE:
			{
				char* contentType = nullptr;
				result = ::curl_easy_getinfo(hostCurl, CURLINFO_CONTENT_TYPE, &contentType);
				curl->info_contentType = UpdateGuestString(curl->info_contentType, contentType);
				*(MEMPTR<char>*)parameter = curl->info_contentType;
				break;
			}
			case WU_CURLINFO::REDIRECT_URL:
			{
				char* redirectUrl = nullptr;
				result = ::curl_easy_getinfo(hostCurl, CURLINFO_REDIRECT_URL, &redirectUrl);
				curl->info_redirectUrl = UpdateGuestString(curl->info_redirectUrl, redirectUrl);
				*(MEMPTR<char>*)parameter = curl->info_redirectUrl;
				break;
			}
			default:
				cemu_assert_unimplemented();
				result = CURLE_BAD_FUNCTION_ARGUMENT;
		}
		return result;
	}

	const char* curl_easy_strerror(uint32 code)
	{
		const char* error = ::curl_easy_strerror((CURLcode)code);
		if (!error)
			return nullptr;
		cemuLog_logDebug(LogType::Force, "curl_easy_strerror: {}", error);
		uint32 size = (uint32)strlen(error) + 1;
		MEMPTR<char> result{ coreinit_allocFromSysArea(size, 4) };
		memcpy(result.GetPtr(), error, size);
		return result;
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

	class : public COSModule
	{
		public:
		std::vector<std::string_view> GetDependencies() override
		{
			return {"nsysnet"};
		}

		std::string_view GetName() override
		{
			return "nlibcurl";
		}

		void RPLMapped() override
		{
			s_abortWorkers = false;

			cafeExportRegister("nlibcurl", curl_global_init_mem, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_global_init, LogType::nlibcurl);

			cafeExportRegister("nlibcurl", curl_slist_append, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_slist_free_all, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_easy_strerror, LogType::nlibcurl);

			cafeExportRegister("nlibcurl", curl_share_init, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_share_setopt, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_share_cleanup, LogType::nlibcurl);

			cafeExportRegister("nlibcurl", mw_curl_easy_init, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_multi_init, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_multi_add_handle, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_multi_perform, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_multi_info_read, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_multi_remove_handle, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_multi_setopt, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_multi_fdset, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_multi_cleanup, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_multi_timeout, LogType::nlibcurl);

			cafeExportRegister("nlibcurl", curl_easy_init, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_easy_reset, LogType::nlibcurl);
			cafeExportRegisterFunc(nlibcurl_easy_setopt, "nlibcurl", "curl_easy_setopt", LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_easy_getinfo, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_easy_perform, LogType::nlibcurl);

			cafeExportRegister("nlibcurl", curl_easy_cleanup, LogType::nlibcurl);
			cafeExportRegister("nlibcurl", curl_easy_pause, LogType::nlibcurl);
		};

		void RPLUnmapped() override
		{
			// abort transfers that are still active
			{
				std::unique_lock _l(s_curlMutex);
				s_abortWorkers = true;
			}
			s_workerCondVar.notify_all();
			while (s_activeWorkerCount != 0)
				std::this_thread::sleep_for(std::chrono::milliseconds(1));

			for (auto& it : s_multiHandles)
				delete it.second;
			s_multiHandles.clear();
			for (auto& it : s_curlHandles)
				delete it.second;
			s_curlHandles.clear();
			if (g_nlibcurl.initialized)
				::curl_global_cleanup();
			g_nlibcurl = {};
		}
	}s_COSnlibcurlModule;

	COSModule* GetModule()
	{
		return &s_COSnlibcurlModule;
	}

}
