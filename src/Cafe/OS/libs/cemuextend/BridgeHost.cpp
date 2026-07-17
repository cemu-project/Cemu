#include "Common/precompiled.h"

#include "Cafe/OS/libs/cemuextend/BridgeHost.h"
#include "Cafe/OS/libs/cemuextend/BuildId.h"

#include "Cafe/CafeSystem.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/MMU/MMU.h"
#include "Cafe/OS/libs/vpad/vpad.h"
#include "Cemu/Logging/CemuLogging.h"
#include "config/ActiveSettings.h"
#include "config/CemuConfig.h"
#include "gui/interface/WindowSystem.h"
#include "input/InputManager.h"
#include "input/api/Controller.h"

#include <condition_variable>
#include <deque>
#include <fstream>
#include <set>

namespace cemuextend_hle
{
	namespace wire = cemuextend::wire;

	namespace
	{
		constexpr size_t kMaximumResponses = 256;
		constexpr size_t kMaximumInputEvents = 1024;

		uint32 ServiceBit(wire::ServiceId service)
		{
			const auto id = static_cast<uint32>(service);
			return id >= 1 && id <= 32 ? 1U << (id - 1U) : 0;
		}

		std::span<const std::byte> BytesOf(const auto& value)
		{
			return {reinterpret_cast<const std::byte*>(&value), sizeof(value)};
		}

		bool IsValidUtf8(std::string_view text)
		{
			for (size_t index = 0; index < text.size();)
			{
				const auto lead = static_cast<uint8>(text[index]);
				size_t continuation{};
				uint32 codepoint{};
				if (lead < 0x80)
				{
					++index;
					continue;
				}
				if ((lead & 0xe0) == 0xc0) { continuation = 1; codepoint = lead & 0x1f; }
				else if ((lead & 0xf0) == 0xe0) { continuation = 2; codepoint = lead & 0x0f; }
				else if ((lead & 0xf8) == 0xf0) { continuation = 3; codepoint = lead & 0x07; }
				else return false;
				if (index + continuation >= text.size())
					return false;
				for (size_t offset = 1; offset <= continuation; ++offset)
				{
					const auto next = static_cast<uint8>(text[index + offset]);
					if ((next & 0xc0) != 0x80)
						return false;
					codepoint = (codepoint << 6U) | (next & 0x3fU);
				}
				if ((continuation == 1 && codepoint < 0x80) ||
					(continuation == 2 && codepoint < 0x800) ||
					(continuation == 3 && codepoint < 0x10000) ||
					codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff))
					return false;
				index += continuation + 1;
			}
			return true;
		}

		struct PermissionSet
		{
			uint32 read{kDefaultReadMask};
			uint32 write{kDefaultWriteMask};
			uint32 inject{kDefaultInjectMask};
		};

		PermissionSet LoadPermissions(uint64 titleId)
		{
			if (const auto configured = GetConfig().GetCemuExtendGrant(titleId))
				return {configured->read_mask, configured->write_mask, configured->inject_mask};
			return {};
		}

		struct Outbound
		{
			wire::MessageHeader header{};
			std::vector<std::byte> payload;
			bool useBulk{};
		};

		struct CaptureState
		{
			bool pending{};
			bool targetMainWindow{};
			uint32 pendingCorrelation{};
			Renderer::ScreenshotRequestId screenshotRequestId{};
			std::chrono::steady_clock::time_point deadline{};
			std::chrono::steady_clock::time_point expiresAt{};
			uint32 handle{};
			uint32 width{};
			uint32 height{};
			std::vector<std::byte> rgb;
		};

		struct ClipboardState
		{
			bool pending{};
			uint32 correlation{};
			uint16 operation{};
		};

		struct Session
		{
			uint32 id{};
			uint64 titleId{};
			MPTR guestAddress{};
			std::span<std::byte> region;
			wire::BridgeHeader* header{};
			wire::ServiceDirectoryHeader* serviceDirectory{};
			wire::ServiceDescriptor* directoryDescriptors{};
			uint16 directoryCapacity{};
			std::vector<wire::ServiceDescriptor> guestServices;
			std::vector<wire::ServiceDescriptor> publishedServices;
			wire::RingView guestControl;
			wire::RingView hostControl;
			wire::RingView guestEvents;
			wire::RingView hostEvents;
			wire::StatePageView hostState;
			wire::StatePageView guestState;
			wire::BulkAreaView bulk;
			PermissionSet permissions{};
			std::set<uint16> subscriptions;

			std::mutex responseMutex;
			std::deque<Outbound> responses;
			std::mutex inputMutex;
			std::set<uint16> pressedKeyboardUsages;
			std::deque<std::vector<std::byte>> inputEvents;
			std::array<wire::ObservedVpadState, 2> observedVpad{};
			std::array<bool, 2> hasObservedVpad{};
			std::array<wire::ObservedVpadState, 2> mappedInjection{};
			std::array<bool, 2> hasMappedInjection{};
			std::array<std::chrono::steady_clock::time_point, 2> mappedInjectionTime{};
			std::vector<wire::PhysicalControllerState> previousControllers;
			wire::RawInputStateHeader previousRawHeader{};
			bool hasPreviousRaw{};

			CaptureState capture;
			ClipboardState clipboard;
			double loggingTokens{50.0};
			std::chrono::steady_clock::time_point loggingLastRefill{std::chrono::steady_clock::now()};
			std::atomic_bool permissionsDirty{};
			std::atomic_bool running{true};
			std::atomic<uint64> droppedEvents{};
			std::atomic<uint64> protocolErrors{};
			std::atomic<uint64> requests{};
			std::atomic<uint64> responsesSent{};
			std::atomic<uint64> bulkBytes{};
			std::atomic<uint64> nextInputEventId{1};
			std::atomic<sint32> lastError{};
			uint32 lastGuestHeartbeat{};
			uint32 lastFrameCounter{};
			std::chrono::steady_clock::time_point lastGuestHeartbeatTime{};
		};

		wire::ServiceDescriptor MakeDescriptor(wire::ServiceId id, wire::ServiceFeature features,
			wire::Permission required, wire::Permission granted)
		{
			wire::ServiceDescriptor descriptor{};
			descriptor.serviceId = static_cast<uint16>(id);
			descriptor.major = wire::kAbiMajor;
			descriptor.minor = wire::kAbiMinor;
			descriptor.direction = static_cast<uint8>(wire::ServiceDirection::HostProvides);
			descriptor.features = static_cast<uint8>(features);
			descriptor.requiredPermissions = static_cast<uint32>(required);
			descriptor.grantedPermissions = static_cast<uint32>(granted);
			return descriptor;
		}

		wire::Permission GrantedPermissions(const Session& session, wire::ServiceId service)
		{
			const auto bit = ServiceBit(service);
			uint32 result{};
			if (session.permissions.read & bit)
				result |= static_cast<uint32>(wire::Permission::Read);
			if (session.permissions.write & bit)
				result |= static_cast<uint32>(wire::Permission::Write);
			if (session.permissions.inject & bit)
				result |= static_cast<uint32>(wire::Permission::Inject);
			return static_cast<wire::Permission>(result);
		}

		bool HasPermission(const Session& session, wire::ServiceId service, wire::Permission permission)
		{
			return (static_cast<uint32>(GrantedPermissions(session, service)) &
				static_cast<uint32>(permission)) != 0;
		}

		bool Enqueue(Session& session, Outbound outbound)
		{
			std::lock_guard lock(session.responseMutex);
			if (session.responses.size() >= kMaximumResponses)
			{
				session.lastError = static_cast<sint32>(wire::Error::ProtocolError);
				++session.protocolErrors;
				wire::AtomicStore(session.header->connectionState,
					static_cast<uint32>(wire::ConnectionState::Failed));
				session.running = false;
				return false;
			}
			session.responses.push_back(std::move(outbound));
			return true;
		}

		void EnqueueResponse(Session& session, const wire::MessageHeader& request, wire::Status status,
			std::span<const std::byte> payload = {}, bool forceBulk = false)
		{
			Outbound outbound;
			outbound.header.serviceId = request.serviceId.get();
			outbound.header.operation = request.operation.get();
			outbound.header.kind = static_cast<uint8>(wire::MessageKind::Response);
			outbound.header.flags = 0;
			outbound.header.status = static_cast<uint16>(status);
			outbound.header.correlationId = request.correlationId.get();
			outbound.header.timestampNs = static_cast<uint64>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					std::chrono::steady_clock::now().time_since_epoch()).count());
			if (payload.size() > wire::kBulkPayloadSize)
			{
				outbound.header.status = static_cast<uint16>(wire::Status::TooLarge);
			}
			else
			{
				outbound.payload.assign(payload.begin(), payload.end());
				outbound.useBulk = forceBulk ||
					payload.size() > wire::kControlRingCapacity - sizeof(wire::MessageHeader);
			}
			Enqueue(session, std::move(outbound));
		}

		void EnqueueCorrelationResponse(Session& session, wire::ServiceId service, uint16 operation,
			uint32 correlation, wire::Status status, std::span<const std::byte> payload = {})
		{
			wire::MessageHeader request{};
			request.serviceId = static_cast<uint16>(service);
			request.operation = operation;
			request.correlationId = correlation;
			EnqueueResponse(session, request, status, payload);
		}

		bool FlushResponses(Session& session)
		{
			for (;;)
			{
				std::lock_guard lock(session.responseMutex);
				if (session.responses.empty())
					return true;
				auto& response = session.responses.front();
				if (!session.hostControl.valid())
					return false;

				wire::BulkHandle bulkHandle{};
				std::span<const std::byte> wirePayload(response.payload);
				std::array<std::byte, sizeof(wire::BulkHandle)> handleBytes{};
				if (response.useBulk)
				{
					if (!session.bulk.TryWrite(wire::BulkOwner::Host, response.payload, bulkHandle))
						return true;
					std::memcpy(handleBytes.data(), &bulkHandle, sizeof(bulkHandle));
					wirePayload = handleBytes;
					response.header.flags = static_cast<uint8>(wire::MessageFlag::HasBulk);
				}
				if (!session.hostControl.Push(response.header, wirePayload))
				{
					if (response.useBulk)
					{
						std::vector<std::byte> discard;
						session.bulk.ReadAndRelease(wire::BulkOwner::Host, bulkHandle, discard);
					}
					return true;
				}
				if (response.useBulk)
					session.bulkBytes += response.payload.size();
				session.responses.pop_front();
				++session.responsesSent;
			}
		}

		void EmitEvent(Session& session, wire::ServiceId service, uint16 operation,
			std::span<const std::byte> payload, bool force = false)
		{
			if (!force && !session.subscriptions.contains(static_cast<uint16>(service)))
				return;
			if (service != wire::ServiceId::Core &&
				!HasPermission(session, service, wire::Permission::Read))
				return;
			wire::MessageHeader event{};
			event.serviceId = static_cast<uint16>(service);
			event.operation = operation;
			event.kind = static_cast<uint8>(wire::MessageKind::Event);
			event.timestampNs = static_cast<uint64>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					std::chrono::steady_clock::now().time_since_epoch()).count());
			if (!session.hostEvents.Push(event, payload, true))
				++session.droppedEvents;
		}

		std::vector<wire::ServiceDescriptor> HostDescriptors(const Session& session)
		{
			using SF = wire::ServiceFeature;
			using P = wire::Permission;
			const auto make = [&](wire::ServiceId id, uint8 features, P required) {
				return MakeDescriptor(id, static_cast<SF>(features), required, GrantedPermissions(session, id));
			};
			const auto requests = static_cast<uint8>(SF::Requests);
			const auto events = static_cast<uint8>(SF::Events);
			const auto state = static_cast<uint8>(SF::State);
			return {
				make(wire::ServiceId::Core, requests | events, P::Read),
				make(wire::ServiceId::Input, requests | events | state, P::Read | P::Write | P::Inject),
				make(wire::ServiceId::Logging, requests, P::Write),
				make(wire::ServiceId::Configuration, requests, P::Read | P::Write),
				make(wire::ServiceId::File, requests, P::Read | P::Write),
				make(wire::ServiceId::Clipboard, requests, P::Read | P::Write),
				make(wire::ServiceId::Window, requests | events | state, P::Read),
				make(wire::ServiceId::Capture, requests, P::Read),
				make(wire::ServiceId::Diagnostics, requests | state, P::Read),
			};
		}

		bool SnapshotGuestServices(Session& session)
		{
			const auto hostCount = session.serviceDirectory->hostServiceCount.get();
			const auto guestCount = session.serviceDirectory->guestServiceCount.get();
			if (hostCount != 0 || guestCount > session.directoryCapacity)
				return false;

			std::set<uint16> identifiers;
			session.guestServices.clear();
			session.guestServices.reserve(guestCount);
			constexpr uint8 validFeatures = static_cast<uint8>(wire::ServiceFeature::Requests) |
				static_cast<uint8>(wire::ServiceFeature::Events) |
				static_cast<uint8>(wire::ServiceFeature::State);
			for (uint16 index = 0; index < guestCount; ++index)
			{
				auto descriptor = session.directoryDescriptors[index];
				const auto identifier = descriptor.serviceId.get();
				const auto isReservedGuest = identifier == static_cast<uint16>(wire::ServiceId::GameState);
				const auto isCustom = identifier >= static_cast<uint16>(wire::ServiceId::CustomBase);
				if ((!isReservedGuest && !isCustom) || !identifiers.insert(identifier).second ||
					descriptor.direction != static_cast<uint8>(wire::ServiceDirection::GuestProvides) ||
					descriptor.major.get() != wire::kAbiMajor || descriptor.minor.get() > wire::kAbiMinor ||
					descriptor.features == 0 || (descriptor.features & ~validFeatures) != 0 ||
					descriptor.requiredPermissions.get() != 0 || descriptor.grantedPermissions.get() != 0)
					return false;
				session.guestServices.push_back(descriptor);
			}
			return true;
		}

		bool UpdateServiceDirectory(Session& session, bool emitChanged)
		{
			const auto host = HostDescriptors(session);
			if (host.size() + session.guestServices.size() > session.directoryCapacity)
				return false;

			session.publishedServices = host;
			session.publishedServices.insert(session.publishedServices.end(),
				session.guestServices.begin(), session.guestServices.end());

			// Publish the advisory shared directory as a seqlock.  Host decisions and
			// GetServices use publishedServices above and never trust these bytes.
			auto generation = wire::AtomicLoad(session.serviceDirectory->generation,
				std::memory_order_relaxed);
			if (generation & 1U)
				++generation;
			wire::AtomicStore(session.serviceDirectory->generation, generation + 1U);
			std::memset(session.directoryDescriptors, 0,
				session.directoryCapacity * sizeof(*session.directoryDescriptors));
			std::copy(session.publishedServices.begin(), session.publishedServices.end(),
				session.directoryDescriptors);
			session.serviceDirectory->hostServiceCount = static_cast<uint16>(host.size());
			session.serviceDirectory->guestServiceCount = static_cast<uint16>(session.guestServices.size());
			wire::AtomicStore(session.serviceDirectory->generation, generation + 2U);
			wire::AtomicFetchAdd(session.header->generation, 1);
			if (emitChanged)
				EmitEvent(session, wire::ServiceId::Core, static_cast<uint16>(wire::CoreEvent::ServicesChanged), {}, true);
			return true;
		}

		fs::path FileRoot(const Session& session)
		{
			return ActiveSettings::GetUserDataPath("cemuextend/files/{:016x}", session.titleId);
		}

		std::optional<fs::path> ResolveSandboxPath(const Session& session, std::string_view path,
			bool allowMissingFinal = true)
		{
			if (path.empty() || !IsValidUtf8(path))
				return std::nullopt;
			const auto relative = _utf8ToPath(path).lexically_normal();
			if (relative.empty() || relative == "." || relative.is_absolute() || relative.has_root_name())
				return std::nullopt;
			for (const auto& component : relative)
				if (component == "..")
					return std::nullopt;
			const auto root = FileRoot(session);
			std::error_code error;
			fs::create_directories(root, error);
			if (error)
				return std::nullopt;
			auto current = root;
			for (auto iterator = relative.begin(); iterator != relative.end(); ++iterator)
			{
				current /= *iterator;
				const bool final = std::next(iterator) == relative.end();
				const auto status = fs::symlink_status(current, error);
				if (error)
				{
					if (final && allowMissingFinal && error == std::errc::no_such_file_or_directory)
					{
						error.clear();
						break;
					}
					return std::nullopt;
				}
				if (fs::is_symlink(status))
					return std::nullopt;
			}
			return current;
		}

		struct FileNamespaceUsage
		{
			uint64 bytes{};
			uint32 files{};
		};

		std::optional<FileNamespaceUsage> MeasureFileNamespace(const Session& session)
		{
			FileNamespaceUsage usage{};
			std::error_code error;
			const auto root = FileRoot(session);
			for (fs::recursive_directory_iterator iterator(root,
				fs::directory_options::skip_permission_denied, error), end; !error && iterator != end; ++iterator)
			{
				const auto status = iterator->symlink_status(error);
				if (error)
					break;
				if (fs::is_symlink(status))
				{
					if (fs::is_directory(status))
						iterator.disable_recursion_pending();
					continue;
				}
				if (!fs::is_regular_file(status))
					continue;
				const auto size = iterator->file_size(error);
				if (error || size > std::numeric_limits<uint64>::max() - usage.bytes)
					break;
				usage.bytes += size;
				++usage.files;
			}
			return error ? std::nullopt : std::optional{usage};
		}

		using ConfigValues = std::map<std::string, std::pair<wire::ValueType, std::vector<std::byte>>>;
		constexpr size_t kMaximumConfigEntries = 1024;
		constexpr size_t kMaximumConfigBytes = 1024 * 1024;

		bool IsValidConfigValue(wire::ValueType type, size_t size)
		{
			switch (type)
			{
			case wire::ValueType::Boolean: return size == 1;
			case wire::ValueType::SignedInteger:
			case wire::ValueType::UnsignedInteger:
			case wire::ValueType::Float: return size == 8;
			case wire::ValueType::String:
			case wire::ValueType::Bytes: return size <= wire::kBulkPayloadSize;
			default: return false;
			}
		}

		fs::path ConfigPath(const Session& session)
		{
			return ActiveSettings::GetUserDataPath("cemuextend/config/{:016x}.bin", session.titleId);
		}

		bool LoadConfigValues(const Session& session, ConfigValues& values)
		{
			std::ifstream input(ConfigPath(session), std::ios::binary);
			if (!input)
				return true;
			uint32 magic{};
			uint32 count{};
			input.read(reinterpret_cast<char*>(&magic), sizeof(magic));
			input.read(reinterpret_cast<char*>(&count), sizeof(count));
			if (!input || magic != 0x43455843U || count > kMaximumConfigEntries)
				return false;
			size_t totalBytes{};
			for (uint32 index = 0; index < count; ++index)
			{
				uint8 type{};
				uint16 keySize{};
				uint32 valueSize{};
				input.read(reinterpret_cast<char*>(&type), sizeof(type));
				input.read(reinterpret_cast<char*>(&keySize), sizeof(keySize));
				input.read(reinterpret_cast<char*>(&valueSize), sizeof(valueSize));
				const auto valueType = static_cast<wire::ValueType>(type);
				if (!input || keySize == 0 || keySize > 256 ||
					!IsValidConfigValue(valueType, valueSize) ||
					keySize + static_cast<size_t>(valueSize) > kMaximumConfigBytes - totalBytes)
					return false;
				totalBytes += keySize + valueSize;
				std::string key(keySize, '\0');
				std::vector<std::byte> value(valueSize);
				input.read(key.data(), key.size());
				input.read(reinterpret_cast<char*>(value.data()), value.size());
				if (!input || !IsValidUtf8(key) ||
					(valueType == wire::ValueType::String &&
					 !IsValidUtf8({reinterpret_cast<const char*>(value.data()), value.size()})))
					return false;
				if (!values.emplace(std::move(key), std::pair{valueType, std::move(value)}).second)
					return false;
			}
			return true;
		}

		bool SaveConfigValues(const Session& session, const ConfigValues& values)
		{
			if (values.size() > kMaximumConfigEntries)
				return false;
			const auto path = ConfigPath(session);
			std::error_code error;
			fs::create_directories(path.parent_path(), error);
			if (error)
				return false;
			const auto temporary = path.string() + ".tmp";
			std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
			const uint32 magic = 0x43455843U;
			const uint32 count = static_cast<uint32>(values.size());
			output.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
			output.write(reinterpret_cast<const char*>(&count), sizeof(count));
			for (const auto& [key, typed] : values)
			{
				const auto type = static_cast<uint8>(typed.first);
				const auto keySize = static_cast<uint16>(key.size());
				const auto valueSize = static_cast<uint32>(typed.second.size());
				output.write(reinterpret_cast<const char*>(&type), sizeof(type));
				output.write(reinterpret_cast<const char*>(&keySize), sizeof(keySize));
				output.write(reinterpret_cast<const char*>(&valueSize), sizeof(valueSize));
				output.write(key.data(), key.size());
				output.write(reinterpret_cast<const char*>(typed.second.data()), typed.second.size());
			}
			output.flush();
			output.close();
			if (!output)
				return false;
			fs::rename(temporary, path, error);
			if (error)
			{
				fs::remove(path, error);
				error.clear();
				fs::rename(temporary, path, error);
			}
			return !error;
		}

		wire::DiagnosticsPayload Diagnostics(const Session& session)
		{
			wire::DiagnosticsPayload payload{};
			payload.hostHeartbeat = wire::AtomicLoad(session.header->hostHeartbeat);
			payload.guestHeartbeat = wire::AtomicLoad(session.header->guestHeartbeat);
			payload.guestToHostControlUsed = session.guestControl.used();
			payload.hostToGuestControlUsed = session.hostControl.used();
			payload.guestToHostEventUsed = session.guestEvents.used();
			payload.hostToGuestEventUsed = session.hostEvents.used();
			payload.droppedEvents = session.droppedEvents.load();
			payload.protocolErrors = session.protocolErrors.load();
			payload.requests = session.requests.load();
			payload.responses = session.responsesSent.load();
			payload.bulkBytes = session.bulkBytes.load();
			payload.lastError = session.lastError.load();
			return payload;
		}

		wire::Status RequiredPermission(const Session& session, wire::ServiceId service,
			uint16 operation)
		{
			wire::Permission permission = wire::Permission::Read;
			switch (service)
			{
			case wire::ServiceId::Input:
				permission = operation == static_cast<uint16>(wire::InputOperation::InjectMapped) ?
					wire::Permission::Inject : wire::Permission::Write;
				break;
			case wire::ServiceId::Logging:
				permission = wire::Permission::Write;
				break;
			case wire::ServiceId::Configuration:
				permission = operation == static_cast<uint16>(wire::ConfigurationOperation::Get) ||
					operation == static_cast<uint16>(wire::ConfigurationOperation::List) ?
					wire::Permission::Read : wire::Permission::Write;
				break;
			case wire::ServiceId::File:
				permission = operation == static_cast<uint16>(wire::FileOperation::Stat) ||
					operation == static_cast<uint16>(wire::FileOperation::List) ||
					operation == static_cast<uint16>(wire::FileOperation::Read) ?
					wire::Permission::Read : wire::Permission::Write;
				break;
			case wire::ServiceId::Clipboard:
				permission = operation == static_cast<uint16>(wire::ClipboardOperation::Get) ?
					wire::Permission::Read : wire::Permission::Write;
				break;
			default:
				break;
			}
			return HasPermission(session, service, permission) ? wire::Status::Ok : wire::Status::PermissionDenied;
		}

		bool IsKnownOperation(wire::ServiceId service, uint16 operation)
		{
			switch (service)
			{
			case wire::ServiceId::Core: return operation >= 1 && operation <= 7;
			case wire::ServiceId::Input: return operation >= 1 && operation <= 2;
			case wire::ServiceId::Logging: return operation == 1;
			case wire::ServiceId::Configuration: return operation >= 1 && operation <= 4;
			case wire::ServiceId::File: return operation >= 1 && operation <= 7;
			case wire::ServiceId::Clipboard: return operation >= 1 && operation <= 2;
			case wire::ServiceId::Window: return operation == 1;
			case wire::ServiceId::Capture: return operation >= 1 && operation <= 3;
			case wire::ServiceId::Diagnostics: return operation == 1;
			default: return false;
			}
		}

		bool SupportsEvents(wire::ServiceId service)
		{
			return service == wire::ServiceId::Core || service == wire::ServiceId::Input ||
				service == wire::ServiceId::Window;
		}

		void HandleCore(Session& session, const wire::MessageView& request, std::span<const std::byte> payload)
		{
			switch (static_cast<wire::CoreOperation>(request.header.operation.get()))
			{
			case wire::CoreOperation::GetServices:
			{
				wire::Encoder encoder;
				encoder.U16(static_cast<uint16>(session.publishedServices.size()));
				encoder.Bytes({reinterpret_cast<const std::byte*>(session.publishedServices.data()),
					session.publishedServices.size() * sizeof(wire::ServiceDescriptor)});
				EnqueueResponse(session, request.header, wire::Status::Ok, encoder.data());
				break;
			}
			case wire::CoreOperation::Ping:
				EnqueueResponse(session, request.header, wire::Status::Ok, payload);
				break;
			case wire::CoreOperation::GetVersion:
			{
				wire::Encoder encoder;
				encoder.U16(wire::kAbiMajor);
				encoder.U16(wire::kAbiMinor);
				encoder.U64(kCemuExtendBuildId);
				EnqueueResponse(session, request.header, wire::Status::Ok, encoder.data());
				break;
			}
			case wire::CoreOperation::Subscribe:
			case wire::CoreOperation::Unsubscribe:
			{
				wire::Decoder decoder(payload);
				uint16 service{};
				if (!decoder.U16(service) || decoder.remaining())
				{
					EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
					break;
				}
				const auto serviceId = static_cast<wire::ServiceId>(service);
				if (!SupportsEvents(serviceId))
				{
					EnqueueResponse(session, request.header, wire::Status::NotSupported);
					break;
				}
				if (serviceId != wire::ServiceId::Core &&
					!HasPermission(session, serviceId, wire::Permission::Read))
				{
					EnqueueResponse(session, request.header, wire::Status::PermissionDenied);
					break;
				}
				if (request.header.operation.get() == static_cast<uint16>(wire::CoreOperation::Subscribe))
					session.subscriptions.insert(service);
				else
					session.subscriptions.erase(service);
				EnqueueResponse(session, request.header, wire::Status::Ok);
				break;
			}
			case wire::CoreOperation::GetStatistics:
			{
				const auto diagnostics = Diagnostics(session);
				EnqueueResponse(session, request.header, wire::Status::Ok, BytesOf(diagnostics));
				break;
			}
			case wire::CoreOperation::Cancel:
			{
				wire::Decoder decoder(payload);
				uint32 correlation{};
				if (!decoder.U32(correlation) || decoder.remaining() || correlation == 0 ||
					correlation == request.header.correlationId.get())
				{
					EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
					break;
				}
				bool cancelled{};
				if (session.capture.pending && session.capture.pendingCorrelation == correlation)
				{
					const auto screenshotRequestId = session.capture.screenshotRequestId;
					session.capture = {};
					if (g_renderer && screenshotRequestId)
						g_renderer->CancelScreenshotRequest(screenshotRequestId);
					EnqueueCorrelationResponse(session, wire::ServiceId::Capture,
						static_cast<uint16>(wire::CaptureOperation::Open), correlation,
						wire::Status::Cancelled);
					cancelled = true;
				}
				else if (session.clipboard.pending && session.clipboard.correlation == correlation)
				{
					const auto operation = session.clipboard.operation;
					session.clipboard = {};
					EnqueueCorrelationResponse(session, wire::ServiceId::Clipboard, operation,
						correlation, wire::Status::Cancelled);
					cancelled = true;
				}
				EnqueueResponse(session, request.header,
					cancelled ? wire::Status::Ok : wire::Status::NotFound);
				break;
			}
			default:
				EnqueueResponse(session, request.header, wire::Status::NotSupported);
			}
		}

		void HandleInput(Session& session, const wire::MessageView& request, std::span<const std::byte> payload)
		{
			const auto operation = static_cast<wire::InputOperation>(request.header.operation.get());
			if (operation == wire::InputOperation::InjectGuest)
			{
				if (payload.size() != sizeof(wire::ControllerEventPayload))
				{
					EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
					return;
				}
				wire::ControllerEventPayload event{};
				std::memcpy(&event, payload.data(), sizeof(event));
				const std::array axes{event.leftX.get(), event.leftY.get(), event.rightX.get(), event.rightY.get(),
					event.leftTrigger.get(), event.rightTrigger.get()};
				if (!std::ranges::all_of(axes, [](float value) { return std::isfinite(value); }))
				{
					EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
					return;
				}
				event.identity.eventId = session.nextInputEventId.fetch_add(1);
				event.identity.parentEventId = 0;
				event.identity.origin = static_cast<uint8>(wire::InputOrigin::ClientInjected);
				event.identity.channel = static_cast<uint8>(wire::InputChannel::Controller);
				event.identity.frameNumber = LatteGPUState.frameCounter;
				event.leftX = std::clamp(event.leftX.get(), -1.0f, 1.0f);
				event.leftY = std::clamp(event.leftY.get(), -1.0f, 1.0f);
				event.rightX = std::clamp(event.rightX.get(), -1.0f, 1.0f);
				event.rightY = std::clamp(event.rightY.get(), -1.0f, 1.0f);
				event.leftTrigger = std::clamp(event.leftTrigger.get(), 0.0f, 1.0f);
				event.rightTrigger = std::clamp(event.rightTrigger.get(), 0.0f, 1.0f);
				EmitEvent(session, wire::ServiceId::Input,
					static_cast<uint16>(wire::InputEvent::Controller), BytesOf(event));
				EnqueueResponse(session, request.header, wire::Status::Ok);
				return;
			}
			if (operation != wire::InputOperation::InjectMapped ||
				payload.size() != sizeof(uint8) + sizeof(wire::ObservedVpadState))
			{
				EnqueueResponse(session, request.header, wire::Status::NotSupported);
				return;
			}
			const auto channel = std::to_integer<uint8>(payload[0]);
			if (channel >= 2)
			{
				EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
				return;
			}
			wire::ObservedVpadState injected{};
			std::memcpy(&injected, payload.data() + 1, sizeof(injected));
			const std::array sticks{injected.leftX.get(), injected.leftY.get(),
				injected.rightX.get(), injected.rightY.get()};
			if (!std::ranges::all_of(sticks, [](float value) { return std::isfinite(value); }))
			{
				EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
				return;
			}
			injected.leftX = std::clamp(injected.leftX.get(), -1.0f, 1.0f);
			injected.leftY = std::clamp(injected.leftY.get(), -1.0f, 1.0f);
			injected.rightX = std::clamp(injected.rightX.get(), -1.0f, 1.0f);
			injected.rightY = std::clamp(injected.rightY.get(), -1.0f, 1.0f);
			std::lock_guard lock(session.inputMutex);
			session.mappedInjection[channel] = injected;
			session.hasMappedInjection[channel] = true;
			session.mappedInjectionTime[channel] = std::chrono::steady_clock::now();
			EnqueueResponse(session, request.header, wire::Status::Ok);
		}

		void HandleLogging(Session& session, const wire::MessageView& request, std::span<const std::byte> payload)
		{
			wire::Decoder decoder(payload);
			uint8 level{};
			std::string message;
			if (!decoder.U8(level) || !decoder.String(message) || decoder.remaining() || message.size() > 4096 ||
				!IsValidUtf8(message) ||
				level > static_cast<uint8>(wire::LogLevel::Critical))
			{
				EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
				return;
			}
			const auto now = std::chrono::steady_clock::now();
			const auto elapsed = std::chrono::duration<double>(now - session.loggingLastRefill).count();
			session.loggingLastRefill = now;
			session.loggingTokens = std::min(50.0, session.loggingTokens + elapsed * 20.0);
			if (session.loggingTokens < 1.0)
			{
				EnqueueResponse(session, request.header, wire::Status::Busy);
				return;
			}
			session.loggingTokens -= 1.0;
			std::string escaped;
			escaped.reserve(message.size());
			constexpr char hex[] = "0123456789abcdef";
			for (const auto character : message)
			{
				const auto value = static_cast<uint8>(character);
				if (value < 0x20 || value == 0x7f)
				{
					escaped.append("\\x");
					escaped.push_back(hex[value >> 4]);
					escaped.push_back(hex[value & 0xf]);
				}
				else
					escaped.push_back(character);
			}
			cemuLog_log(LogType::Force, "[CemuExtend guest/{:016x}/{}] {}", session.titleId, level, escaped);
			EnqueueResponse(session, request.header, wire::Status::Ok);
		}

		void HandleConfiguration(Session& session, const wire::MessageView& request,
			std::span<const std::byte> payload, std::span<const std::byte> bulkData)
		{
			wire::Decoder decoder(payload);
			std::string key;
			const auto operation = static_cast<wire::ConfigurationOperation>(request.header.operation.get());
			if (!decoder.String(key) ||
				(key.empty() && operation != wire::ConfigurationOperation::List) ||
				key.size() > 256 || !IsValidUtf8(key))
			{
				EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
				return;
			}
			ConfigValues values;
			if (!LoadConfigValues(session, values))
			{
				EnqueueResponse(session, request.header, wire::Status::IoError);
				return;
			}
			switch (operation)
			{
			case wire::ConfigurationOperation::Get:
			{
				const auto found = values.find(key);
				if (found == values.end())
				{
					EnqueueResponse(session, request.header, wire::Status::NotFound);
					break;
				}
				wire::Encoder encoder;
				encoder.U8(static_cast<uint8>(found->second.first));
				encoder.U32(static_cast<uint32>(found->second.second.size()));
				encoder.Bytes(found->second.second);
				EnqueueResponse(session, request.header, wire::Status::Ok, encoder.data());
				break;
			}
			case wire::ConfigurationOperation::Set:
			{
				uint8 type{};
				uint32 size{};
				std::span<const std::byte> value;
				const bool hasBulk = request.header.flags & static_cast<uint8>(wire::MessageFlag::HasBulk);
				if (!decoder.U8(type) || !decoder.U32(size) || size > wire::kBulkPayloadSize ||
					type < static_cast<uint8>(wire::ValueType::Boolean) ||
					type > static_cast<uint8>(wire::ValueType::Bytes))
				{
					EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
					break;
				}
				if (hasBulk)
				{
					if (decoder.remaining() || bulkData.size() != size)
					{
						EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
						break;
					}
					value = bulkData;
				}
				else if (!decoder.Bytes(size, value) || decoder.remaining())
				{
					EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
					break;
				}
				const auto valueType = static_cast<wire::ValueType>(type);
				if (!IsValidConfigValue(valueType, value.size()) ||
					(valueType == wire::ValueType::String &&
					 !IsValidUtf8({reinterpret_cast<const char*>(value.data()), value.size()})))
				{
					EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
					break;
				}
				if (!values.contains(key) && values.size() >= kMaximumConfigEntries)
				{
					EnqueueResponse(session, request.header, wire::Status::TooLarge);
					break;
				}
				size_t totalBytes = key.size() + value.size();
				for (const auto& [candidate, typed] : values)
					if (candidate != key)
						totalBytes += candidate.size() + typed.second.size();
				if (totalBytes > kMaximumConfigBytes)
				{
					EnqueueResponse(session, request.header, wire::Status::TooLarge);
					break;
				}
				values[key] = {valueType, {value.begin(), value.end()}};
				EnqueueResponse(session, request.header,
					SaveConfigValues(session, values) ? wire::Status::Ok : wire::Status::IoError);
				break;
			}
			case wire::ConfigurationOperation::Delete:
				if (!values.erase(key))
					EnqueueResponse(session, request.header, wire::Status::NotFound);
				else
					EnqueueResponse(session, request.header,
						SaveConfigValues(session, values) ? wire::Status::Ok : wire::Status::IoError);
				break;
			case wire::ConfigurationOperation::List:
			{
				wire::Encoder encoder;
				uint32 count{};
				for (const auto& [candidate, value] : values)
					if (candidate.starts_with(key))
						++count;
				encoder.U32(count);
				for (const auto& [candidate, value] : values)
					if (candidate.starts_with(key))
					{
						encoder.String(candidate);
						encoder.U8(static_cast<uint8>(value.first));
					}
				EnqueueResponse(session, request.header, wire::Status::Ok, encoder.data());
				break;
			}
			default:
				EnqueueResponse(session, request.header, wire::Status::NotSupported);
			}
		}

		void HandleFile(Session& session, const wire::MessageView& request, std::span<const std::byte> payload,
			std::span<const std::byte> bulkData)
		{
			wire::Decoder decoder(payload);
			std::string pathText;
			if (!decoder.String(pathText))
			{
				EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
				return;
			}
			const auto path = ResolveSandboxPath(session, pathText);
			if (!path)
			{
				EnqueueResponse(session, request.header, wire::Status::PermissionDenied);
				return;
			}
			std::error_code error;
			constexpr uint64 maximumFileSize = 16ULL * 1024ULL * 1024ULL;
			constexpr auto maximumStreamOffset = static_cast<uint64>(std::numeric_limits<std::streamoff>::max());
			switch (static_cast<wire::FileOperation>(request.header.operation.get()))
			{
			case wire::FileOperation::Stat:
			{
				const auto status = fs::status(*path, error);
				if (error || !fs::exists(status))
				{
					EnqueueResponse(session, request.header, wire::Status::NotFound);
					break;
				}
				wire::FileStatPayload stat{};
				stat.type = fs::is_directory(status) ? 2 : 1;
				if (fs::is_regular_file(status))
					stat.size = fs::file_size(*path, error);
				stat.mode = static_cast<uint32>(status.permissions());
				EnqueueResponse(session, request.header, error ? wire::Status::IoError : wire::Status::Ok,
					BytesOf(stat));
				break;
			}
			case wire::FileOperation::List:
			{
				wire::Encoder encoder;
				std::vector<fs::directory_entry> entries;
				for (fs::directory_iterator iterator(*path, error); !error && iterator != fs::directory_iterator();
					 ++iterator)
				{
					if (iterator->is_symlink(error))
						continue;
					entries.push_back(*iterator);
					if (entries.size() >= 128)
						break;
				}
				if (error)
				{
					EnqueueResponse(session, request.header, wire::Status::IoError);
					break;
				}
				encoder.U32(static_cast<uint32>(entries.size()));
				for (const auto& entry : entries)
				{
					const auto name = _pathToUtf8(entry.path().filename());
					encoder.String(name);
					encoder.U8(entry.is_directory(error) ? 2 : 1);
					encoder.U64(entry.is_regular_file(error) ? entry.file_size(error) : 0);
					if (error)
						break;
				}
				EnqueueResponse(session, request.header,
					error ? wire::Status::IoError : wire::Status::Ok,
					error ? std::span<const std::byte>{} : encoder.data());
				break;
			}
			case wire::FileOperation::Read:
			{
				uint64 offset{};
				uint32 size{};
				if (!decoder.U64(offset) || !decoder.U32(size) || decoder.remaining() ||
					size > wire::kBulkPayloadSize || offset > maximumStreamOffset || offset > maximumFileSize)
				{
					EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
					break;
				}
				std::ifstream input(*path, std::ios::binary);
				if (!input)
				{
					EnqueueResponse(session, request.header, wire::Status::NotFound);
					break;
				}
				const auto fileSize = fs::file_size(*path, error);
				if (error || offset > fileSize)
				{
					EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
					break;
				}
				input.seekg(static_cast<std::streamoff>(offset));
				std::vector<std::byte> bytes(size);
				input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
				bytes.resize(static_cast<size_t>(input.gcount()));
				EnqueueResponse(session, request.header, wire::Status::Ok, bytes, true);
				break;
			}
			case wire::FileOperation::Write:
			{
				uint64 offset{};
				if (!decoder.U64(offset) || decoder.remaining() ||
					!(request.header.flags & static_cast<uint8>(wire::MessageFlag::HasBulk)) ||
					offset > maximumStreamOffset || offset > maximumFileSize ||
					bulkData.size() > maximumFileSize - offset)
				{
					EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
					break;
				}
				const auto usage = MeasureFileNamespace(session);
				const bool existed = fs::exists(*path, error);
				const auto existingSize = existed && !error ? fs::file_size(*path, error) : 0;
				const auto resultingSize = std::max<uint64>(existingSize, offset + bulkData.size());
				constexpr uint64 maximumNamespaceBytes = 64ULL * 1024ULL * 1024ULL;
				constexpr uint32 maximumNamespaceFiles = 4096;
				if (!usage || error || usage->bytes > maximumNamespaceBytes || offset > existingSize ||
					(!existed && usage->files >= maximumNamespaceFiles) ||
					existingSize > usage->bytes || resultingSize - existingSize > maximumNamespaceBytes - usage->bytes)
				{
					EnqueueResponse(session, request.header,
						(!usage || error) ? wire::Status::IoError : wire::Status::TooLarge);
					break;
				}
				fs::create_directories(path->parent_path(), error);
				std::fstream output(*path, std::ios::binary | std::ios::in | std::ios::out);
				if (!output)
				{
					std::ofstream create(*path, std::ios::binary);
					create.close();
					output.open(*path, std::ios::binary | std::ios::in | std::ios::out);
				}
				output.seekp(static_cast<std::streamoff>(offset));
				output.write(reinterpret_cast<const char*>(bulkData.data()), bulkData.size());
				output.flush();
				EnqueueResponse(session, request.header, output ? wire::Status::Ok : wire::Status::IoError);
				break;
			}
			case wire::FileOperation::Mkdir:
				fs::create_directories(*path, error);
				EnqueueResponse(session, request.header, error ? wire::Status::IoError : wire::Status::Ok);
				break;
			case wire::FileOperation::Remove:
				fs::remove_all(*path, error);
				EnqueueResponse(session, request.header, error ? wire::Status::IoError : wire::Status::Ok);
				break;
			case wire::FileOperation::Rename:
			{
				std::string destinationText;
				if (!decoder.String(destinationText) || decoder.remaining())
				{
					EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
					break;
				}
				const auto destination = ResolveSandboxPath(session, destinationText);
				if (!destination)
				{
					EnqueueResponse(session, request.header, wire::Status::PermissionDenied);
					break;
				}
				if (fs::exists(*destination, error) || error)
				{
					EnqueueResponse(session, request.header,
						error ? wire::Status::IoError : wire::Status::Busy);
					break;
				}
				fs::create_directories(destination->parent_path(), error);
				if (!error)
					fs::rename(*path, *destination, error);
				EnqueueResponse(session, request.header, error ? wire::Status::IoError : wire::Status::Ok);
				break;
			}
			default:
				EnqueueResponse(session, request.header, wire::Status::NotSupported);
			}
		}

		void HandleClipboard(Session& session, const wire::MessageView& request, std::span<const std::byte> payload)
		{
			const auto operation = static_cast<wire::ClipboardOperation>(request.header.operation.get());
			const auto sessionId = session.id;
			const auto correlation = request.header.correlationId.get();
			if (session.clipboard.pending)
			{
				EnqueueResponse(session, request.header, wire::Status::Busy);
				return;
			}
			if (operation == wire::ClipboardOperation::Get)
			{
				session.clipboard = {true, correlation, static_cast<uint16>(operation)};
				WindowSystem::GetClipboardTextAsync([sessionId, correlation](bool success, std::string text) {
					BridgeHost::Instance().CompleteClipboard(sessionId, correlation,
						static_cast<uint16>(wire::ClipboardOperation::Get), success, std::move(text));
				});
				return;
			}
			if (operation == wire::ClipboardOperation::Set)
			{
				wire::Decoder decoder(payload);
				std::string text;
				if (!decoder.String(text) || decoder.remaining() || text.size() > wire::kBulkPayloadSize ||
					!IsValidUtf8(text))
				{
					EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
					return;
				}
				session.clipboard = {true, correlation, static_cast<uint16>(operation)};
				WindowSystem::SetClipboardTextAsync(std::move(text), [sessionId, correlation](bool success) {
					BridgeHost::Instance().CompleteClipboard(sessionId, correlation,
						static_cast<uint16>(wire::ClipboardOperation::Set), success, {});
				});
				return;
			}
			EnqueueResponse(session, request.header, wire::Status::NotSupported);
		}

		void HandleWindow(Session& session, const wire::MessageView& request)
		{
			if (request.header.operation.get() != static_cast<uint16>(wire::WindowOperation::Get))
			{
				EnqueueResponse(session, request.header, wire::Status::NotSupported);
				return;
			}
			const auto& window = WindowSystem::GetWindowInfo();
			wire::WindowStatePayload state{};
			state.frameNumber = LatteGPUState.frameCounter;
			state.tvWidth = std::max(0, window.phys_width.load());
			state.tvHeight = std::max(0, window.phys_height.load());
			state.drcWidth = std::max(0, window.phys_pad_width.load());
			state.drcHeight = std::max(0, window.phys_pad_height.load());
			state.dpiScale = static_cast<float>(window.dpi_scale.load());
			state.focused = window.app_active.load();
			state.fullscreen = window.is_fullscreen.load();
			EnqueueResponse(session, request.header, wire::Status::Ok, BytesOf(state));
		}

		void HandleCapture(Session& session, const wire::MessageView& request, std::span<const std::byte> payload)
		{
			switch (static_cast<wire::CaptureOperation>(request.header.operation.get()))
			{
			case wire::CaptureOperation::Open:
			{
				wire::Decoder decoder(payload);
				uint8 drc{};
				if (!decoder.U8(drc) || decoder.remaining() || drc > 1)
				{
					EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
					break;
				}
				if (session.capture.handle && std::chrono::steady_clock::now() >= session.capture.expiresAt)
					session.capture = {};
				if (session.capture.pending || session.capture.handle || !g_renderer)
				{
					EnqueueResponse(session, request.header, g_renderer ? wire::Status::Busy : wire::Status::NotSupported);
					break;
				}
				session.capture.pending = true;
				session.capture.targetMainWindow = drc == 0;
				session.capture.pendingCorrelation = request.header.correlationId.get();
				session.capture.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
				const auto sessionId = session.id;
				const auto correlation = request.header.correlationId.get();
				const auto screenshotRequestId = g_renderer->RequestScreenshot(
					[sessionId, correlation](const std::vector<uint8>& rgb, int width, int height, bool mainWindow) {
						BridgeHost::Instance().CompleteCapture(sessionId, correlation, rgb, width, height, mainWindow);
						return std::optional<std::string>{};
					}, session.capture.targetMainWindow);
				if (!screenshotRequestId)
				{
					session.capture = {};
					EnqueueResponse(session, request.header, wire::Status::Busy);
				}
				else
					session.capture.screenshotRequestId = *screenshotRequestId;
				break;
			}
			case wire::CaptureOperation::Read:
			{
				wire::Decoder decoder(payload);
				uint32 handle{};
				uint32 offset{};
				if (session.capture.handle && std::chrono::steady_clock::now() >= session.capture.expiresAt)
					session.capture = {};
				if (!decoder.U32(handle) || !decoder.U32(offset) || decoder.remaining() ||
					handle == 0 || handle != session.capture.handle || offset > session.capture.rgb.size())
				{
					EnqueueResponse(session, request.header, wire::Status::NotFound);
					break;
				}
				const auto size = std::min<size_t>(wire::kBulkPayloadSize, session.capture.rgb.size() - offset);
				EnqueueResponse(session, request.header, wire::Status::Ok,
					std::span<const std::byte>(session.capture.rgb).subspan(offset, size), true);
				break;
			}
			case wire::CaptureOperation::Close:
			{
				wire::Decoder decoder(payload);
				uint32 handle{};
				if (!decoder.U32(handle) || decoder.remaining() || handle != session.capture.handle)
				{
					EnqueueResponse(session, request.header, wire::Status::NotFound);
					break;
				}
				session.capture = {};
				EnqueueResponse(session, request.header, wire::Status::Ok);
				break;
			}
			default:
				EnqueueResponse(session, request.header, wire::Status::NotSupported);
			}
		}

		bool ProcessRequests(Session& session)
		{
			for (;;)
			{
				{
					std::lock_guard lock(session.responseMutex);
					if (session.responses.size() >= kMaximumResponses)
						return true;
				}
				wire::MessageView request;
				const auto result = session.guestControl.Pop(request);
				if (result == wire::RingReadResult::Empty)
					return true;
				if (result == wire::RingReadResult::ProtocolError ||
					request.header.kind != static_cast<uint8>(wire::MessageKind::Request))
					return false;
				++session.requests;
				constexpr uint8 validFlags = static_cast<uint8>(wire::MessageFlag::HasBulk);
				if (request.header.flags & ~validFlags)
				{
					EnqueueResponse(session, request.header, wire::Status::ProtocolError);
					continue;
				}
				std::span<const std::byte> payload(request.payload);
				std::vector<std::byte> bulkData;
				if (request.header.flags & static_cast<uint8>(wire::MessageFlag::HasBulk))
				{
					if (payload.size() < sizeof(wire::BulkHandle))
					{
						EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
						continue;
					}
					wire::BulkHandle handle{};
					std::memcpy(&handle, payload.data(), sizeof(handle));
					if (!session.bulk.ReadAndRelease(wire::BulkOwner::Guest, handle, bulkData))
					{
						EnqueueResponse(session, request.header, wire::Status::InvalidArgument);
						continue;
					}
					session.bulkBytes += bulkData.size();
					payload = payload.subspan(sizeof(handle));
				}
				const auto service = static_cast<wire::ServiceId>(request.header.serviceId.get());
				if (!IsKnownOperation(service, request.header.operation.get()))
				{
					EnqueueResponse(session, request.header, wire::Status::NotSupported);
					continue;
				}
				if (const auto permission = RequiredPermission(session, service, request.header.operation.get());
					permission != wire::Status::Ok)
				{
					EnqueueResponse(session, request.header, permission);
					continue;
				}
				switch (service)
				{
				case wire::ServiceId::Core: HandleCore(session, request, payload); break;
				case wire::ServiceId::Input: HandleInput(session, request, payload); break;
				case wire::ServiceId::Logging: HandleLogging(session, request, payload); break;
				case wire::ServiceId::Configuration: HandleConfiguration(session, request, payload, bulkData); break;
				case wire::ServiceId::File: HandleFile(session, request, payload, bulkData); break;
				case wire::ServiceId::Clipboard: HandleClipboard(session, request, payload); break;
				case wire::ServiceId::Window: HandleWindow(session, request); break;
				case wire::ServiceId::Capture: HandleCapture(session, request, payload); break;
				case wire::ServiceId::Diagnostics:
					if (request.header.operation.get() == static_cast<uint16>(wire::DiagnosticsOperation::Get))
					{
						const auto diagnostics = Diagnostics(session);
						EnqueueResponse(session, request.header, wire::Status::Ok, BytesOf(diagnostics));
					}
					else EnqueueResponse(session, request.header, wire::Status::NotSupported);
					break;
				default:
					EnqueueResponse(session, request.header, wire::Status::NotSupported);
				}
			}
		}

		bool ProcessGuestEvents(Session& session)
		{
			for (;;)
			{
				wire::MessageView event;
				const auto result = session.guestEvents.Pop(event);
				if (result == wire::RingReadResult::Empty)
					return true;
				if (result == wire::RingReadResult::ProtocolError ||
					event.header.kind != static_cast<uint8>(wire::MessageKind::Event))
					return false;
			}
		}

		std::vector<std::byte> BuildRawInput(Session& session)
		{
			wire::RawInputStateHeader header{};
			header.frameNumber = LatteGPUState.frameCounter;
			const auto& input = InputManager::instance();
			{
				std::shared_lock lock(input.m_main_mouse.m_mutex);
				header.mouseX = input.m_main_mouse.position.x;
				header.mouseY = input.m_main_mouse.position.y;
				header.mouseButtons = (input.m_main_mouse.left_down ? 1U : 0U) |
					(input.m_main_mouse.right_down ? 2U : 0U);
			}
			header.wheel = input.m_mouse_wheel_cumulative.load(std::memory_order_relaxed);

			std::vector<wire::PhysicalControllerState> controllers;
			controllers.reserve(InputManager::kMaxController);
			std::array<std::shared_ptr<ControllerBase>, InputManager::kMaxController> configured{};
			size_t configuredCount{};
			for (size_t player = 0; player < InputManager::kMaxController; ++player)
			{
				if (const auto emulated = input.get_controller(player))
					emulated->copy_unique_controllers(configured, configuredCount);
			}
			for (size_t index = 0; index < configuredCount; ++index)
			{
				const auto& controller = configured[index];
				if (!controller->is_connected())
					continue;
				wire::PhysicalControllerState state{};
				state.deviceId = static_cast<uint16>(controllers.size());
				const auto& source = controller->get_state();
				const uint64 buttons = source.buttons.GetButtonMask64();
				state.buttonsLow = static_cast<uint32>(buttons);
				state.buttonsHigh = static_cast<uint32>(buttons >> 32U);
				state.leftX = source.axis.x;
				state.leftY = source.axis.y;
				state.rightX = source.rotation.x;
				state.rightY = source.rotation.y;
				state.leftTrigger = source.trigger.x;
				state.rightTrigger = source.trigger.y;
				controllers.push_back(state);
			}

			std::set<uint16> keys;
			{
				std::lock_guard lock(session.inputMutex);
				keys = session.pressedKeyboardUsages;
			}
			header.keyboardUsageCount = static_cast<uint16>(keys.size());
			header.controllerCount = static_cast<uint16>(controllers.size());
			std::vector<std::byte> result(sizeof(header) + keys.size() * sizeof(wire::Be16) +
				controllers.size() * sizeof(wire::PhysicalControllerState));
			auto* cursor = result.data();
			std::memcpy(cursor, &header, sizeof(header));
			cursor += sizeof(header);
			for (const auto key : keys)
			{
				const wire::Be16 encoded{key};
				std::memcpy(cursor, &encoded, sizeof(encoded));
				cursor += sizeof(encoded);
			}
			if (!controllers.empty())
				std::memcpy(cursor, controllers.data(), controllers.size() * sizeof(controllers.front()));

			const bool controllersChanged = controllers.size() != session.previousControllers.size() ||
				(!controllers.empty() && std::memcmp(controllers.data(), session.previousControllers.data(),
					controllers.size() * sizeof(controllers.front())) != 0);
			if (session.hasPreviousRaw && (controllersChanged ||
				std::memcmp(&header, &session.previousRawHeader, sizeof(header)) != 0))
			{
				if (header.mouseX.get() != session.previousRawHeader.mouseX.get() ||
					header.mouseY.get() != session.previousRawHeader.mouseY.get() ||
					header.wheel.get() != session.previousRawHeader.wheel.get() ||
					header.mouseButtons.get() != session.previousRawHeader.mouseButtons.get())
				{
					wire::MouseEventPayload event{};
					event.identity.eventId = session.nextInputEventId.fetch_add(1);
					event.identity.origin = static_cast<uint8>(wire::InputOrigin::Physical);
					event.identity.channel = static_cast<uint8>(wire::InputChannel::Mouse);
					event.identity.frameNumber = LatteGPUState.frameCounter;
					event.deltaX = header.mouseX.get() - session.previousRawHeader.mouseX.get();
					event.deltaY = header.mouseY.get() - session.previousRawHeader.mouseY.get();
					event.wheelY = header.wheel.get() - session.previousRawHeader.wheel.get();
					event.buttons = header.mouseButtons.get();
					EmitEvent(session, wire::ServiceId::Input,
						static_cast<uint16>(wire::InputEvent::Mouse), BytesOf(event));
				}
				for (size_t index = 0; index < controllers.size(); ++index)
				{
					if (index < session.previousControllers.size() &&
						std::memcmp(&controllers[index], &session.previousControllers[index], sizeof(controllers[index])) == 0)
						continue;
					wire::ControllerEventPayload event{};
					event.identity.eventId = session.nextInputEventId.fetch_add(1);
					event.identity.origin = static_cast<uint8>(wire::InputOrigin::Physical);
					event.identity.channel = static_cast<uint8>(wire::InputChannel::Controller);
					event.identity.deviceId = static_cast<uint16>(index);
					event.identity.frameNumber = LatteGPUState.frameCounter;
					event.buttonsLow = controllers[index].buttonsLow.get();
					event.buttonsHigh = controllers[index].buttonsHigh.get();
					event.leftX = controllers[index].leftX.get();
					event.leftY = controllers[index].leftY.get();
					event.rightX = controllers[index].rightX.get();
					event.rightY = controllers[index].rightY.get();
					event.leftTrigger = controllers[index].leftTrigger.get();
					event.rightTrigger = controllers[index].rightTrigger.get();
					EmitEvent(session, wire::ServiceId::Input,
						static_cast<uint16>(wire::InputEvent::Controller), BytesOf(event));
				}
			}
			session.previousControllers = controllers;
			session.previousRawHeader = header;
			session.hasPreviousRaw = true;
			return result;
		}

		bool PublishState(Session& session)
		{
			std::vector<wire::StateValue> values;
			if (HasPermission(session, wire::ServiceId::Input, wire::Permission::Read))
			{
				auto raw = BuildRawInput(session);
				values.push_back({static_cast<uint16>(wire::ServiceId::Input),
					static_cast<uint16>(wire::InputState::RawSnapshot), 1, std::move(raw)});
			}

			if (HasPermission(session, wire::ServiceId::Window, wire::Permission::Read))
			{
				wire::WindowStatePayload windowState{};
				const auto& window = WindowSystem::GetWindowInfo();
				windowState.frameNumber = LatteGPUState.frameCounter;
				windowState.tvWidth = std::max(0, window.phys_width.load());
				windowState.tvHeight = std::max(0, window.phys_height.load());
				windowState.drcWidth = std::max(0, window.phys_pad_width.load());
				windowState.drcHeight = std::max(0, window.phys_pad_height.load());
				windowState.dpiScale = static_cast<float>(window.dpi_scale.load());
				windowState.focused = window.app_active.load();
				windowState.fullscreen = window.is_fullscreen.load();
				values.push_back({static_cast<uint16>(wire::ServiceId::Window),
					static_cast<uint16>(wire::WindowState::Snapshot), 1,
					{BytesOf(windowState).begin(), BytesOf(windowState).end()}});
			}

			if (HasPermission(session, wire::ServiceId::Input, wire::Permission::Read))
			{
				std::lock_guard lock(session.inputMutex);
				for (size_t channel = 0; channel < session.observedVpad.size(); ++channel)
				{
					if (!session.hasObservedVpad[channel])
						continue;
					const auto& observed = session.observedVpad[channel];
					values.push_back({static_cast<uint16>(wire::ServiceId::Input),
						static_cast<uint16>(static_cast<uint16>(wire::InputState::ObservedVpad0) +
							static_cast<uint16>(channel)), 1,
						{BytesOf(observed).begin(), BytesOf(observed).end()}});
				}
			}
			if (HasPermission(session, wire::ServiceId::Diagnostics, wire::Permission::Read))
			{
				const auto diagnostics = Diagnostics(session);
				values.push_back({static_cast<uint16>(wire::ServiceId::Diagnostics),
					static_cast<uint16>(wire::DiagnosticsState::Snapshot), 1,
					{BytesOf(diagnostics).begin(), BytesOf(diagnostics).end()}});
			}
			return session.hostState.Publish(values);
		}
	}

	struct BridgeHost::Impl
	{
		std::mutex lifecycleMutex;
		std::unique_ptr<Session> session;
		std::jthread worker;
		std::mutex wakeMutex;
		std::condition_variable wakeCondition;
		std::atomic_bool notified{};
		std::atomic<uint32> nextSessionId{1};

		void Fail(Session& current, wire::Error error)
		{
			current.lastError = static_cast<sint32>(error);
			++current.protocolErrors;
			current.header->lastError = static_cast<sint32>(error);
			wire::AtomicStore(current.header->connectionState,
				static_cast<uint32>(wire::ConnectionState::Failed));
			current.running = false;
		}

		void Run(Session& current, std::stop_token stop)
		{
			SetThreadName("CemuExtBridge");
			while (!stop.stop_requested() && current.running.load())
			{
				{
					std::unique_lock lock(wakeMutex);
					wakeCondition.wait_for(lock, std::chrono::milliseconds(4), [&] {
						return stop.stop_requested() || notified.exchange(false);
					});
				}
				if (stop.stop_requested())
					break;
				if (!CafeSystem::IsTitleRunning())
					continue;
				if (!memory_isAddressRangeAccessible(current.guestAddress,
					static_cast<uint32>(current.region.size())))
				{
					Fail(current, wire::Error::InvalidLayout);
					break;
				}
				const auto now = std::chrono::steady_clock::now();
				const auto guestHeartbeat = wire::AtomicLoad(current.header->guestHeartbeat);
				if (guestHeartbeat != current.lastGuestHeartbeat)
				{
					current.lastGuestHeartbeat = guestHeartbeat;
					current.lastGuestHeartbeatTime = now;
				}
				if (LatteGPUState.frameCounter == current.lastFrameCounter)
					current.lastGuestHeartbeatTime = now;
				else
					current.lastFrameCounter = LatteGPUState.frameCounter;
				if (now - current.lastGuestHeartbeatTime > std::chrono::seconds(5))
				{
					Fail(current, wire::Error::TimedOut);
					break;
				}
				wire::AtomicFetchAdd(current.header->hostHeartbeat, 1);

				if (current.permissionsDirty.exchange(false))
				{
					current.permissions = LoadPermissions(current.titleId);
					std::erase_if(current.subscriptions, [&](uint16 service) {
						return service != static_cast<uint16>(wire::ServiceId::Core) &&
							!HasPermission(current, static_cast<wire::ServiceId>(service), wire::Permission::Read);
					});
					if (!HasPermission(current, wire::ServiceId::Input, wire::Permission::Read))
					{
						std::lock_guard inputLock(current.inputMutex);
						current.inputEvents.clear();
						current.hasPreviousRaw = false;
					}
					if (current.clipboard.pending)
					{
						const auto operation = current.clipboard.operation;
						const auto permission = operation == static_cast<uint16>(wire::ClipboardOperation::Get) ?
							wire::Permission::Read : wire::Permission::Write;
						if (!HasPermission(current, wire::ServiceId::Clipboard, permission))
						{
							const auto correlation = current.clipboard.correlation;
							current.clipboard = {};
							EnqueueCorrelationResponse(current, wire::ServiceId::Clipboard, operation,
								correlation, wire::Status::PermissionDenied);
						}
					}
					if (!HasPermission(current, wire::ServiceId::Capture, wire::Permission::Read) &&
						(current.capture.pending || current.capture.handle))
					{
						const auto correlation = current.capture.pendingCorrelation;
						const auto screenshotRequestId = current.capture.screenshotRequestId;
						const bool wasPending = current.capture.pending;
						current.capture = {};
						if (g_renderer && screenshotRequestId)
							g_renderer->CancelScreenshotRequest(screenshotRequestId);
						if (wasPending)
							EnqueueCorrelationResponse(current, wire::ServiceId::Capture,
								static_cast<uint16>(wire::CaptureOperation::Open), correlation,
								wire::Status::PermissionDenied);
					}
					if (!current.hostState.Clear())
					{
						Fail(current, wire::Error::ProtocolError);
						break;
					}
					if (!UpdateServiceDirectory(current, true))
					{
						Fail(current, wire::Error::ProtocolError);
						break;
					}
				}
				if (current.capture.pending && now > current.capture.deadline)
				{
					const auto correlation = current.capture.pendingCorrelation;
					const auto screenshotRequestId = current.capture.screenshotRequestId;
					current.capture = {};
					if (g_renderer && screenshotRequestId)
						g_renderer->CancelScreenshotRequest(screenshotRequestId);
					EnqueueCorrelationResponse(current, wire::ServiceId::Capture,
						static_cast<uint16>(wire::CaptureOperation::Open), correlation, wire::Status::TimedOut);
				}

				if (!FlushResponses(current))
				{
					Fail(current, wire::Error::ProtocolError);
					break;
				}
				if (!ProcessRequests(current))
				{
					Fail(current, wire::Error::ProtocolError);
					break;
				}
				if (!ProcessGuestEvents(current))
				{
					Fail(current, wire::Error::ProtocolError);
					break;
				}
				for (;;)
				{
					std::vector<std::byte> event;
					{
						std::lock_guard lock(current.inputMutex);
						if (current.inputEvents.empty())
							break;
						event = std::move(current.inputEvents.front());
						current.inputEvents.pop_front();
					}
					EmitEvent(current, wire::ServiceId::Input,
						static_cast<uint16>(wire::InputEvent::Keyboard), event);
				}
				if (!PublishState(current))
				{
					Fail(current, wire::Error::ProtocolError);
					break;
				}
				if (!FlushResponses(current))
				{
					Fail(current, wire::Error::ProtocolError);
					break;
				}
			}
		}
	};

	BridgeHost& BridgeHost::Instance()
	{
		static BridgeHost instance;
		return instance;
	}

	BridgeHost::BridgeHost() : m_impl(std::make_unique<Impl>()) {}
	BridgeHost::~BridgeHost() { Stop(); }

	sint32 BridgeHost::Register(uint32 abiVersion, void* region, uint32 regionSize, uint32& sessionId)
	{
		std::lock_guard lifecycleLock(m_impl->lifecycleMutex);
		if (m_impl->session && !m_impl->session->running.load())
		{
			const auto staleSessionId = m_impl->session->id;
			m_impl->worker.request_stop();
			m_impl->wakeCondition.notify_all();
			if (m_impl->worker.joinable())
				m_impl->worker.join();
			m_impl->session.reset();
			cemuLog_log(LogType::Force,
				"CemuExtend Bridge reaped failed session {} before reconnect", staleSessionId);
		}
		if (m_impl->session)
			return static_cast<sint32>(wire::Error::Busy);
		if ((abiVersion >> 16U) != wire::kAbiMajor || (abiVersion & 0xffffU) > wire::kAbiMinor)
			return static_cast<sint32>(wire::Error::AbiMismatch);
		if (regionSize < wire::kDefaultRegionSize || regionSize > wire::kMaximumRegionSize ||
			memory_getVirtualOffsetFromPointer(region) % wire::kAlignment != 0)
			return static_cast<sint32>(wire::Error::InvalidArgument);
		std::span<std::byte> memory(static_cast<std::byte*>(region), regionSize);
		if (const auto validation = wire::ValidateLayout(memory); !validation)
			return static_cast<sint32>(wire::Error::InvalidLayout);
		wire::BridgeHeader layout{};
		std::memcpy(&layout, memory.data(), sizeof(layout));
		const auto validSlice = [&](uint32 offset, uint32 size) {
			return offset % wire::kAlignment == 0 && offset <= memory.size() &&
				size <= memory.size() - offset;
		};
		if (!validSlice(layout.serviceDirectoryOffset.get(), layout.serviceDirectorySize.get()) ||
			!validSlice(layout.hostStateOffset.get(), layout.hostStateSize.get()) ||
			!validSlice(layout.guestStateOffset.get(), layout.guestStateSize.get()) ||
			!validSlice(layout.guestToHostControlOffset.get(), layout.guestToHostControlSize.get()) ||
			!validSlice(layout.hostToGuestControlOffset.get(), layout.hostToGuestControlSize.get()) ||
			!validSlice(layout.guestToHostEventOffset.get(), layout.guestToHostEventSize.get()) ||
			!validSlice(layout.hostToGuestEventOffset.get(), layout.hostToGuestEventSize.get()) ||
			!validSlice(layout.bulkOffset.get(), layout.bulkSize.get()))
			return static_cast<sint32>(wire::Error::InvalidLayout);

		auto current = std::make_unique<Session>();
		current->id = m_impl->nextSessionId.fetch_add(1);
		if (current->id == 0)
			current->id = m_impl->nextSessionId.fetch_add(1);
		current->titleId = CafeSystem::GetForegroundTitleId();
		current->guestAddress = memory_getVirtualOffsetFromPointer(region);
		current->region = memory;
		current->header = reinterpret_cast<wire::BridgeHeader*>(memory.data());
		current->serviceDirectory = reinterpret_cast<wire::ServiceDirectoryHeader*>(
			memory.data() + layout.serviceDirectoryOffset.get());
		wire::ServiceDirectoryHeader directoryLayout{};
		std::memcpy(&directoryLayout, current->serviceDirectory, sizeof(directoryLayout));
		const auto descriptorBytes = static_cast<uint64>(directoryLayout.capacity.get()) *
			sizeof(wire::ServiceDescriptor);
		if (directoryLayout.descriptorsOffset.get() < sizeof(wire::ServiceDirectoryHeader) ||
			directoryLayout.descriptorsOffset.get() > layout.serviceDirectorySize.get() ||
			descriptorBytes > layout.serviceDirectorySize.get() - directoryLayout.descriptorsOffset.get())
			return static_cast<sint32>(wire::Error::InvalidLayout);
		current->directoryCapacity = directoryLayout.capacity.get();
		current->directoryDescriptors = reinterpret_cast<wire::ServiceDescriptor*>(
			reinterpret_cast<std::byte*>(current->serviceDirectory) + directoryLayout.descriptorsOffset.get());
		current->header->hostBuildId = kCemuExtendBuildId;
		current->header->sessionId = current->id;
		current->permissions = LoadPermissions(current->titleId);
		current->guestControl = {memory.data() + layout.guestToHostControlOffset.get(),
			layout.guestToHostControlSize.get()};
		current->hostControl = {memory.data() + layout.hostToGuestControlOffset.get(),
			layout.hostToGuestControlSize.get()};
		current->guestEvents = {memory.data() + layout.guestToHostEventOffset.get(),
			layout.guestToHostEventSize.get()};
		current->hostEvents = {memory.data() + layout.hostToGuestEventOffset.get(),
			layout.hostToGuestEventSize.get()};
		current->hostState = {memory.data() + layout.hostStateOffset.get(), layout.hostStateSize.get()};
		current->guestState = {memory.data() + layout.guestStateOffset.get(), layout.guestStateSize.get()};
		current->bulk = {memory.data() + layout.bulkOffset.get(), layout.bulkSize.get()};
		current->lastGuestHeartbeat = wire::AtomicLoad(current->header->guestHeartbeat);
		current->lastGuestHeartbeatTime = std::chrono::steady_clock::now();
		current->lastFrameCounter = LatteGPUState.frameCounter;
		if (!SnapshotGuestServices(*current) || !UpdateServiceDirectory(*current, false))
			return static_cast<sint32>(wire::Error::InvalidLayout);
		wire::AtomicStore(current->header->connectionState,
			static_cast<uint32>(wire::ConnectionState::Connected));
		sessionId = current->id;
		auto* sessionPointer = current.get();
		m_impl->session = std::move(current);
		m_impl->worker = std::jthread([this, sessionPointer](std::stop_token stop) {
			m_impl->Run(*sessionPointer, stop);
		});
		cemuLog_log(LogType::Force, "CemuExtend Bridge registered for title {:016x}, session {}",
			sessionPointer->titleId, sessionPointer->id);
		return 0;
	}

	sint32 BridgeHost::Notify(uint32 sessionId, uint32 flags)
	{
		std::lock_guard lifecycleLock(m_impl->lifecycleMutex);
		if (!m_impl->session || m_impl->session->id != sessionId)
			return static_cast<sint32>(wire::Error::NotFound);
		constexpr uint32 validFlags = static_cast<uint32>(wire::NotifyFlag::Control) |
			static_cast<uint32>(wire::NotifyFlag::Event) |
			static_cast<uint32>(wire::NotifyFlag::State) |
			static_cast<uint32>(wire::NotifyFlag::Bulk) |
			static_cast<uint32>(wire::NotifyFlag::Closing);
		if (flags & ~validFlags)
			return static_cast<sint32>(wire::Error::InvalidArgument);
		m_impl->notified = true;
		m_impl->wakeCondition.notify_one();
		return 0;
	}

	sint32 BridgeHost::Unregister(uint32 sessionId)
	{
		{
			std::lock_guard lifecycleLock(m_impl->lifecycleMutex);
			if (!m_impl->session || m_impl->session->id != sessionId)
				return static_cast<sint32>(wire::Error::NotFound);
		}
		Stop();
		return 0;
	}

	void BridgeHost::Stop()
	{
		std::lock_guard lifecycleLock(m_impl->lifecycleMutex);
		if (!m_impl->session)
			return;
		m_impl->session->running = false;
		wire::AtomicStore(m_impl->session->header->connectionState,
			static_cast<uint32>(wire::ConnectionState::Closing));
		if (g_renderer && m_impl->session->capture.pending &&
			m_impl->session->capture.screenshotRequestId)
			g_renderer->CancelScreenshotRequest(m_impl->session->capture.screenshotRequestId);
		m_impl->worker.request_stop();
		m_impl->wakeCondition.notify_all();
		if (m_impl->worker.joinable())
			m_impl->worker.join();
		cemuLog_log(LogType::Force, "CemuExtend Bridge session {} closed", m_impl->session->id);
		m_impl->session.reset();
	}

	void BridgeHost::ObserveVpad(sint32 channel, const VPADStatus& status, sint32 error, sint32 sampleCount)
	{
		if (channel < 0 || channel >= 2)
			return;
		std::lock_guard lifecycleLock(m_impl->lifecycleMutex);
		if (!m_impl->session)
			return;
		wire::ObservedVpadState observed{};
		observed.frameNumber = LatteGPUState.frameCounter;
		observed.sampleError = static_cast<uint32>(error);
		observed.hold = status.hold;
		observed.trigger = status.trig;
		observed.release = status.release;
		observed.leftX = status.leftStick.x;
		observed.leftY = status.leftStick.y;
		observed.rightX = status.rightStick.x;
		observed.rightY = status.rightStick.y;
		observed.gyroX = status.gyroChange.x;
		observed.gyroY = status.gyroChange.y;
		observed.gyroZ = status.gyroChange.z;
		observed.touchX = static_cast<float>(status.tpData.x);
		observed.touchY = static_cast<float>(status.tpData.y);
		observed.touched = status.tpData.touch != 0;
		std::lock_guard inputLock(m_impl->session->inputMutex);
		m_impl->session->observedVpad[channel] = observed;
		m_impl->session->hasObservedVpad[channel] = sampleCount > 0;
	}

	void BridgeHost::ApplyMappedVpad(sint32 channel, VPADStatus& status)
	{
		if (channel < 0 || channel >= 2)
			return;
		std::lock_guard lifecycleLock(m_impl->lifecycleMutex);
		if (!m_impl->session)
			return;
		std::lock_guard inputLock(m_impl->session->inputMutex);
		if (!m_impl->session->hasMappedInjection[channel] ||
			std::chrono::steady_clock::now() - m_impl->session->mappedInjectionTime[channel] >
				std::chrono::milliseconds(250))
		{
			m_impl->session->hasMappedInjection[channel] = false;
			return;
		}
		auto& injected = m_impl->session->mappedInjection[channel];
		status.hold = status.hold | injected.hold.get();
		status.trig = status.trig | injected.trigger.get();
		status.release = status.release | injected.release.get();
		status.leftStick.x = injected.leftX.get();
		status.leftStick.y = injected.leftY.get();
		status.rightStick.x = injected.rightX.get();
		status.rightStick.y = injected.rightY.get();
		injected.trigger = 0;
		injected.release = 0;
	}

	void BridgeHost::KeyboardEvent(uint16 usbHidUsage, bool pressed, uint8 modifiers)
	{
		if (!usbHidUsage)
			return;
		std::lock_guard lifecycleLock(m_impl->lifecycleMutex);
		if (!m_impl->session)
			return;
		auto& session = *m_impl->session;
		std::lock_guard inputLock(session.inputMutex);
		if (pressed)
			session.pressedKeyboardUsages.insert(usbHidUsage);
		else
			session.pressedKeyboardUsages.erase(usbHidUsage);
		if (session.inputEvents.size() >= kMaximumInputEvents)
		{
			++session.droppedEvents;
			return;
		}
		wire::KeyboardEventPayload event{};
		event.identity.eventId = session.nextInputEventId.fetch_add(1);
		event.identity.origin = static_cast<uint8>(wire::InputOrigin::Physical);
		event.identity.channel = static_cast<uint8>(wire::InputChannel::Keyboard);
		event.identity.frameNumber = LatteGPUState.frameCounter;
		event.usbHidUsage = usbHidUsage;
		event.pressed = pressed;
		event.modifiers = modifiers;
		session.inputEvents.emplace_back(BytesOf(event).begin(), BytesOf(event).end());
		m_impl->notified = true;
		m_impl->wakeCondition.notify_one();
	}

	void BridgeHost::KeyboardFocusLost()
	{
		std::lock_guard lifecycleLock(m_impl->lifecycleMutex);
		if (!m_impl->session)
			return;
		auto& session = *m_impl->session;
		std::lock_guard inputLock(session.inputMutex);
		for (const auto usage : session.pressedKeyboardUsages)
		{
			if (session.inputEvents.size() >= kMaximumInputEvents)
			{
				++session.droppedEvents;
				break;
			}
			wire::KeyboardEventPayload event{};
			event.identity.eventId = session.nextInputEventId.fetch_add(1);
			event.identity.origin = static_cast<uint8>(wire::InputOrigin::Physical);
			event.identity.channel = static_cast<uint8>(wire::InputChannel::Keyboard);
			event.identity.frameNumber = LatteGPUState.frameCounter;
			event.usbHidUsage = usage;
			event.pressed = false;
			session.inputEvents.emplace_back(BytesOf(event).begin(), BytesOf(event).end());
		}
		session.pressedKeyboardUsages.clear();
		m_impl->notified = true;
		m_impl->wakeCondition.notify_one();
	}

	void BridgeHost::PermissionsChanged()
	{
		std::lock_guard lifecycleLock(m_impl->lifecycleMutex);
		if (!m_impl->session)
			return;
		m_impl->session->permissionsDirty = true;
		m_impl->notified = true;
		m_impl->wakeCondition.notify_one();
	}

	void BridgeHost::CompleteClipboard(uint32 sessionId, uint32 correlationId, uint16 operation,
		bool success, std::string text)
	{
		std::lock_guard lifecycleLock(m_impl->lifecycleMutex);
		if (!m_impl->session || m_impl->session->id != sessionId)
			return;
		auto& session = *m_impl->session;
		if (!session.clipboard.pending || session.clipboard.correlation != correlationId ||
			session.clipboard.operation != operation)
			return;
		session.clipboard = {};
		const auto permission = operation == static_cast<uint16>(wire::ClipboardOperation::Get) ?
			wire::Permission::Read : wire::Permission::Write;
		if (!HasPermission(session, wire::ServiceId::Clipboard, permission))
		{
			EnqueueCorrelationResponse(session, wire::ServiceId::Clipboard, operation,
				correlationId, wire::Status::PermissionDenied);
			m_impl->notified = true;
			m_impl->wakeCondition.notify_one();
			return;
		}
		wire::Encoder encoder;
		if (success && operation == static_cast<uint16>(wire::ClipboardOperation::Get))
		{
			if (text.size() > wire::kBulkPayloadSize || !IsValidUtf8(text) || !encoder.String(text))
				success = false;
		}
		EnqueueCorrelationResponse(session, wire::ServiceId::Clipboard,
			operation, correlationId,
			success ? wire::Status::Ok : wire::Status::IoError, encoder.data());
		m_impl->notified = true;
		m_impl->wakeCondition.notify_one();
	}

	void BridgeHost::CompleteCapture(uint32 sessionId, uint32 correlationId, std::vector<uint8> rgb,
		int width, int height, bool mainWindow)
	{
		std::lock_guard lifecycleLock(m_impl->lifecycleMutex);
		if (!m_impl->session || m_impl->session->id != sessionId)
			return;
		auto& session = *m_impl->session;
		if (!session.capture.pending || session.capture.pendingCorrelation != correlationId ||
			session.capture.targetMainWindow != mainWindow)
			return;
		if (!HasPermission(session, wire::ServiceId::Capture, wire::Permission::Read))
		{
			session.capture = {};
			EnqueueCorrelationResponse(session, wire::ServiceId::Capture,
				static_cast<uint16>(wire::CaptureOperation::Open), correlationId,
				wire::Status::PermissionDenied);
			m_impl->notified = true;
			m_impl->wakeCondition.notify_one();
			return;
		}
		const auto widthValue = width > 0 ? static_cast<uint64>(width) : 0;
		const auto heightValue = height > 0 ? static_cast<uint64>(height) : 0;
		const auto expectedBytes = widthValue * heightValue * 3ULL;
		if (widthValue == 0 || heightValue == 0 || expectedBytes > 64ULL * 1024ULL * 1024ULL ||
			rgb.size() != expectedBytes)
		{
			session.capture = {};
			EnqueueCorrelationResponse(session, wire::ServiceId::Capture,
				static_cast<uint16>(wire::CaptureOperation::Open), correlationId, wire::Status::ProtocolError);
			m_impl->notified = true;
			m_impl->wakeCondition.notify_one();
			return;
		}
		session.capture.pending = false;
		session.capture.screenshotRequestId = 0;
		session.capture.handle = correlationId ? correlationId : 1;
		session.capture.expiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(30);
		session.capture.width = static_cast<uint32>(width);
		session.capture.height = static_cast<uint32>(height);
		session.capture.rgb.resize(rgb.size());
		std::memcpy(session.capture.rgb.data(), rgb.data(), rgb.size());
		wire::CaptureOpenResponse response{};
		response.handle = session.capture.handle;
		response.width = session.capture.width;
		response.height = session.capture.height;
		response.totalBytes = static_cast<uint32>(session.capture.rgb.size());
		response.format = 1; // RGB8
		response.chunkSize = wire::kBulkPayloadSize;
		EnqueueCorrelationResponse(session, wire::ServiceId::Capture,
			static_cast<uint16>(wire::CaptureOperation::Open), correlationId,
			wire::Status::Ok, BytesOf(response));
		m_impl->notified = true;
		m_impl->wakeCondition.notify_one();
	}
}
