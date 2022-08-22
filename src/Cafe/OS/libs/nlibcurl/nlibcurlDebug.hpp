
uint32 _curlDebugSessionId = 0;

// begin a new request if one is not already active
void curlDebug_markActiveRequest(CURL_t* curl)
{
	if (!ActiveSettings::DumpLibcurlRequestsEnabled())
		return;

	if (curl->debug.activeRequestIndex != 0)
		return; // already tracking request

	if (_curlDebugSessionId == 0)
	{
		_curlDebugSessionId = (uint32)std::chrono::seconds(std::time(NULL)).count();
		if (_curlDebugSessionId == 0)
			_curlDebugSessionId = 1;
		wchar_t filePath[256];
		swprintf(filePath, sizeof(filePath), L"dump//curl//session%u", _curlDebugSessionId);
		fs::create_directories(fs::path(filePath));
	}

	static uint32 _nextRequestIndex = 1;
	curl->debug.activeRequestIndex = _nextRequestIndex;
	_nextRequestIndex++;

	wchar_t filePath[256];
	swprintf(filePath, sizeof(filePath) / sizeof(wchar_t), L"dump//curl//session%u//request%04d_param.txt", _curlDebugSessionId, (sint32)curl->debug.activeRequestIndex);
	curl->debug.file_requestParam = FileStream::createFile(filePath);
	auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	if (curl->debug.file_requestParam)
	{
		auto t = std::localtime(&now);
		curl->debug.file_requestParam->writeStringFmt("Request %d %d-%d-%d %d:%02d:%02d\r\n", (sint32)curl->debug.activeRequestIndex, t->tm_year+1900, t->tm_mon, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	}
}

void curlDebug_notifySubmitRequest(CURL_t* curl)
{
	// start new response
	curl->debug.responseRequestIndex = curl->debug.activeRequestIndex;
	if (curl->debug.file_responseRaw)
	{
		delete curl->debug.file_responseRaw;
		curl->debug.file_responseRaw = nullptr;
	}
	if (curl->debug.file_responseHeaders)
	{
		delete curl->debug.file_responseHeaders;
		curl->debug.file_responseHeaders = nullptr;
	}
	curl->debug.hasDumpedResultInfo = false;
	// end current request
	curl->debug.activeRequestIndex = 0;
	if (curl->debug.file_requestParam)
	{
		delete curl->debug.file_requestParam;
		curl->debug.file_requestParam = nullptr;
	}
}

void curlDebug_logEasySetOptStr(CURL_t* curl, const char* optName, const char* optValue)
{
	curlDebug_markActiveRequest(curl);
	if (curl->debug.file_requestParam)
	{
		curl->debug.file_requestParam->writeStringFmt("SetOpt %s: ", optName, optValue ? optValue : "NULL");
		if(optValue)
			curl->debug.file_requestParam->writeString(optValue);
		else
			curl->debug.file_requestParam->writeString("NULL");
		curl->debug.file_requestParam->writeString("\r\n");
	}
}

void curlDebug_logEasySetOptPtr(CURL_t* curl, const char* optName, uint32 ppcPtr)
{
	curlDebug_markActiveRequest(curl);
	if (curl->debug.file_requestParam)
	{
		curl->debug.file_requestParam->writeStringFmt("SetOpt %s: 0x%08x\r\n", optName, ppcPtr);
	}
}

void curlDebug_resultWrite(CURL_t* curl, char* ptr, size_t size, size_t nmemb)
{
	if (!ActiveSettings::DumpLibcurlRequestsEnabled())
		return;
	if (curl->debug.responseRequestIndex == 0)
		return;
	if (curl->debug.file_responseRaw == nullptr)
	{
		wchar_t filePath[256];
		swprintf(filePath, sizeof(filePath) / sizeof(wchar_t), L"dump//curl//session%u//request%04d_responseRaw.bin", _curlDebugSessionId, (sint32)curl->debug.responseRequestIndex);
		curl->debug.file_responseRaw = FileStream::createFile(filePath);
	}
	if (curl->debug.file_responseRaw)
	{
		curl->debug.file_responseRaw->writeData(ptr, size*nmemb);
	}
}

void curlDebug_headerWrite(CURL_t* curl, char* ptr, size_t size, size_t nmemb)
{
	if (!ActiveSettings::DumpLibcurlRequestsEnabled())
		return;
	if (curl->debug.responseRequestIndex == 0)
		return;
	if (curl->debug.file_responseHeaders == nullptr)
	{
		wchar_t filePath[256];
		swprintf(filePath, sizeof(filePath) / sizeof(wchar_t), L"dump//curl//session%u//request%04d_responseHeaders.txt", _curlDebugSessionId, (sint32)curl->debug.responseRequestIndex);
		curl->debug.file_responseHeaders = FileStream::createFile(filePath);
	}
	if (curl->debug.file_responseHeaders)
	{
		curl->debug.file_responseHeaders->writeData(ptr, size*nmemb);
	}
}

void curlDebug_cleanup(CURL_t* curl)
{
	if (curl->debug.file_requestParam)
	{
		delete curl->debug.file_requestParam;
		curl->debug.file_requestParam = nullptr;
	}
	if (curl->debug.file_responseRaw)
	{
		delete curl->debug.file_responseRaw;
		curl->debug.file_responseRaw = nullptr;
	}
	if (curl->debug.file_responseHeaders)
	{
		delete curl->debug.file_responseHeaders;
		curl->debug.file_responseHeaders = nullptr;
	}
}