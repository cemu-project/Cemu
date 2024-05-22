#include "ndef.h"

#include <cassert>

namespace ndef
{

	Record::Record()
	 : mFlags(0), mTNF(NDEF_TNF_EMPTY)
	{
	}

	Record::~Record()
	{
	}

	std::optional<Record> Record::FromStream(Stream& stream)
	{
		Record rec;

		// Read record header
		uint8 recHdr;
		stream >> recHdr;
		rec.mFlags = recHdr & ~NDEF_TNF_MASK;
		rec.mTNF = static_cast<TypeNameFormat>(recHdr & NDEF_TNF_MASK);

		// Type length
		uint8 typeLen;
		stream >> typeLen;

		// Payload length;
		uint32 payloadLen;
		if (recHdr & NDEF_SR)
		{
			uint8 len;
			stream >> len;
			payloadLen = len;
		}
		else
		{
			stream >> payloadLen;
		}

		// Some sane limits for the payload size
		if (payloadLen > 2 * 1024 * 1024)
		{
			return {};
		}

		// ID length
		uint8 idLen = 0;
		if (recHdr & NDEF_IL)
		{
			stream >> idLen;
		}

		// Make sure we didn't read past the end of the stream yet
		if (stream.GetError() != Stream::ERROR_OK)
		{
			return {};
		}

		// Type
		rec.mType.resize(typeLen);
		stream.Read(rec.mType);

		// ID
		rec.mID.resize(idLen);
		stream.Read(rec.mID);

		// Payload
		rec.mPayload.resize(payloadLen);
		stream.Read(rec.mPayload);

		// Make sure we didn't read past the end of the stream again
		if (stream.GetError() != Stream::ERROR_OK)
		{
			return {};
		}

		return rec;
	}

	std::vector<std::byte> Record::ToBytes(uint8 flags) const
	{
		std::vector<std::byte> bytes;
		VectorStream stream(bytes, std::endian::big);

		// Combine flags (clear message begin and end flags)
		uint8 finalFlags = mFlags & ~(NDEF_MB | NDEF_ME);
		finalFlags |= flags;

		// Write flags + tnf
		stream << uint8(finalFlags | uint8(mTNF));

		// Type length
		stream << uint8(mType.size());

		// Payload length
		if (IsShort())
		{
			stream << uint8(mPayload.size());
		}
		else
		{
			stream << uint32(mPayload.size());
		}

		// ID length
		if (mFlags & NDEF_IL)
		{
			stream << uint8(mID.size());
		}

		// Type
		stream.Write(mType);

		// ID
		stream.Write(mID);

		// Payload
		stream.Write(mPayload);

		return bytes;
	}

	Record::TypeNameFormat Record::GetTNF() const
	{
		return mTNF;
	}

	const std::vector<std::byte>& Record::GetID() const
	{
		return mID;
	}

	const std::vector<std::byte>& Record::GetType() const
	{
		return mType;
	}

	const std::vector<std::byte>& Record::GetPayload() const
	{
		return mPayload;
	}

	void Record::SetTNF(TypeNameFormat tnf)
	{
		mTNF = tnf;
	}

	void Record::SetID(const std::span<const std::byte>& id)
	{
		cemu_assert(id.size() < 0x100);

		if (id.size() > 0)
		{
			mFlags |= NDEF_IL;
		}
		else
		{
			mFlags &= ~NDEF_IL;
		}

		mID.assign(id.begin(), id.end());
	}

	void Record::SetType(const std::span<const std::byte>& type)
	{
		cemu_assert(type.size() < 0x100);

		mType.assign(type.begin(), type.end());
	}

	void Record::SetPayload(const std::span<const std::byte>& payload)
	{
		// Update short record flag
		if (payload.size() < 0xff)
		{
			mFlags |= NDEF_SR;
		}
		else
		{
			mFlags &= ~NDEF_SR;
		}

		mPayload.assign(payload.begin(), payload.end());
	}

	bool Record::IsLast() const
	{
		return mFlags & NDEF_ME;
	}

	bool Record::IsShort() const
	{
		return mFlags & NDEF_SR;
	}

	Message::Message()
	{
	}

	Message::~Message()
	{
	}

	std::optional<Message> Message::FromBytes(const std::span<const std::byte>& data)
	{
		Message msg;
		SpanStream stream(data, std::endian::big);

		while (stream.GetRemaining() > 0)
		{
			std::optional<Record> rec = Record::FromStream(stream);
			if (!rec)
			{
				cemuLog_log(LogType::Force, "Warning: Failed to parse NDEF Record #{}."
							"Ignoring the remaining {} bytes in NDEF message", msg.mRecords.size(), stream.GetRemaining());
				break;
			}

			msg.mRecords.emplace_back(*rec);

			if ((*rec).IsLast() && stream.GetRemaining() > 0)
			{
				cemuLog_log(LogType::Force, "Warning: Ignoring {} bytes in NDEF message", stream.GetRemaining());
				break;
			}
		}

		if (msg.mRecords.empty())
		{
			return {};
		}

		if (!msg.mRecords.back().IsLast())
		{
			cemuLog_log(LogType::Force, "Error: NDEF message missing end record");
			return {}; 
		}

		return msg;
	}

	std::vector<std::byte> Message::ToBytes() const
	{
		std::vector<std::byte> bytes;

		for (std::size_t i = 0; i < mRecords.size(); i++)
		{
			uint8 flags = 0;

			// Add message begin flag to first record
			if (i == 0)
			{
				flags |= Record::NDEF_MB;
			}

			// Add message end flag to last record
			if (i == mRecords.size() - 1)
			{
				flags |= Record::NDEF_ME;
			}

			std::vector<std::byte> recordBytes = mRecords[i].ToBytes(flags);
			bytes.insert(bytes.end(), recordBytes.begin(), recordBytes.end());
		}

		return bytes;
	}

	void Message::append(const Record& r)
	{
		mRecords.push_back(r);
	}

} // namespace ndef
