#include "Common/precompiled.h"

#include "Cafe/OS/libs/cemuextend/Cex2Host.h"
#include "Cafe/OS/libs/cemuextend/Cex2Storage.h"

#include "Cafe/HW/Espresso/ModExecutionContext.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteOverlay.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/OS/libs/cemuextend/BuildId.h"
#include "Cafe/OS/libs/vpad/vpad.h"
#include "Cemu/Logging/CemuLogging.h"
#include "gui/interface/WindowSystem.h"
#include "cemuextend/services.hpp"
#include "cemuextend/transport.hpp"

#include <deque>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <thread>

namespace cemuextend_hle {
namespace {

std::uint64_t CurrentFrameNumber()
{
#ifdef CEMU_CEX2_TESTING
	return 0;
#else
	return LatteGPUState.frameCounter;
#endif
}

void LogGuestRecord(std::string_view principal, std::uint8_t level, std::string_view message)
{
#ifndef CEMU_CEX2_TESTING
	cemuLog_log(LogType::Force, "[CemuExtend Mod/{}/{}] {}", principal, level, message);
#endif
}

void AuditSensitiveUse(std::string_view principal, std::string_view action)
{
#ifndef CEMU_CEX2_TESTING
	const auto message = fmt::format("CemuExtend Mod [{}]: {}", principal, action);
	cemuLog_log(LogType::Force, "AUDIT {}", message);
	LatteOverlay_pushNotification(message, 3000);
#endif
}

using cemuextend::transport::RequestHeader;
using cemuextend::transport::ResponseHeader;
using cemuextend::wire::Error;
using cemuextend::wire::ServiceId;
using cemuextend::wire::Status;

struct ServiceDefinition
{
	std::uint16_t id;
	std::uint16_t version;
	std::uint32_t requiredPermission;
	std::uint32_t maximumRequest;
	std::uint32_t maximumResponse;
};

enum class Handler : std::uint8_t
{
	Core,
	Input,
	Logging,
	Configuration,
	File,
	Clipboard,
	Window,
	Capture,
	Diagnostics,
};

struct OperationDefinition
{
	std::uint16_t service;
	std::uint16_t operation;
	std::uint16_t version;
	std::uint32_t permission;
	std::uint32_t maximumRequest;
	std::uint32_t maximumResponse;
	std::uint16_t ratePerSecond;
	std::uint16_t burst;
	Handler handler;
};

constexpr std::array kOperations{
	OperationDefinition{1,1,1,0,0,4096,0,0,Handler::Core},
	OperationDefinition{1,2,1,0,8,8,0,0,Handler::Core},
	OperationDefinition{1,3,1,0,0,32,0,0,Handler::Core},
	OperationDefinition{1,4,1,0,2,0,0,0,Handler::Core},
	OperationDefinition{1,5,1,0,2,0,0,0,Handler::Core},
	OperationDefinition{1,6,1,0,0,128,0,0,Handler::Core},
	OperationDefinition{2,1,1,4,sizeof(cemuextend::wire::ControllerEventPayload),0,0,0,Handler::Input},
	OperationDefinition{2,2,1,4,1+sizeof(cemuextend::wire::ObservedVpadState),0,0,0,Handler::Input},
	OperationDefinition{3,1,1,2,4096+5,0,20,50,Handler::Logging},
	OperationDefinition{4,1,1,1,260,65520,0,0,Handler::Configuration},
	OperationDefinition{4,2,1,2,65520,0,0,0,Handler::Configuration},
	OperationDefinition{4,3,1,2,260,0,0,0,Handler::Configuration},
	OperationDefinition{4,4,1,1,65520,65520,0,0,Handler::Configuration},
	OperationDefinition{5,1,1,1,4096,64,0,0,Handler::File},
	OperationDefinition{5,2,1,1,4096,65520,0,0,Handler::File},
	OperationDefinition{5,3,1,1,4096,65520,0,0,Handler::File},
	OperationDefinition{5,4,1,2,65520,0,0,0,Handler::File},
	OperationDefinition{5,5,1,2,4096,0,0,0,Handler::File},
	OperationDefinition{5,6,1,2,4096,0,0,0,Handler::File},
	OperationDefinition{5,7,1,2,8192,0,0,0,Handler::File},
	OperationDefinition{6,1,1,8,0,65520,0,0,Handler::Clipboard},
	OperationDefinition{6,2,1,8,65520,0,0,0,Handler::Clipboard},
	OperationDefinition{7,1,1,1,0,sizeof(cemuextend::wire::WindowStatePayload),0,0,Handler::Window},
	OperationDefinition{8,1,1,16,1,64,0,0,Handler::Capture},
	OperationDefinition{8,2,1,16,8,65520,0,0,Handler::Capture},
	OperationDefinition{8,3,1,16,4,0,0,0,Handler::Capture},
	OperationDefinition{9,1,1,1,0,sizeof(cemuextend::wire::DiagnosticsPayload),0,0,Handler::Diagnostics},
};

const OperationDefinition* FindOperation(std::uint16_t service, std::uint16_t operation)
{
	const auto found = std::ranges::find_if(kOperations, [=](const OperationDefinition& definition) {
		return definition.service == service && definition.operation == operation;
	});
	return found == kOperations.end() ? nullptr : &*found;
}

constexpr std::array kServices{
	ServiceDefinition{1, 1, 0, 64U * 1024U, 64U * 1024U},
	ServiceDefinition{2, 1, 4, 64U * 1024U, 64U * 1024U},
	ServiceDefinition{3, 1, 2, 4U * 1024U, 64},
	ServiceDefinition{4, 1, 3, 64U * 1024U, 64U * 1024U},
	ServiceDefinition{5, 1, 3, 64U * 1024U, 64U * 1024U},
	ServiceDefinition{6, 1, 3, 64U * 1024U, 64U * 1024U},
	ServiceDefinition{7, 1, 1, 64, 256},
	ServiceDefinition{8, 1, 1, 64, 64U * 1024U},
	ServiceDefinition{9, 1, 1, 64, 4U * 1024U},
};

struct WireServiceDefinition
{
	cemuextend::wire::Be16 id;
	cemuextend::wire::Be16 version;
	cemuextend::wire::Be32 requiredPermission;
	cemuextend::wire::Be32 maximumRequest;
	cemuextend::wire::Be32 maximumResponse;
};
static_assert(sizeof(WireServiceDefinition) == 16);

std::vector<std::byte> MakeResponse(const RequestHeader& request, Status status,
	std::span<const std::byte> payload = {})
{
	std::vector<std::byte> result(sizeof(ResponseHeader) + payload.size());
	auto& header = *reinterpret_cast<ResponseHeader*>(result.data());
	header.totalSize = static_cast<std::uint32_t>(result.size());
	header.correlationId = request.correlationId.get();
	header.serviceId = request.serviceId.get();
	header.operation = request.operation.get();
	header.status = static_cast<std::uint16_t>(status);
	header.flags = 0;
	if (!payload.empty())
		std::memcpy(result.data() + sizeof(header), payload.data(), payload.size());
	return result;
}

} // namespace

struct Cex2Host::Impl
{
	struct Session
	{
		struct Pending
		{
			std::uint32_t permission{};
			RequestHeader header{};
			std::chrono::steady_clock::time_point deadline{};
		};
		ModExecutionContext* owner{};
		std::uint64_t addressSpaceId{};
		std::uint32_t generation{};
		std::deque<std::vector<std::byte>> responses;
		std::unordered_set<std::uint16_t> subscriptions;
		std::uint64_t acceptedRequests{};
		std::uint64_t completedResponses{};
		std::uint64_t protocolErrors{};
		std::uint64_t droppedEvents{};
		std::uint64_t bytesCopied{};
		std::uint64_t nextInputEventId{1};
		std::size_t reservedResponses{};
		std::unordered_map<std::uint32_t, Pending> pending;
		double loggingTokens{50.0};
		std::chrono::steady_clock::time_point loggingLastRefill{std::chrono::steady_clock::now()};
		std::set<std::uint16_t> pressedKeyboardUsages;
		std::array<cemuextend::wire::ObservedVpadState, 2> observedVpad{};
		std::array<bool, 2> hasObservedVpad{};
		std::array<cemuextend::wire::ObservedVpadState, 2> mappedInjection{};
		std::array<bool, 2> hasMappedInjection{};
		std::array<std::chrono::steady_clock::time_point, 2> mappedInjectionTime{};
		bool clipboardPending{};
		struct Capture
		{
			std::uint32_t handle{};
			std::uint32_t width{};
			std::uint32_t height{};
			std::vector<std::byte> rgb;
			std::chrono::steady_clock::time_point expires{};
			bool pending{};
			bool mainWindow{};
		} capture;
	};

	std::mutex mutex;
	std::unordered_map<std::uint32_t, Session> sessions;
	std::uint32_t nextSession{1};
	std::mutex workMutex;
	std::condition_variable workReady;
	std::deque<std::function<void()>> work;
	bool stopping{};
	std::array<std::thread, 2> workers;

	Impl()
	{
		for (auto& worker : workers)
			worker = std::thread([this] {
				for (;;)
				{
					std::function<void()> task;
					{
						std::unique_lock lock(workMutex);
						workReady.wait(lock, [this] { return stopping || !work.empty(); });
						if (stopping && work.empty()) return;
						task = std::move(work.front()); work.pop_front();
					}
					task();
				}
			});
	}

	~Impl()
	{
		{ std::lock_guard lock(workMutex); stopping = true; }
		workReady.notify_all();
		for (auto& worker : workers) if (worker.joinable()) worker.join();
	}

	void Enqueue(std::function<void()> task)
	{
		{ std::lock_guard lock(workMutex); work.push_back(std::move(task)); }
		workReady.notify_one();
	}

	void Complete(std::uint32_t sessionId, std::uint64_t addressSpaceId, std::uint32_t generation,
		std::uint32_t correlationId, Status status, std::span<const std::byte> payload = {})
	{
		std::lock_guard lock(mutex);
		const auto found = sessions.find(sessionId);
		if (found == sessions.end() || found->second.addressSpaceId != addressSpaceId ||
			found->second.generation != generation) return;
		auto& session = found->second;
		const auto pending = session.pending.find(correlationId);
		if (pending == session.pending.end()) return;
		if (!HasPermission(session, pending->second.permission)) status = Status::PermissionDenied;
		const auto service = pending->second.header.serviceId.get();
		if (service == static_cast<std::uint16_t>(ServiceId::Clipboard)) session.clipboardPending = false;
		if (service == static_cast<std::uint16_t>(ServiceId::Capture) && status != Status::Ok)
			session.capture = {};
		auto response = MakeResponse(pending->second.header, status, payload);
		session.bytesCopied += response.size();
		session.responses.push_back(std::move(response));
		session.pending.erase(pending);
		--session.reservedResponses;
	}

	static bool Owns(const Session& session, ModExecutionContext& owner)
	{
		return session.owner == &owner && session.addressSpaceId == owner.AddressSpaceId() &&
			session.generation == owner.Generation() && !owner.IsStopped();
	}

	static bool HasPermission(const Session& session, std::uint32_t permission)
	{
		return permission == 0 || (session.owner->GrantedPermissions() & permission) == permission;
	}

	static bool IsValidUtf8(std::string_view text)
	{
		for (std::size_t index = 0; index < text.size();)
		{
			const auto lead = static_cast<std::uint8_t>(text[index]);
			if (lead < 0x80) { ++index; continue; }
			std::size_t count = (lead & 0xe0) == 0xc0 ? 1 : (lead & 0xf0) == 0xe0 ? 2 :
				(lead & 0xf8) == 0xf0 ? 3 : 0;
			if (!count || index + count >= text.size()) return false;
			std::uint32_t codepoint = lead & (0x7fU >> count);
			for (std::size_t offset = 1; offset <= count; ++offset)
			{
				const auto next = static_cast<std::uint8_t>(text[index + offset]);
				if ((next & 0xc0) != 0x80) return false;
				codepoint = (codepoint << 6) | (next & 0x3f);
			}
			if ((count == 1 && codepoint < 0x80) || (count == 2 && codepoint < 0x800) ||
				(count == 3 && codepoint < 0x10000) || codepoint > 0x10ffff ||
				(codepoint >= 0xd800 && codepoint <= 0xdfff)) return false;
			index += count + 1;
		}
		return true;
	}

	void EmitEvent(Session& session, ServiceId service, std::uint16_t operation,
		std::span<const std::byte> payload)
	{
		if (!session.subscriptions.contains(static_cast<std::uint16_t>(service)) ||
			session.responses.size() + session.reservedResponses >=
				cemuextend::transport::kMaximumResponseQueue)
		{
			if (session.subscriptions.contains(static_cast<std::uint16_t>(service))) ++session.droppedEvents;
			return;
		}
		std::vector<std::byte> result(sizeof(ResponseHeader) + payload.size());
		auto& header = *reinterpret_cast<ResponseHeader*>(result.data());
		header.totalSize = static_cast<std::uint32_t>(result.size());
		header.correlationId = 0;
		header.serviceId = static_cast<std::uint16_t>(service);
		header.operation = operation;
		header.status = static_cast<std::uint16_t>(Status::Ok);
		header.flags = static_cast<std::uint16_t>(cemuextend::transport::ResponseFlag::Event);
		if (!payload.empty()) std::memcpy(result.data() + sizeof(header), payload.data(), payload.size());
		session.responses.push_back(std::move(result));
	}

	std::vector<std::byte> Dispatch(Session& session, const RequestHeader& request,
		std::span<const std::byte> payload)
	{
		using namespace cemuextend::wire;
		const auto* definition = FindOperation(request.serviceId.get(), request.operation.get());
		if (!definition)
			return MakeResponse(request, Status::NotSupported);
		if (request.operationVersion.get() != definition->version)
			return MakeResponse(request, Status::NotSupported);
		if (!HasPermission(session, definition->permission))
			return MakeResponse(request, Status::PermissionDenied);
		if (payload.size() > definition->maximumRequest)
			return MakeResponse(request, Status::TooLarge);

		if (definition->handler == Handler::Input)
		{
			if (request.operation.get() == static_cast<std::uint16_t>(InputOperation::InjectGuest))
			{
				if (payload.size() != sizeof(ControllerEventPayload))
					return MakeResponse(request, Status::InvalidArgument);
				ControllerEventPayload event{};
				std::memcpy(&event, payload.data(), sizeof(event));
				const std::array values{event.leftX.get(), event.leftY.get(), event.rightX.get(),
					event.rightY.get(), event.leftTrigger.get(), event.rightTrigger.get()};
				if (!std::ranges::all_of(values, [](float value) { return std::isfinite(value); }))
					return MakeResponse(request, Status::InvalidArgument);
				event.identity.eventId = session.nextInputEventId++;
				event.identity.parentEventId = 0;
				event.identity.origin = static_cast<std::uint8_t>(InputOrigin::ClientInjected);
				event.identity.channel = static_cast<std::uint8_t>(InputChannel::Controller);
				event.identity.frameNumber = CurrentFrameNumber();
				event.leftX = std::clamp(event.leftX.get(), -1.0f, 1.0f);
				event.leftY = std::clamp(event.leftY.get(), -1.0f, 1.0f);
				event.rightX = std::clamp(event.rightX.get(), -1.0f, 1.0f);
				event.rightY = std::clamp(event.rightY.get(), -1.0f, 1.0f);
				event.leftTrigger = std::clamp(event.leftTrigger.get(), 0.0f, 1.0f);
				event.rightTrigger = std::clamp(event.rightTrigger.get(), 0.0f, 1.0f);
				AuditSensitiveUse(session.owner->Principal(), "Input Inject");
				EmitEvent(session, ServiceId::Input, static_cast<std::uint16_t>(InputEvent::Controller),
					{reinterpret_cast<const std::byte*>(&event), sizeof(event)});
				return MakeResponse(request, Status::Ok);
			}
			if (payload.size() != 1 + sizeof(ObservedVpadState))
				return MakeResponse(request, Status::InvalidArgument);
			const auto channel = std::to_integer<std::uint8_t>(payload[0]);
			if (channel >= 2) return MakeResponse(request, Status::InvalidArgument);
			ObservedVpadState injected{};
			std::memcpy(&injected, payload.data() + 1, sizeof(injected));
			const std::array sticks{injected.leftX.get(), injected.leftY.get(),
				injected.rightX.get(), injected.rightY.get()};
			if (!std::ranges::all_of(sticks, [](float value) { return std::isfinite(value); }))
				return MakeResponse(request, Status::InvalidArgument);
			injected.leftX = std::clamp(injected.leftX.get(), -1.0f, 1.0f);
			injected.leftY = std::clamp(injected.leftY.get(), -1.0f, 1.0f);
			injected.rightX = std::clamp(injected.rightX.get(), -1.0f, 1.0f);
			injected.rightY = std::clamp(injected.rightY.get(), -1.0f, 1.0f);
			session.mappedInjection[channel] = injected;
			session.hasMappedInjection[channel] = true;
			session.mappedInjectionTime[channel] = std::chrono::steady_clock::now();
			AuditSensitiveUse(session.owner->Principal(), "Mapped Input Inject");
			return MakeResponse(request, Status::Ok);
		}
		if (definition->handler == Handler::Logging)
		{
			Decoder decoder(payload);
			std::uint8_t level{};
			std::string message;
			if (!decoder.U8(level) || !decoder.String(message) || decoder.remaining() ||
				message.size() > 4096 || !IsValidUtf8(message) ||
				level > static_cast<std::uint8_t>(LogLevel::Critical))
				return MakeResponse(request, Status::InvalidArgument);
			const auto now = std::chrono::steady_clock::now();
			const auto elapsed = std::chrono::duration<double>(now - session.loggingLastRefill).count();
			session.loggingLastRefill = now;
			session.loggingTokens = std::min(50.0, session.loggingTokens + elapsed * 20.0);
			if (session.loggingTokens < 1.0) return MakeResponse(request, Status::Busy);
			--session.loggingTokens;
			std::string escaped;
			constexpr char hex[] = "0123456789abcdef";
			for (const unsigned char character : message)
			{
				if (character < 0x20 || character == 0x7f)
				{
					escaped.append("\\x"); escaped.push_back(hex[character >> 4]); escaped.push_back(hex[character & 15]);
				}
				else escaped.push_back(static_cast<char>(character));
			}
			LogGuestRecord(session.owner->Principal(), level, escaped);
			return MakeResponse(request, Status::Ok);
		}
		if (definition->handler == Handler::Window)
		{
			if (!payload.empty()) return MakeResponse(request, Status::InvalidArgument);
			WindowStatePayload state{};
			state.frameNumber = CurrentFrameNumber();
#ifndef CEMU_CEX2_TESTING
			const auto& window = WindowSystem::GetWindowInfo();
			state.tvWidth = std::max(0, window.phys_width.load());
			state.tvHeight = std::max(0, window.phys_height.load());
			state.drcWidth = std::max(0, window.phys_pad_width.load());
			state.drcHeight = std::max(0, window.phys_pad_height.load());
			state.dpiScale = static_cast<float>(window.dpi_scale.load());
			state.focused = window.app_active.load();
			state.fullscreen = window.is_fullscreen.load();
#endif
			return MakeResponse(request, Status::Ok,
				{reinterpret_cast<const std::byte*>(&state), sizeof(state)});
		}
		if (definition->handler == Handler::Diagnostics)
		{
			if (!payload.empty()) return MakeResponse(request, Status::InvalidArgument);
			DiagnosticsPayload diagnostics{};
			diagnostics.droppedEvents = session.droppedEvents;
			diagnostics.protocolErrors = session.protocolErrors;
			diagnostics.requests = session.acceptedRequests;
			diagnostics.responses = session.completedResponses;
			diagnostics.bulkBytes = session.bytesCopied;
			return MakeResponse(request, Status::Ok,
				{reinterpret_cast<const std::byte*>(&diagnostics), sizeof(diagnostics)});
		}
		if (definition->handler == Handler::Configuration || definition->handler == Handler::File)
		{
			auto result = Cex2Storage::Dispatch(session.owner->TitleId(), session.owner->Principal(),
				static_cast<ServiceId>(request.serviceId.get()), request.operation.get(), payload);
			if (result.payload.size() > definition->maximumResponse)
				return MakeResponse(request, Status::TooLarge);
			return MakeResponse(request, result.status, result.payload);
		}
		if (definition->handler == Handler::Capture)
		{
			if (session.capture.handle && std::chrono::steady_clock::now() >= session.capture.expires)
				session.capture = {};
			Decoder decoder(payload); std::uint32_t handle{};
			if (request.operation.get() == static_cast<std::uint16_t>(CaptureOperation::Read))
			{
				std::uint32_t offset{};
				if (!decoder.U32(handle) || !decoder.U32(offset) || decoder.remaining() ||
					handle == 0 || handle != session.capture.handle || offset > session.capture.rgb.size())
					return MakeResponse(request, Status::NotFound);
				const auto size = std::min<std::size_t>(64U * 1024U - sizeof(ResponseHeader),
					session.capture.rgb.size() - offset);
				return MakeResponse(request, Status::Ok,
					std::span<const std::byte>(session.capture.rgb).subspan(offset, size));
			}
			if (request.operation.get() == static_cast<std::uint16_t>(CaptureOperation::Close))
			{
				if (!decoder.U32(handle) || decoder.remaining() || handle == 0 || handle != session.capture.handle)
					return MakeResponse(request, Status::NotFound);
				session.capture = {};
				return MakeResponse(request, Status::Ok);
			}
			return MakeResponse(request, Status::NotSupported);
		}
		if (definition->handler != Handler::Core)
			return MakeResponse(request, Status::NotSupported);

		Encoder encoder;
		switch (static_cast<CoreOperation>(request.operation.get()))
		{
		case CoreOperation::GetServices:
			if (!payload.empty())
				return MakeResponse(request, Status::InvalidArgument);
			encoder.U16(static_cast<std::uint16_t>(kServices.size()));
			for (const auto& service : kServices)
			{
				WireServiceDefinition descriptor{};
				descriptor.id = service.id;
				descriptor.version = service.version;
				descriptor.requiredPermission = service.requiredPermission;
				descriptor.maximumRequest = service.maximumRequest;
				descriptor.maximumResponse = service.maximumResponse;
				const auto* bytes = reinterpret_cast<const std::byte*>(&descriptor);
				encoder.Bytes({bytes, sizeof(descriptor)});
			}
			return MakeResponse(request, Status::Ok, encoder.data());
		case CoreOperation::Ping:
			return payload.size() == sizeof(std::uint64_t) ? MakeResponse(request, Status::Ok, payload)
				: MakeResponse(request, Status::InvalidArgument);
		case CoreOperation::GetVersion:
			if (!payload.empty())
				return MakeResponse(request, Status::InvalidArgument);
			encoder.U16(cemuextend::transport::kAbiMajor);
			encoder.U16(cemuextend::transport::kAbiMinor);
			encoder.U16(1); // core service version
			encoder.U16(cemuextend::transport::kOperationVersion);
			encoder.U64(kCemuExtendBuildId);
			return MakeResponse(request, Status::Ok, encoder.data());
		case CoreOperation::Subscribe:
		case CoreOperation::Unsubscribe:
		{
			Decoder decoder(payload);
			std::uint16_t service{};
			if (!decoder.U16(service) || decoder.remaining() != 0 || service == 0)
				return MakeResponse(request, Status::InvalidArgument);
			const auto exists = std::ranges::any_of(kServices,
				[service](const ServiceDefinition& definition) { return definition.id == service; });
			const bool supportsEvents = service == static_cast<std::uint16_t>(ServiceId::Core) ||
				service == static_cast<std::uint16_t>(ServiceId::Input) ||
				service == static_cast<std::uint16_t>(ServiceId::Window);
			if (!exists || !supportsEvents)
				return MakeResponse(request, Status::NotSupported);
			if (service != static_cast<std::uint16_t>(ServiceId::Core) &&
				!HasPermission(session, 1))
				return MakeResponse(request, Status::PermissionDenied);
			if (static_cast<CoreOperation>(request.operation.get()) == CoreOperation::Subscribe)
				session.subscriptions.insert(service);
			else
				session.subscriptions.erase(service);
			return MakeResponse(request, Status::Ok);
		}
		case CoreOperation::GetStatistics:
			if (!payload.empty())
				return MakeResponse(request, Status::InvalidArgument);
			encoder.U64(session.acceptedRequests);
			encoder.U64(session.completedResponses);
			encoder.U32(static_cast<std::uint32_t>(session.responses.size()));
			return MakeResponse(request, Status::Ok, encoder.data());
		case CoreOperation::Cancel:
			return MakeResponse(request, Status::NotSupported);
		default:
			return MakeResponse(request, Status::NotSupported);
		}
	}
};

Cex2Host& Cex2Host::Instance()
{
	static Cex2Host instance;
	return instance;
}

Cex2Host::Cex2Host() : m_impl(std::make_unique<Impl>()) {}
Cex2Host::~Cex2Host() = default;

std::int32_t Cex2Host::Query(ModExecutionContext& owner, std::uint32_t query,
	std::span<std::byte> output)
{
	if (owner.IsStopped() || query != static_cast<std::uint32_t>(cemuextend::transport::Query::Info) ||
		output.size() < sizeof(cemuextend::transport::Info))
		return static_cast<std::int32_t>(Error::InvalidArgument);
	cemuextend::transport::Info info{};
	info.abiMajor = cemuextend::transport::kAbiMajor;
	info.abiMinor = cemuextend::transport::kAbiMinor;
	info.maximumMessageSize = cemuextend::transport::kMaximumMessageSize;
	info.maximumResponseQueue = cemuextend::transport::kMaximumResponseQueue;
	info.maximumPageEntries = cemuextend::transport::kMaximumPageEntries;
	info.hostBuildId = kCemuExtendBuildId;
	info.features = static_cast<std::uint64_t>(cemuextend::transport::Feature::CopyTransport) |
		static_cast<std::uint64_t>(cemuextend::transport::Feature::Cancellation) |
		static_cast<std::uint64_t>(cemuextend::transport::Feature::Pagination) |
		static_cast<std::uint64_t>(cemuextend::transport::Feature::PermissionRevocation);
	info.coreServiceVersion = 1;
	std::memcpy(output.data(), &info, sizeof(info));
	return static_cast<std::int32_t>(Error::Ok);
}

std::int32_t Cex2Host::Open(ModExecutionContext& owner, std::span<const std::byte> options,
	std::uint32_t& sessionId)
{
	if (owner.IsStopped() || options.size() != sizeof(cemuextend::transport::OpenOptions))
		return static_cast<std::int32_t>(Error::InvalidArgument);
	const auto& requested = *reinterpret_cast<const cemuextend::transport::OpenOptions*>(options.data());
	if (requested.abiMajor.get() != cemuextend::transport::kAbiMajor ||
		requested.abiMinor.get() > cemuextend::transport::kAbiMinor)
		return static_cast<std::int32_t>(Error::AbiMismatch);
	if (requested.flags.get() != 0 || requested.reserved.get() != 0 ||
		requested.maximumPendingRequests.get() == 0 ||
		requested.maximumPendingRequests.get() > cemuextend::transport::kMaximumResponseQueue)
		return static_cast<std::int32_t>(Error::InvalidArgument);

	std::lock_guard lock(m_impl->mutex);
	if (m_impl->sessions.size() >= 16)
		return static_cast<std::int32_t>(Error::Busy);
	for (const auto& [id, session] : m_impl->sessions)
		if (Impl::Owns(session, owner))
			return static_cast<std::int32_t>(Error::Busy);
	for (std::uint64_t attempt = 0; attempt <= std::numeric_limits<std::uint32_t>::max(); ++attempt)
	{
		sessionId = m_impl->nextSession++;
		if (sessionId != 0 && !m_impl->sessions.contains(sessionId))
			break;
		sessionId = 0;
	}
	if (!sessionId)
		return static_cast<std::int32_t>(Error::Busy);
	m_impl->sessions.emplace(sessionId, Impl::Session{&owner, owner.AddressSpaceId(), owner.Generation()});
	return static_cast<std::int32_t>(Error::Ok);
}

std::int32_t Cex2Host::Submit(ModExecutionContext& owner, std::uint32_t sessionId,
	std::span<const std::byte> requestBytes)
{
	std::unique_lock lock(m_impl->mutex);
	const auto found = m_impl->sessions.find(sessionId);
	if (found == m_impl->sessions.end() || !Impl::Owns(found->second, owner))
		return static_cast<std::int32_t>(Error::PermissionDenied);
	auto& session = found->second;
	if (session.responses.size() + session.reservedResponses >=
		cemuextend::transport::kMaximumResponseQueue)
		return static_cast<std::int32_t>(Error::Busy);
	if (requestBytes.size() < sizeof(RequestHeader) ||
		requestBytes.size() > cemuextend::transport::kMaximumMessageSize)
	{
		m_impl->sessions.erase(found);
		return static_cast<std::int32_t>(Error::ProtocolError);
	}
	const auto& request = *reinterpret_cast<const RequestHeader*>(requestBytes.data());
	if (request.totalSize.get() != requestBytes.size() || request.correlationId.get() == 0 ||
		request.flags.get() != 0)
	{
		m_impl->sessions.erase(found);
		return static_cast<std::int32_t>(Error::ProtocolError);
	}
	const auto correlationId = request.correlationId.get();
	if (session.pending.contains(correlationId) || std::ranges::any_of(session.responses,
		[correlationId](const auto& bytes) {
			return reinterpret_cast<const ResponseHeader*>(bytes.data())->correlationId.get() == correlationId;
		}))
		return static_cast<std::int32_t>(Error::Busy);
	const auto payload = requestBytes.subspan(sizeof(RequestHeader));
	const auto* definition = FindOperation(request.serviceId.get(), request.operation.get());
	const bool asynchronous = definition && request.operationVersion.get() == definition->version &&
		(definition->handler == Handler::Configuration || definition->handler == Handler::File);
	if (definition && request.operationVersion.get() == definition->version &&
		definition->handler == Handler::Clipboard)
	{
		if (!Impl::HasPermission(session, definition->permission))
		{
			session.responses.push_back(MakeResponse(request, Status::PermissionDenied));
			++session.acceptedRequests; return static_cast<std::int32_t>(Error::Ok);
		}
		if (session.clipboardPending) return static_cast<std::int32_t>(Error::Busy);
		std::string text;
		if (request.operation.get() == static_cast<std::uint16_t>(cemuextend::wire::ClipboardOperation::Get))
		{
			if (!payload.empty()) { session.responses.push_back(MakeResponse(request, Status::InvalidArgument)); ++session.acceptedRequests; return static_cast<std::int32_t>(Error::Ok); }
		}
		else
		{
			cemuextend::wire::Decoder decoder(payload);
			if (!decoder.String(text) || decoder.remaining() || text.size() > 64U * 1024U ||
				!Impl::IsValidUtf8(text))
			{ session.responses.push_back(MakeResponse(request, Status::InvalidArgument)); ++session.acceptedRequests; return static_cast<std::int32_t>(Error::Ok); }
		}
		const auto copiedHeader = request; const auto addressSpaceId = owner.AddressSpaceId();
		const auto generation = owner.Generation(); const auto principal = owner.Principal();
		++session.reservedResponses; session.clipboardPending = true;
		session.pending.emplace(correlationId, Impl::Session::Pending{definition->permission, copiedHeader,
			std::chrono::steady_clock::now() + std::chrono::seconds(5)});
		++session.acceptedRequests; session.bytesCopied += requestBytes.size();
		lock.unlock();
#ifdef CEMU_CEX2_TESTING
		m_impl->Complete(sessionId, addressSpaceId, generation, correlationId, Status::NotSupported);
#else
		AuditSensitiveUse(principal, request.operation.get() == 1 ? "Clipboard Read" : "Clipboard Write");
		if (request.operation.get() == static_cast<std::uint16_t>(cemuextend::wire::ClipboardOperation::Get))
			WindowSystem::GetClipboardTextAsync([impl = m_impl.get(), sessionId, addressSpaceId, generation, correlationId](bool success, std::string result) {
				if (!success) { impl->Complete(sessionId, addressSpaceId, generation, correlationId, Status::IoError); return; }
				if (result.size() > 64U * 1024U || !Impl::IsValidUtf8(result)) { impl->Complete(sessionId, addressSpaceId, generation, correlationId, Status::TooLarge); return; }
				impl->Complete(sessionId, addressSpaceId, generation, correlationId, Status::Ok,
					{reinterpret_cast<const std::byte*>(result.data()), result.size()});
			});
		else
			WindowSystem::SetClipboardTextAsync(std::move(text), [impl = m_impl.get(), sessionId, addressSpaceId, generation, correlationId](bool success) {
				impl->Complete(sessionId, addressSpaceId, generation, correlationId, success ? Status::Ok : Status::IoError);
			});
#endif
		return static_cast<std::int32_t>(Error::Ok);
	}
	if (definition && request.operationVersion.get() == definition->version &&
		definition->handler == Handler::Capture &&
		request.operation.get() == static_cast<std::uint16_t>(cemuextend::wire::CaptureOperation::Open))
	{
		if (!Impl::HasPermission(session, definition->permission))
		{ session.responses.push_back(MakeResponse(request, Status::PermissionDenied)); ++session.acceptedRequests; return static_cast<std::int32_t>(Error::Ok); }
		cemuextend::wire::Decoder decoder(payload); std::uint8_t drc{};
		if (!decoder.U8(drc) || decoder.remaining() || drc > 1)
		{ session.responses.push_back(MakeResponse(request, Status::InvalidArgument)); ++session.acceptedRequests; return static_cast<std::int32_t>(Error::Ok); }
		if (session.capture.handle && std::chrono::steady_clock::now() >= session.capture.expires) session.capture = {};
		if (session.capture.pending || session.capture.handle) return static_cast<std::int32_t>(Error::Busy);
		const auto copiedHeader = request; const auto addressSpaceId = owner.AddressSpaceId();
		const auto generation = owner.Generation(); const auto principal = owner.Principal();
		session.capture.pending = true; session.capture.mainWindow = drc == 0;
		++session.reservedResponses;
		session.pending.emplace(correlationId, Impl::Session::Pending{definition->permission, copiedHeader,
			std::chrono::steady_clock::now() + std::chrono::seconds(5)});
		++session.acceptedRequests; session.bytesCopied += requestBytes.size(); lock.unlock();
#ifdef CEMU_CEX2_TESTING
		m_impl->Complete(sessionId, addressSpaceId, generation, correlationId, Status::NotSupported);
#else
		AuditSensitiveUse(principal, "Capture");
		if (!g_renderer)
			m_impl->Complete(sessionId, addressSpaceId, generation, correlationId, Status::NotSupported);
		else
		{
			const bool mainWindow = drc == 0;
			const auto accepted = g_renderer->RequestScreenshot(
				[impl = m_impl.get(), sessionId, addressSpaceId, generation, correlationId, mainWindow]
				(const std::vector<uint8>& rgb, int width, int height, bool actualMainWindow) {
					Status status = Status::Ok; cemuextend::wire::CaptureOpenResponse response{};
					{
						std::lock_guard guard(impl->mutex); const auto found = impl->sessions.find(sessionId);
						if (found == impl->sessions.end() || found->second.addressSpaceId != addressSpaceId || found->second.generation != generation ||
							!found->second.pending.contains(correlationId) || !found->second.capture.pending) return std::optional<std::string>{};
						auto& capture = found->second.capture; const std::uint64_t w = width > 0 ? width : 0; const std::uint64_t h = height > 0 ? height : 0;
						if (actualMainWindow != mainWindow || w == 0 || h == 0 || w > (64ULL * 1024ULL * 1024ULL) / 3ULL / h || rgb.size() != w * h * 3ULL)
							status = Status::ProtocolError;
						else { capture.pending = false; capture.handle = correlationId; capture.width = width; capture.height = height; capture.expires = std::chrono::steady_clock::now() + std::chrono::seconds(30); capture.rgb.resize(rgb.size()); std::memcpy(capture.rgb.data(), rgb.data(), rgb.size()); response.handle = capture.handle; response.width = width; response.height = height; response.totalBytes = rgb.size(); response.format = 1; response.chunkSize = 64U * 1024U - sizeof(ResponseHeader); }
					}
					impl->Complete(sessionId, addressSpaceId, generation, correlationId, status,
						status == Status::Ok ? std::span<const std::byte>(reinterpret_cast<const std::byte*>(&response), sizeof(response)) : std::span<const std::byte>{});
					return std::optional<std::string>{};
				}, mainWindow);
			if (!accepted) m_impl->Complete(sessionId, addressSpaceId, generation, correlationId, Status::Busy);
		}
#endif
		return static_cast<std::int32_t>(Error::Ok);
	}
	if (asynchronous)
	{
		if (!Impl::HasPermission(session, definition->permission))
		{
			session.responses.push_back(MakeResponse(request, Status::PermissionDenied));
			++session.acceptedRequests;
			return static_cast<std::int32_t>(Error::Ok);
		}
		if (payload.size() > definition->maximumRequest)
		{
			session.responses.push_back(MakeResponse(request, Status::TooLarge));
			++session.acceptedRequests;
			return static_cast<std::int32_t>(Error::Ok);
		}
		const auto copiedHeader = request;
		std::vector<std::byte> copiedPayload(payload.begin(), payload.end());
		const auto titleId = owner.TitleId();
		const auto principal = owner.Principal();
		const auto addressSpaceId = owner.AddressSpaceId();
		const auto generation = owner.Generation();
		const auto permission = definition->permission;
		const auto maximumResponse = definition->maximumResponse;
		const auto service = static_cast<ServiceId>(request.serviceId.get());
		const auto operation = request.operation.get();
		++session.reservedResponses;
		session.pending.emplace(correlationId, Impl::Session::Pending{permission, copiedHeader,
			std::chrono::steady_clock::now() + std::chrono::seconds(5)});
		++session.acceptedRequests;
		session.bytesCopied += requestBytes.size();
		m_impl->Enqueue([impl = m_impl.get(), sessionId, addressSpaceId, generation,
			copiedHeader, copiedPayload = std::move(copiedPayload), titleId, principal,
			permission, maximumResponse, service, operation, correlationId]() mutable {
			auto result = Cex2Storage::Dispatch(titleId, principal, service, operation, copiedPayload);
			std::lock_guard lock(impl->mutex);
			const auto found = impl->sessions.find(sessionId);
			if (found == impl->sessions.end() || found->second.addressSpaceId != addressSpaceId ||
				found->second.generation != generation) return;
			auto& current = found->second;
			const auto pending = current.pending.find(correlationId);
			if (pending == current.pending.end()) return;
			if (!Impl::HasPermission(current, permission)) result = {Status::PermissionDenied};
			if (result.payload.size() > maximumResponse) result = {Status::TooLarge};
			auto response = MakeResponse(copiedHeader, result.status, result.payload);
			current.bytesCopied += response.size();
			current.pending.erase(pending);
			--current.reservedResponses;
			current.responses.push_back(std::move(response));
		});
		return static_cast<std::int32_t>(Error::Ok);
	}
	++session.reservedResponses;
	auto response = m_impl->Dispatch(session, request, payload);
	--session.reservedResponses;
	if (response.size() > cemuextend::transport::kMaximumMessageSize)
		response = MakeResponse(request, Status::TooLarge);
	++session.acceptedRequests;
	session.bytesCopied += requestBytes.size() + response.size();
	session.responses.push_back(std::move(response));
	return static_cast<std::int32_t>(Error::Ok);
}

std::int32_t Cex2Host::Poll(ModExecutionContext& owner, std::uint32_t sessionId,
	std::span<std::byte> output, std::uint32_t& outputSize)
{
	outputSize = 0;
	std::lock_guard lock(m_impl->mutex);
	const auto found = m_impl->sessions.find(sessionId);
	if (found == m_impl->sessions.end() || !Impl::Owns(found->second, owner))
		return static_cast<std::int32_t>(Error::PermissionDenied);
	auto& session = found->second;
	for (auto pending = session.pending.begin(); pending != session.pending.end();)
	{
		if (std::chrono::steady_clock::now() < pending->second.deadline) { ++pending; continue; }
		const auto service = pending->second.header.serviceId.get();
		if (service == static_cast<std::uint16_t>(ServiceId::Clipboard)) session.clipboardPending = false;
		if (service == static_cast<std::uint16_t>(ServiceId::Capture)) session.capture = {};
		session.responses.push_back(MakeResponse(pending->second.header, Status::TimedOut));
		pending = session.pending.erase(pending); --session.reservedResponses;
	}
	if (session.responses.empty())
		return static_cast<std::int32_t>(Error::NotFound);
	if (output.size() < session.responses.front().size())
		return static_cast<std::int32_t>(Error::TooLarge);
	outputSize = static_cast<std::uint32_t>(session.responses.front().size());
	std::memcpy(output.data(), session.responses.front().data(), outputSize);
	session.responses.pop_front();
	++session.completedResponses;
	return static_cast<std::int32_t>(Error::Ok);
}

std::int32_t Cex2Host::Cancel(ModExecutionContext& owner, std::uint32_t sessionId,
	std::uint32_t correlationId)
{
	if (!correlationId)
		return static_cast<std::int32_t>(Error::InvalidArgument);
	std::lock_guard lock(m_impl->mutex);
	const auto found = m_impl->sessions.find(sessionId);
	if (found == m_impl->sessions.end() || !Impl::Owns(found->second, owner))
		return static_cast<std::int32_t>(Error::PermissionDenied);
	if (const auto pending = found->second.pending.find(correlationId);
		pending != found->second.pending.end())
	{
		found->second.responses.push_back(MakeResponse(pending->second.header, Status::Cancelled));
		found->second.pending.erase(pending);
		--found->second.reservedResponses;
		return static_cast<std::int32_t>(Error::Ok);
	}
	for (auto& response : found->second.responses)
	{
		auto& header = *reinterpret_cast<ResponseHeader*>(response.data());
		if (header.correlationId.get() != correlationId)
			continue;
		header.status = static_cast<std::uint16_t>(Status::Cancelled);
		header.totalSize = sizeof(ResponseHeader);
		response.resize(sizeof(ResponseHeader));
		return static_cast<std::int32_t>(Error::Ok);
	}
	return static_cast<std::int32_t>(Error::NotFound);
}

std::int32_t Cex2Host::Close(ModExecutionContext& owner, std::uint32_t sessionId)
{
	std::lock_guard lock(m_impl->mutex);
	const auto found = m_impl->sessions.find(sessionId);
	if (found == m_impl->sessions.end())
		return static_cast<std::int32_t>(Error::NotFound);
	if (!Impl::Owns(found->second, owner))
		return static_cast<std::int32_t>(Error::PermissionDenied);
	m_impl->sessions.erase(found);
	return static_cast<std::int32_t>(Error::Ok);
}

void Cex2Host::CloseOwner(ModExecutionContext& owner)
{
	std::lock_guard lock(m_impl->mutex);
	std::erase_if(m_impl->sessions, [&owner](const auto& entry) {
		const auto& session = entry.second;
		return session.owner == &owner && session.addressSpaceId == owner.AddressSpaceId() &&
			session.generation == owner.Generation();
	});
}

void Cex2Host::CloseAll()
{
	std::lock_guard lock(m_impl->mutex);
	m_impl->sessions.clear();
}

void Cex2Host::ObserveVpad(std::int32_t channel, const VPADStatus& status,
	std::int32_t error, std::int32_t sampleCount)
{
	if (channel < 0 || channel >= 2) return;
	std::lock_guard lock(m_impl->mutex);
	for (auto& [id, session] : m_impl->sessions)
	{
		cemuextend::wire::ObservedVpadState observed{};
		observed.frameNumber = CurrentFrameNumber();
		observed.sampleError = static_cast<std::uint32_t>(error);
		observed.hold = status.hold; observed.trigger = status.trig; observed.release = status.release;
		observed.leftX = status.leftStick.x; observed.leftY = status.leftStick.y;
		observed.rightX = status.rightStick.x; observed.rightY = status.rightStick.y;
		observed.gyroX = status.gyroChange.x; observed.gyroY = status.gyroChange.y;
		observed.gyroZ = status.gyroChange.z;
		observed.touchX = static_cast<float>(status.tpData.x);
		observed.touchY = static_cast<float>(status.tpData.y);
		observed.touched = status.tpData.touch != 0;
		session.observedVpad[channel] = observed;
		session.hasObservedVpad[channel] = sampleCount > 0;
	}
}

void Cex2Host::ApplyMappedVpad(std::int32_t channel, VPADStatus& status)
{
	if (channel < 0 || channel >= 2) return;
	std::lock_guard lock(m_impl->mutex);
	for (auto& [id, session] : m_impl->sessions)
	{
		if (!Impl::HasPermission(session, 4) || !session.hasMappedInjection[channel] ||
			std::chrono::steady_clock::now() - session.mappedInjectionTime[channel] >
				std::chrono::milliseconds(250))
		{
			session.hasMappedInjection[channel] = false;
			continue;
		}
		auto& injected = session.mappedInjection[channel];
		status.hold |= injected.hold.get(); status.trig |= injected.trigger.get();
		status.release |= injected.release.get();
		status.leftStick.x = injected.leftX.get(); status.leftStick.y = injected.leftY.get();
		status.rightStick.x = injected.rightX.get(); status.rightStick.y = injected.rightY.get();
		injected.trigger = 0; injected.release = 0;
	}
}

void Cex2Host::KeyboardEvent(std::uint16_t usage, bool pressed, std::uint8_t modifiers)
{
	if (!usage) return;
	std::lock_guard lock(m_impl->mutex);
	for (auto& [id, session] : m_impl->sessions)
	{
		if (!Impl::HasPermission(session, 1)) continue;
		if (pressed) session.pressedKeyboardUsages.insert(usage);
		else session.pressedKeyboardUsages.erase(usage);
		cemuextend::wire::KeyboardEventPayload event{};
		event.identity.eventId = session.nextInputEventId++;
		event.identity.origin = static_cast<std::uint8_t>(cemuextend::wire::InputOrigin::Physical);
		event.identity.channel = static_cast<std::uint8_t>(cemuextend::wire::InputChannel::Keyboard);
		event.identity.frameNumber = CurrentFrameNumber();
		event.usbHidUsage = usage; event.pressed = pressed; event.modifiers = modifiers;
		m_impl->EmitEvent(session, ServiceId::Input,
			static_cast<std::uint16_t>(cemuextend::wire::InputEvent::Keyboard),
			{reinterpret_cast<const std::byte*>(&event), sizeof(event)});
	}
}

void Cex2Host::KeyboardFocusLost()
{
	std::lock_guard lock(m_impl->mutex);
	for (auto& [id, session] : m_impl->sessions)
	{
		for (const auto usage : session.pressedKeyboardUsages)
		{
			cemuextend::wire::KeyboardEventPayload event{};
			event.identity.eventId = session.nextInputEventId++;
			event.identity.origin = static_cast<std::uint8_t>(cemuextend::wire::InputOrigin::Physical);
			event.identity.channel = static_cast<std::uint8_t>(cemuextend::wire::InputChannel::Keyboard);
			event.identity.frameNumber = CurrentFrameNumber();
			event.usbHidUsage = usage;
			m_impl->EmitEvent(session, ServiceId::Input,
				static_cast<std::uint16_t>(cemuextend::wire::InputEvent::Keyboard),
				{reinterpret_cast<const std::byte*>(&event), sizeof(event)});
		}
		session.pressedKeyboardUsages.clear();
	}
}

void Cex2Host::PermissionsChanged(ModExecutionContext& owner, std::uint32_t permissions)
{
	std::lock_guard lock(m_impl->mutex);
	owner.SetGrantedPermissions(permissions);
	for (auto& [id, session] : m_impl->sessions)
	{
		if (session.owner != &owner) continue;
		if ((permissions & 1) == 0)
		{
			session.subscriptions.erase(static_cast<std::uint16_t>(ServiceId::Input));
			session.subscriptions.erase(static_cast<std::uint16_t>(ServiceId::Window));
			std::erase_if(session.responses, [](const std::vector<std::byte>& response) {
				const auto& header = *reinterpret_cast<const ResponseHeader*>(response.data());
				return header.flags.get() == static_cast<std::uint16_t>(
					cemuextend::transport::ResponseFlag::Event);
			});
		}
		if ((permissions & 4) == 0) session.hasMappedInjection.fill(false);
		for (auto pending = session.pending.begin(); pending != session.pending.end();)
		{
			if (pending->second.permission == 0 ||
				(permissions & pending->second.permission) == pending->second.permission)
			{ ++pending; continue; }
			session.responses.push_back(MakeResponse(pending->second.header, Status::PermissionDenied));
			const auto service = pending->second.header.serviceId.get();
			if (service == static_cast<std::uint16_t>(ServiceId::Clipboard)) session.clipboardPending = false;
			if (service == static_cast<std::uint16_t>(ServiceId::Capture)) session.capture = {};
			pending = session.pending.erase(pending);
			--session.reservedResponses;
		}
	}
}

} // namespace cemuextend_hle
