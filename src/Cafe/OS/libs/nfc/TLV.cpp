#include "TLV.h"
#include "stream.h"

#include <cassert>

TLV::TLV()
{
}

TLV::TLV(Tag tag, std::vector<std::byte> value)
 : mTag(tag), mValue(std::move(value))
{
}

TLV::~TLV()
{
}

std::vector<TLV> TLV::FromBytes(const std::span<std::byte>& data)
{
	bool hasTerminator = false;
	std::vector<TLV> tlvs;
	SpanStream stream(data, std::endian::big);

	while (stream.GetRemaining() > 0 && !hasTerminator)
	{
		// Read the tag
		uint8 byte;
		stream >> byte;
		Tag tag = static_cast<Tag>(byte);

		switch (tag)
		{
			case TLV::TAG_NULL:
				// Don't need to do anything for NULL tags
				break;
			
			case TLV::TAG_TERMINATOR:
				tlvs.emplace_back(tag, std::vector<std::byte>{});
				hasTerminator = true;
				break;

			default:
			{
				// Read the length
				uint16 length;
				stream >> byte;
				length = byte;

				// If the length is 0xff, 2 bytes with length follow
				if (length == 0xff) {
					stream >> length;
				}

				std::vector<std::byte> value;
				value.resize(length);
				stream.Read(value);

				tlvs.emplace_back(tag, value);
				break;
			}
		}

		if (stream.GetError() != Stream::ERROR_OK)
		{
			cemuLog_log(LogType::Force, "Error: TLV parsing read past end of stream");
			// Clear tlvs to prevent further havoc while parsing ndef data
			tlvs.clear();
			break;
		}
	}

	// This seems to be okay, at least NTAGs don't add a terminator tag
	// if (!hasTerminator)
	// {
	//     cemuLog_log(LogType::Force, "Warning: TLV parsing reached end of stream without terminator tag");
	// }

	return tlvs;
}

std::vector<std::byte> TLV::ToBytes() const
{
	std::vector<std::byte> bytes;
	VectorStream stream(bytes, std::endian::big);

	// Write tag
	stream << uint8(mTag);

	switch (mTag)
	{
		case TLV::TAG_NULL:
		case TLV::TAG_TERMINATOR:
			// Nothing to do here
			break;

		default:
		{
			// Write length (decide if as a 8-bit or 16-bit value)
			if (mValue.size() >= 0xff)
			{
				stream << uint8(0xff);
				stream << uint16(mValue.size());
			}
			else
			{
				stream << uint8(mValue.size());
			}

			// Write value
			stream.Write(mValue);
		}
	}

	return bytes;
}

TLV::Tag TLV::GetTag() const
{
	return mTag;
}

const std::vector<std::byte>& TLV::GetValue() const
{
	return mValue;
}

void TLV::SetTag(Tag tag)
{
	mTag = tag;
}

void TLV::SetValue(const std::span<const std::byte>& value)
{
	// Can only write max 16-bit lengths into TLV
	cemu_assert(value.size() < 0x10000);

	mValue.assign(value.begin(), value.end());
}
