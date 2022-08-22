#pragma once

// https://v1993.github.io/cemuhook-protocol/

#include "Common/enumFlags.h"
#include "util/math/vector3.h"

#include <array>
#include <cstdint>

enum class DsState : uint8_t
{
	Disconnected = 0x00,
	Reserved = 0x01,
	Connected = 0x02
};

enum class DsConnection : uint8_t
{
	None = 0x00,
	Usb = 0x01,
	Bluetooth = 0x02
};

enum class DsModel : uint8_t
{
	None = 0,
	DS3 = 1,
	DS4 = 2,
	Generic = 3
};

enum class DsBattery : uint8_t
{
	None = 0x00,
	Dying = 0x01,
	Low = 0x02,
	Medium = 0x03,
	High = 0x04,
	Full = 0x05,
	Charging = 0xEE,
	Charged = 0xEF
};

enum class RegisterFlag : uint8_t
{
	AllPads = 0x00,
	Index = 0x01,
	MACAddress = 0x02
};
ENABLE_BITMASK_OPERATORS(RegisterFlag);

enum class MessageType : uint32_t
{
	Version = 0x100000,
	Information = 0x100001,
	Data = 0x100002,
	Rumble = 0x100003, // TODO
};

using MACAddress_t = std::array<uint8_t, 6>;

#pragma pack(push,1)

class MessageHeader
{
public:
	MessageHeader(uint32_t magic, uint32_t uid);

	[[nodiscard]] uint16_t GetSize() const { return sizeof(MessageHeader) + m_packet_size; }
	[[nodiscard]] bool IsClientMessage() const;
	[[nodiscard]] bool IsServerMessage() const;
	[[nodiscard]] uint32_t GetCRC32() const { return m_crc32; }
	
protected:
	void Finalize(size_t size);

	[[nodiscard]] uint32_t CRC32(size_t size) const;
private:
	uint32_t m_magic;
	uint16_t m_protocol_version;
	uint16_t m_packet_size = 0;
	mutable uint32_t m_crc32 = 0;
	uint32_t m_uid;
};
static_assert(sizeof(MessageHeader) == 0x10);

class Message : public MessageHeader
{
public:
	Message(uint32_t magic, uint32_t uid, MessageType type);

	[[nodiscard]] MessageType GetMessageType() const { return m_message_type; }
private:
	MessageType m_message_type;
};
static_assert(sizeof(Message) == 0x14);

// client messages
class ClientMessage : public Message
{
public:
	ClientMessage(uint32_t uid, MessageType message_type);
};
static_assert(sizeof(ClientMessage) == sizeof(Message));

class VersionRequest : public ClientMessage
{
public:
	VersionRequest(uint32_t uid);
};
static_assert(sizeof(VersionRequest) == sizeof(ClientMessage));

class ListPorts : public ClientMessage
{
public:
	ListPorts(uint32_t uid, uint32_t num_pads_requests, const std::array<uint8_t, 4>& request_indices);

private:
	uint32_t m_count;
	std::array<uint8_t, 4> m_indices;
};

class DataRequest : public ClientMessage
{
public:
	DataRequest(uint32_t uid);
	DataRequest(uint32_t uid, uint8_t index);
	DataRequest(uint32_t uid, const MACAddress_t& mac_address);
	DataRequest(uint32_t uid, uint8_t index, const MACAddress_t& mac_address);

private:
	RegisterFlag m_reg_flags;
	uint8_t m_index;
	MACAddress_t m_mac_address;
};

// server messages
class ServerMessage : public Message
{
public:
	ServerMessage() = delete;

protected:
	[[nodiscard]] bool ValidateCRC32(size_t size) const;
};

class VersionResponse : public ServerMessage
{
public:
	[[nodiscard]] bool IsValid() const;
	[[nodiscard]] uint16_t GetVersion() const { return m_version; }

private:
	uint16_t m_version;
	uint8_t padding[2];
};
static_assert(sizeof(VersionResponse) == 0x18);

struct PortInfoData
{
	uint8_t index;
	DsState state;
	DsModel model;
	DsConnection connection;
	MACAddress_t mac_address;
	DsBattery battery;
	uint8_t is_active;
};

class PortInfo : public ServerMessage
{
public:
	[[nodiscard]] bool IsValid() const;

	[[nodiscard]] const PortInfoData& GetInfo() const { return m_info; }

	[[nodiscard]] uint8_t GetIndex() const { return m_info.index; }
	[[nodiscard]] DsState GetState() const { return m_info.state; }
	[[nodiscard]] DsModel GetModel() const { return m_info.model; }
	[[nodiscard]] DsConnection GetConnection() const { return m_info.connection; }
	[[nodiscard]] MACAddress_t GetMacAddress() const { return m_info.mac_address; }
	[[nodiscard]] DsBattery GetBattery() const { return m_info.battery; }
	[[nodiscard]] bool IsActive() const { return m_info.is_active != 0; }

protected:
	PortInfoData m_info;
};
static_assert(sizeof(PortInfo) == 0x20);

struct TouchPoint
{
	uint8_t active;
	uint8_t index;
	int16_t x, y;
};
static_assert(sizeof(TouchPoint) == 0x6);

struct DataResponseData
{
	uint32_t m_packet_index;

	uint8_t state1;
	uint8_t state2;
	uint8_t ps;
	uint8_t touch;

	// y values are inverted by convention
	uint8_t lx, ly;
	uint8_t rx, ry;

	uint8_t dpad_left;
	uint8_t dpad_down;
	uint8_t dpad_right;
	uint8_t dpad_up;

	uint8_t square;
	uint8_t cross;
	uint8_t circle;
	uint8_t triangle;

	uint8_t r1;
	uint8_t l1;
	uint8_t r2;
	uint8_t l2;

	TouchPoint tpad1, tpad2;

	uint64_t motion_timestamp;
	Vector3f accel;
	Vector3f gyro;
};

class DataResponse : public PortInfo
{
public:
	[[nodiscard]] bool IsValid() const;

	[[nodiscard]] const DataResponseData& GetData() const { return m_data; }
	
	uint32_t GetPacketIndex() const { return m_data.m_packet_index; }
	uint8_t GetState1() const { return m_data.state1; }
	uint8_t GetState2() const { return m_data.state2; }
	uint8_t GetPs() const { return m_data.ps; }
	uint8_t GetTouch() const { return m_data.touch; }
	uint8_t GetLx() const { return m_data.lx; }
	uint8_t GetLy() const { return m_data.ly; }
	uint8_t GetRx() const { return m_data.rx; }
	uint8_t GetRy() const { return m_data.ry; }
	uint8_t GetDpadLeft() const { return m_data.dpad_left; }
	uint8_t GetDpadDown() const { return m_data.dpad_down; }
	uint8_t GetDpadRight() const { return m_data.dpad_right; }
	uint8_t GetDpadUp() const { return m_data.dpad_up; }
	uint8_t GetSquare() const { return m_data.square; }
	uint8_t GetCross() const { return m_data.cross; }
	uint8_t GetCircle() const { return m_data.circle; }
	uint8_t GetTriangle() const { return m_data.triangle; }
	uint8_t GetR1() const { return m_data.r1; }
	uint8_t GetL1() const { return m_data.l1; }
	uint8_t GetR2() const { return m_data.r2; }
	uint8_t GetL2() const { return m_data.l2; }
	const TouchPoint& GetTpad1() const { return m_data.tpad1; }
	const TouchPoint& GetTpad2() const { return m_data.tpad2; }
	uint64_t GetMotionTimestamp() const { return m_data.motion_timestamp; }
	
	const Vector3f& GetAcceleration() const { return m_data.accel; }
	const Vector3f& GetGyro() const { return m_data.gyro; }

private:
	DataResponseData m_data;
};

//static_assert(sizeof(DataResponse) == 0x20);

#pragma pack(pop)