#include "input/api/DSU/DSUMessages.h"

#include <boost/crc.hpp>

constexpr uint32_t kMagicClient = 'CUSD';
constexpr uint32_t kMagicServer = 'SUSD';
constexpr uint16_t kProtocolVersion = 1001;

MessageHeader::MessageHeader(uint32_t magic, uint32_t uid)
	: m_magic(magic), m_protocol_version(kProtocolVersion), m_uid(uid) { }

void MessageHeader::Finalize(size_t size)
{
	m_packet_size = (uint16_t)(size - sizeof(MessageHeader));
	m_crc32 = CRC32(size);
}

uint32_t MessageHeader::CRC32(size_t size) const
{
	const auto tmp = m_crc32;
	m_crc32 = 0;

	boost::crc_32_type crc;
	crc.process_bytes(this, size);
	const auto result = crc.checksum();

	m_crc32 = tmp;
	return result;
}

bool MessageHeader::IsClientMessage() const { return m_magic == kMagicClient; }
bool MessageHeader::IsServerMessage() const { return m_magic == kMagicServer; }

Message::Message(uint32_t magic, uint32_t uid, MessageType type)
	: MessageHeader(magic, uid), m_message_type(type)
{
	
}

ClientMessage::ClientMessage(uint32_t uid, MessageType message_type)
	: Message(kMagicClient, uid, message_type) { }

VersionRequest::VersionRequest(uint32_t uid)
	: ClientMessage(uid, MessageType::Version)
{
	Finalize(sizeof(VersionRequest));
}

ListPorts::ListPorts(uint32_t uid, uint32_t num_pads_requests, const std::array<uint8_t, 4>& request_indices)
	: ClientMessage(uid, MessageType::Information), m_count(num_pads_requests), m_indices(request_indices)
{
	Finalize(sizeof(ListPorts));
}

DataRequest::DataRequest(uint32_t uid)
	: ClientMessage(uid, MessageType::Data), m_reg_flags(RegisterFlag::AllPads), m_index(0), m_mac_address({})
{
	Finalize(sizeof(DataRequest));
}

DataRequest::DataRequest(uint32_t uid, uint8_t index)
	: ClientMessage(uid, MessageType::Data), m_reg_flags(RegisterFlag::Index), m_index(index), m_mac_address({})
{
	Finalize(sizeof(DataRequest));
}

DataRequest::DataRequest(uint32_t uid, const MACAddress_t& mac_address)
	: ClientMessage(uid, MessageType::Data), m_reg_flags(RegisterFlag::MACAddress), m_index(0), m_mac_address(mac_address)
{
	Finalize(sizeof(DataRequest));
}

DataRequest::DataRequest(uint32_t uid, uint8_t index, const MACAddress_t& mac_address)
	: ClientMessage(uid, MessageType::Data), m_reg_flags(RegisterFlag::Index | RegisterFlag::MACAddress), m_index(index), m_mac_address(mac_address)
{
	Finalize(sizeof(DataRequest));
}

bool ServerMessage::ValidateCRC32(size_t size) const
{
	return GetCRC32() == CRC32(size);
}

bool VersionResponse::IsValid() const
{
	return ValidateCRC32(sizeof(VersionResponse));
}

bool PortInfo::IsValid() const
{
	return ValidateCRC32(sizeof(PortInfo));
}

bool DataResponse::IsValid() const
{
	return ValidateCRC32(sizeof(DataResponse));
}
