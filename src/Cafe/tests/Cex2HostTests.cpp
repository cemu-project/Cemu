#include "Cafe/HW/Espresso/ModExecutionContext.h"
#include "Cafe/OS/libs/cemuextend/Cex2Host.h"
#include "cemuextend/services.hpp"
#include "cemuextend/transport.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <iostream>
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

std::vector<std::byte> Request(std::uint32_t correlation, std::uint16_t operation,
	std::span<const std::byte> payload = {},
	cemuextend::wire::ServiceId service = cemuextend::wire::ServiceId::Core)
{
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
	for (int attempt = 0; attempt < 200; ++attempt)
	{
		const auto result = host.Poll(context, session, response, size);
		if (result == static_cast<std::int32_t>(Error::Ok)) { response.resize(size); return response; }
		CHECK(result == static_cast<std::int32_t>(Error::NotFound));
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
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

	cemuextend::wire::Encoder read; CHECK(read.String("folder/file.bin")); read.U64(0); read.U32(64 * 1024);
	request = Request(4, static_cast<std::uint16_t>(cemuextend::wire::FileOperation::Read),
		read.data(), cemuextend::wire::ServiceId::File);
	CHECK(host.Submit(context, session, request) == static_cast<std::int32_t>(Error::Ok));
	response = PollUntil(host, context, session);
	CHECK(response.size() == sizeof(ResponseHeader) + data.size());
	CHECK(std::memcmp(response.data() + sizeof(ResponseHeader), data.data(), data.size()) == 0);
	CHECK(host.Close(context, session) == static_cast<std::int32_t>(Error::Ok));
}

} // namespace

int main()
{
	TestOwnershipCopyAndCancel();
	TestBackpressureAndProtocolReap();
	TestPrincipalStorageAndPagination();
	return 0;
}
