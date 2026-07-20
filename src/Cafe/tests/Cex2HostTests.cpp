#include "Common/precompiled.h"

#include "Cafe/HW/Espresso/ModExecutionContext.h"
#include "Cafe/OS/libs/cemuextend/Cex2Host.h"
#include "Cafe/OS/libs/vpad/vpad.h"
#include "cemuextend/services.hpp"
#include "cemuextend/transport.hpp"

#include <openssl/crypto.h>

#include <array>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <thread>
#include <vector>

namespace {

[[noreturn]] void CheckFailed(const char* expression, int line)
{
	std::cerr << "CHECK failed at line " << line << ": " << expression << '\n';
	std::abort();
}

#define CHECK(condition) do { if (!(condition)) CheckFailed(#condition, __LINE__); } while (false)

using cemuextend::transport::OpenOptions;
using cemuextend::transport::RequestHeader;
using cemuextend::transport::ResponseHeader;
using cemuextend::wire::Error;
using cemuextend::wire::Status;

std::uint32_t gLastCorrelation{};

std::vector<std::byte> Request(std::uint32_t correlation, std::uint16_t operation,
	std::span<const std::byte> payload = {},
	cemuextend::wire::ServiceId service = cemuextend::wire::ServiceId::Core)
{
	gLastCorrelation = correlation;
	std::vector<std::byte> result(sizeof(RequestHeader) + payload.size());
	auto& header = *reinterpret_cast<RequestHeader*>(result.data());
	header.totalSize = static_cast<std::uint32_t>(result.size());
	header.correlationId = correlation;
	header.serviceId = static_cast<std::uint16_t>(service);
	header.operation = operation;
	header.operationVersion = cemuextend::transport::kOperationVersion;
	header.flags = 0;
	if (!payload.empty())
		std::memcpy(result.data() + sizeof(header), payload.data(), payload.size());
	return result;
}

std::vector<std::byte> PollUntil(cemuextend_hle::Cex2Host& host,
	ModExecutionContext& context, std::uint32_t session)
{
	std::vector<std::byte> response(cemuextend::transport::kMaximumMessageSize);
	std::uint32_t size{};
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (std::chrono::steady_clock::now() < deadline)
	{
		const auto result = host.Poll(context, session, response, size);
		if (result == static_cast<std::int32_t>(Error::Ok)) { response.resize(size); return response; }
		CHECK(result == static_cast<std::int32_t>(Error::NotFound));
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	std::cerr << "Timed out waiting for correlation " << gLastCorrelation << '\n';
	CHECK(false);
	return {};
}

std::uint32_t Open(cemuextend_hle::Cex2Host& host, ModExecutionContext& context)
{
	OpenOptions options{};
	options.abiMajor = cemuextend::transport::kAbiMajor;
	options.abiMinor = cemuextend::transport::kAbiMinor;
	options.maximumPendingRequests = 128;
	std::uint32_t session{};
	const auto bytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(&options), sizeof(options));
	CHECK(host.Open(context, bytes, session) == static_cast<std::int32_t>(Error::Ok));
	CHECK(session != 0);
	return session;
}

void TestOwnershipCopyAndCancel()
{
	auto& host = cemuextend_hle::Cex2Host::Instance();
	host.CloseAll();
	ModExecutionContext first(1, 7, "publisher:first");
	ModExecutionContext second(2, 3, "publisher:second");

	std::array<std::byte, sizeof(cemuextend::transport::Info)> infoBytes{};
	CHECK(host.Query(first, static_cast<std::uint32_t>(cemuextend::transport::Query::Info), infoBytes) ==
		static_cast<std::int32_t>(Error::Ok));
	const auto& info = *reinterpret_cast<const cemuextend::transport::Info*>(infoBytes.data());
	CHECK(info.abiMajor.get() == 2);
	CHECK(info.maximumMessageSize.get() == 64U * 1024U);

	const auto session = Open(host, first);
	const cemuextend::wire::Be64 cookie{0x1122334455667788ULL};
	const auto cookieBytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(&cookie), sizeof(cookie));
	auto request = Request(9, static_cast<std::uint16_t>(cemuextend::wire::CoreOperation::Ping), cookieBytes);
	CHECK(host.Submit(second, session, request) == static_cast<std::int32_t>(Error::PermissionDenied));
	CHECK(host.Submit(first, session, request) == static_cast<std::int32_t>(Error::Ok));
	CHECK(host.Cancel(first, session, 9) == static_cast<std::int32_t>(Error::Ok));

	std::array<std::byte, cemuextend::transport::kMaximumMessageSize> response{};
	std::uint32_t responseSize{};
	CHECK(host.Poll(second, session, response, responseSize) ==
		static_cast<std::int32_t>(Error::PermissionDenied));
	CHECK(host.Poll(first, session, response, responseSize) == static_cast<std::int32_t>(Error::Ok));
	const auto& header = *reinterpret_cast<const ResponseHeader*>(response.data());
	CHECK(header.correlationId.get() == 9);
	CHECK(header.status.get() == static_cast<std::uint16_t>(Status::Cancelled));
	CHECK(responseSize == sizeof(ResponseHeader));
	CHECK(host.Close(first, session) == static_cast<std::int32_t>(Error::Ok));
}

void TestBackpressureAndProtocolReap()
{
	auto& host = cemuextend_hle::Cex2Host::Instance();
	host.CloseAll();
	ModExecutionContext context(10, 1, "unsigned:package");
	auto session = Open(host, context);

	for (std::uint32_t index = 1; index <= cemuextend::transport::kMaximumResponseQueue; ++index)
	{
		auto request = Request(index, static_cast<std::uint16_t>(cemuextend::wire::CoreOperation::GetVersion));
		CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	}
	auto blocked = Request(1000, static_cast<std::uint16_t>(cemuextend::wire::CoreOperation::GetVersion));
	CHECK(host.Submit(context, session, blocked) == static_cast<std::int32_t>(Error::Busy));

	std::array<std::byte, cemuextend::transport::kMaximumMessageSize> response{};
	std::uint32_t responseSize{};
	CHECK(host.Poll(context, session, response, responseSize) == static_cast<std::int32_t>(Error::Ok));
	CHECK(host.Submit(context, session, blocked) == static_cast<std::int32_t>(Error::Ok));
	CHECK(host.Close(context, session) == static_cast<std::int32_t>(Error::Ok));

	session = Open(host, context);
	auto malformed = Request(1, static_cast<std::uint16_t>(cemuextend::wire::CoreOperation::Ping));
	reinterpret_cast<RequestHeader*>(malformed.data())->flags = 0x8000;
	CHECK(host.Submit(context, session, malformed) == static_cast<std::int32_t>(Error::ProtocolError));
	CHECK(host.Poll(context, session, response, responseSize) ==
		static_cast<std::int32_t>(Error::PermissionDenied));
	const auto replacement = Open(host, context);
	CHECK(replacement != session);
	CHECK(host.Close(context, replacement) == static_cast<std::int32_t>(Error::Ok));
}

void TestExactOnceAdmission()
{
	auto& host = cemuextend_hle::Cex2Host::Instance();
	host.CloseAll();
	ModExecutionContext context(11, 1, "exactly-once-principal");
	const auto session = Open(host, context);
	auto request = Request(77,
		static_cast<std::uint16_t>(cemuextend::wire::CoreOperation::GetVersion));
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	(void)PollUntil(host, context, session);
	CHECK(host.Submit(context, session, request) ==
		static_cast<std::int32_t>(Error::ProtocolError));
	std::array<std::byte, cemuextend::transport::kMaximumMessageSize> response{};
	std::uint32_t responseSize{};
	CHECK(host.Poll(context, session, response, responseSize) ==
		static_cast<std::int32_t>(Error::PermissionDenied));
	const auto replacement = Open(host, context);
	CHECK(replacement != session);
	CHECK(host.Close(context, replacement) == static_cast<std::int32_t>(Error::Ok));
}

void TestPrincipalStorageAndPagination()
{
	auto& host = cemuextend_hle::Cex2Host::Instance(); host.CloseAll();
	ModExecutionContext context(20, 1, "storage-test-principal");
	context.SetTitleId(0xce02000000000001ULL); context.SetGrantedPermissions(3);
	const auto session = Open(host, context);

	cemuextend::wire::Encoder set;
	CHECK(set.String("answer")); set.U8(static_cast<std::uint8_t>(cemuextend::wire::ValueType::UnsignedInteger));
	set.U32(8); set.U64(42);
	auto request = Request(1, static_cast<std::uint16_t>(cemuextend::wire::ConfigurationOperation::Set),
		set.data(), cemuextend::wire::ServiceId::Configuration);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	auto response = PollUntil(host, context, session);
	CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() == static_cast<std::uint16_t>(Status::Ok));

	cemuextend::wire::Encoder get; CHECK(get.String("answer"));
	request = Request(2, static_cast<std::uint16_t>(cemuextend::wire::ConfigurationOperation::Get),
		get.data(), cemuextend::wire::ServiceId::Configuration);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	response = PollUntil(host, context, session);
	CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() == static_cast<std::uint16_t>(Status::Ok));

	cemuextend::wire::Encoder write; CHECK(write.String("folder/file.bin")); write.U64(0);
	const std::array data{std::byte{1}, std::byte{2}, std::byte{3}}; write.Bytes(data);
	request = Request(3, static_cast<std::uint16_t>(cemuextend::wire::FileOperation::Write),
		write.data(), cemuextend::wire::ServiceId::File);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	response = PollUntil(host, context, session);
	CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() == static_cast<std::uint16_t>(Status::Ok));

	cemuextend::wire::Encoder stat;
	CHECK(stat.String("folder/file.bin"));
	request = Request(4, static_cast<std::uint16_t>(cemuextend::wire::FileOperation::Stat),
		stat.data(), cemuextend::wire::ServiceId::File);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	response = PollUntil(host, context, session);
	CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() == static_cast<std::uint16_t>(Status::Ok));
	CHECK(response.size() == sizeof(ResponseHeader) + sizeof(cemuextend::wire::FileStatPayload));
	const auto& fileStat = *reinterpret_cast<const cemuextend::wire::FileStatPayload*>(
		response.data() + sizeof(ResponseHeader));
	CHECK(fileStat.size.get() == data.size());
	CHECK(fileStat.modifiedTimeNs.get() != 0);
	CHECK(fileStat.mode.get() == 0600 && fileStat.type == 1);

	cemuextend::wire::Encoder sparse;
	CHECK(sparse.String("folder/file.bin")); sparse.U64(data.size() + 1); sparse.U8(0xff);
	request = Request(5, static_cast<std::uint16_t>(cemuextend::wire::FileOperation::Write),
		sparse.data(), cemuextend::wire::ServiceId::File);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	response = PollUntil(host, context, session);
	CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() ==
		static_cast<std::uint16_t>(Status::InvalidArgument));

	cemuextend::wire::Encoder removeRoot;
	CHECK(removeRoot.String("."));
	request = Request(6, static_cast<std::uint16_t>(cemuextend::wire::FileOperation::Remove),
		removeRoot.data(), cemuextend::wire::ServiceId::File);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	response = PollUntil(host, context, session);
	CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() ==
		static_cast<std::uint16_t>(Status::PermissionDenied));

	cemuextend::wire::Encoder read; CHECK(read.String("folder/file.bin")); read.U64(0); read.U32(64 * 1024);
	request = Request(7, static_cast<std::uint16_t>(cemuextend::wire::FileOperation::Read),
		read.data(), cemuextend::wire::ServiceId::File);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	response = PollUntil(host, context, session);
	CHECK(response.size() == sizeof(ResponseHeader) + data.size());
	CHECK(std::memcmp(response.data() + sizeof(ResponseHeader), data.data(), data.size()) == 0);

	cemuextend::wire::Encoder invalidType;
	CHECK(invalidType.String("invalid-bool"));
	invalidType.U8(static_cast<std::uint8_t>(cemuextend::wire::ValueType::Boolean));
	invalidType.U32(2); invalidType.U8(0); invalidType.U8(1);
	request = Request(8, static_cast<std::uint16_t>(cemuextend::wire::ConfigurationOperation::Set),
		invalidType.data(), cemuextend::wire::ServiceId::Configuration);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	response = PollUntil(host, context, session);
	CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() ==
		static_cast<std::uint16_t>(Status::InvalidArgument));

	for (std::uint32_t index = 0; index < 3; ++index)
	{
		cemuextend::wire::Encoder pageValue;
		CHECK(pageValue.String("page-" + std::to_string(index)));
		pageValue.U8(static_cast<std::uint8_t>(cemuextend::wire::ValueType::Boolean));
		pageValue.U32(1); pageValue.U8(1);
		request = Request(9 + index,
			static_cast<std::uint16_t>(cemuextend::wire::ConfigurationOperation::Set),
			pageValue.data(), cemuextend::wire::ServiceId::Configuration);
		CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
		response = PollUntil(host, context, session);
		CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() ==
			static_cast<std::uint16_t>(Status::Ok));
	}
	cemuextend::wire::Encoder page;
	CHECK(page.String("page-")); page.U16(2); page.U32(0);
	request = Request(12,
		static_cast<std::uint16_t>(cemuextend::wire::ConfigurationOperation::List),
		page.data(), cemuextend::wire::ServiceId::Configuration);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	response = PollUntil(host, context, session);
	CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() ==
		static_cast<std::uint16_t>(Status::Ok));
	cemuextend::wire::Decoder pageResponse(std::span<const std::byte>(response).subspan(
		sizeof(ResponseHeader)));
	std::uint32_t pageCount{}, tokenSize{}; std::uint8_t type{}, truncated{};
	std::string pageKey; std::span<const std::byte> token;
	CHECK(pageResponse.U32(pageCount) && pageCount == 2);
	for (std::uint32_t index = 0; index < pageCount; ++index)
		CHECK(pageResponse.String(pageKey) && pageKey.starts_with("page-") && pageResponse.U8(type));
	CHECK(pageResponse.U8(truncated) && truncated == 1);
	CHECK(pageResponse.U32(tokenSize) && tokenSize != 0 && pageResponse.Bytes(tokenSize, token));
	CHECK(pageResponse.remaining() == 0);
	cemuextend::wire::Encoder hugeOffset;
	CHECK(hugeOffset.String("folder/file.bin"));
	hugeOffset.U64(std::numeric_limits<std::uint64_t>::max()); hugeOffset.U32(1);
	request = Request(13, static_cast<std::uint16_t>(cemuextend::wire::FileOperation::Read),
		hugeOffset.data(), cemuextend::wire::ServiceId::File);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	response = PollUntil(host, context, session);
	CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() ==
		static_cast<std::uint16_t>(Status::InvalidArgument));

#ifndef _WIN32
	const auto titleRoot = std::filesystem::temp_directory_path() / "cemuextend-cex2-tests" /
		"files" / "ce02000000000001";
	std::error_code filesystemError;
	std::filesystem::path namespaceRoot;
	for (std::filesystem::directory_iterator iterator(titleRoot, filesystemError), end;
		!filesystemError && iterator != end; ++iterator)
	{
		if (iterator->is_directory()) { namespaceRoot = iterator->path(); break; }
	}
	CHECK(!namespaceRoot.empty());
	const auto outside = std::filesystem::temp_directory_path() / "cemuextend-cex2-outside";
	std::filesystem::create_directories(outside, filesystemError);
	CHECK(!filesystemError);
	{ std::ofstream secret(outside / "secret.bin", std::ios::binary); secret.put('x'); CHECK(secret.good()); }
	std::filesystem::remove(namespaceRoot / "escape", filesystemError);
	filesystemError.clear();
	std::filesystem::create_directory_symlink(outside, namespaceRoot / "escape", filesystemError);
	CHECK(!filesystemError);
	cemuextend::wire::Encoder escapedStat;
	CHECK(escapedStat.String("escape/secret.bin"));
	request = Request(14, static_cast<std::uint16_t>(cemuextend::wire::FileOperation::Stat),
		escapedStat.data(), cemuextend::wire::ServiceId::File);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	response = PollUntil(host, context, session);
	CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() ==
		static_cast<std::uint16_t>(Status::PermissionDenied));
	std::filesystem::remove(namespaceRoot / "escape", filesystemError);
	std::filesystem::remove_all(outside, filesystemError);
#endif
	CHECK(host.Close(context, session) == static_cast<std::int32_t>(Error::Ok));
}

void TestTitleServicePermissionsAndRevocation()
{
	auto& host = cemuextend_hle::Cex2Host::Instance(); host.CloseAll();
	ModExecutionContext context(30, 1, "permission-test-principal");
	context.SetGrantedPermissions(2);
	constexpr auto loggingBit = 1U <<
		(static_cast<std::uint16_t>(cemuextend::wire::ServiceId::Logging) - 1U);
	context.SetServicePermissions({0, loggingBit, 0});
	const auto session = Open(host, context);

	cemuextend::wire::Encoder payload;
	payload.U8(static_cast<std::uint8_t>(cemuextend::wire::LogLevel::Info));
	CHECK(payload.String("permission test"));
	auto request = Request(1,
		static_cast<std::uint16_t>(cemuextend::wire::LoggingOperation::Write),
		payload.data(), cemuextend::wire::ServiceId::Logging);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));

	// A response already queued before revocation must not leak its result.
	context.SetServicePermissions({0, 0, 0});
	host.PermissionsChanged(context, 2);
	auto response = PollUntil(host, context, session);
	CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() ==
		static_cast<std::uint16_t>(Status::PermissionDenied));

	request = Request(2,
		static_cast<std::uint16_t>(cemuextend::wire::LoggingOperation::Write),
		payload.data(), cemuextend::wire::ServiceId::Logging);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	response = PollUntil(host, context, session);
	CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() ==
		static_cast<std::uint16_t>(Status::PermissionDenied));

	context.SetServicePermissions({0, loggingBit, 0});
	host.PermissionsChanged(context, 2);
	request = Request(3,
		static_cast<std::uint16_t>(cemuextend::wire::LoggingOperation::Write),
		payload.data(), cemuextend::wire::ServiceId::Logging);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	host.PermissionsChanged(context, 0);
	response = PollUntil(host, context, session);
	CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() ==
		static_cast<std::uint16_t>(Status::PermissionDenied));
	host.PermissionsChanged(context, 2);
	request = Request(4,
		static_cast<std::uint16_t>(cemuextend::wire::LoggingOperation::Write),
		payload.data(), cemuextend::wire::ServiceId::Logging);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	response = PollUntil(host, context, session);
	CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() ==
		static_cast<std::uint16_t>(Status::Ok));
	CHECK(host.Close(context, session) == static_cast<std::int32_t>(Error::Ok));
}

void TestObservedInputSnapshot()
{
	auto& host = cemuextend_hle::Cex2Host::Instance(); host.CloseAll();
	ModExecutionContext context(31, 1, "input-snapshot-principal");
	context.SetGrantedPermissions(1);
	const auto session = Open(host, context);
	VPADStatus status{};
	status.hold = 0x1234;
	status.leftStick.x = 2.0f;
	status.leftStick.y = -2.0f;
	status.rightStick.x = std::numeric_limits<float>::quiet_NaN();
	status.rightStick.y = 0.5f;
	host.ObserveVpad(0, status, 0, 1);
	const std::array payload{std::byte{0}};
	auto request = Request(1,
		static_cast<std::uint16_t>(cemuextend::wire::InputOperation::GetObserved),
		payload, cemuextend::wire::ServiceId::Input);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	auto response = PollUntil(host, context, session);
	CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() ==
		static_cast<std::uint16_t>(Status::Ok));
	CHECK(response.size() == sizeof(ResponseHeader) + sizeof(cemuextend::wire::ObservedVpadState));
	const auto& observed = *reinterpret_cast<const cemuextend::wire::ObservedVpadState*>(
		response.data() + sizeof(ResponseHeader));
	CHECK(observed.hold.get() == 0x1234);
	CHECK(observed.leftX.get() == 1.0f && observed.leftY.get() == -1.0f);
	CHECK(observed.rightX.get() == 0.0f && observed.rightY.get() == 0.5f);

	context.SetServicePermissions({0, 0x1ffU, 0x1ffU});
	host.PermissionsChanged(context, 1);
	request = Request(2,
		static_cast<std::uint16_t>(cemuextend::wire::InputOperation::GetObserved),
		payload, cemuextend::wire::ServiceId::Input);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	response = PollUntil(host, context, session);
	CHECK(reinterpret_cast<ResponseHeader*>(response.data())->status.get() ==
		static_cast<std::uint16_t>(Status::PermissionDenied));
	CHECK(host.Close(context, session) == static_cast<std::int32_t>(Error::Ok));
}

} // namespace

int main()
{
	TestOwnershipCopyAndCancel();
	TestBackpressureAndProtocolReap();
	TestExactOnceAdmission();
	TestPrincipalStorageAndPagination();
	TestTitleServicePermissionsAndRevocation();
	TestObservedInputSnapshot();
	cemuextend_hle::Cex2Host::Instance().ShutdownForTesting();
	// OpenSSL keeps provider/configuration state alive until process shutdown.
	// Release it explicitly so LeakSanitizer can distinguish product leaks from
	// the library's process-lifetime caches in this short-lived test binary.
	OPENSSL_cleanup();
	return 0;
}
