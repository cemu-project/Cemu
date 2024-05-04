#pragma once

#include <cstdint>
#include <span>
#include <vector>

class TLV
{
public:
	enum Tag
	{
		TAG_NULL        = 0x00,
		TAG_LOCK_CTRL   = 0x01,
		TAG_MEM_CTRL    = 0x02,
		TAG_NDEF        = 0x03,
		TAG_PROPRIETARY = 0xFD,
		TAG_TERMINATOR  = 0xFE,
	};

public:
	TLV();
	TLV(Tag tag, std::vector<std::byte> value);
	virtual ~TLV();

	static std::vector<TLV> FromBytes(const std::span<std::byte>& data);
	std::vector<std::byte> ToBytes() const;

	Tag GetTag() const;
	const std::vector<std::byte>& GetValue() const;

	void SetTag(Tag tag);
	void SetValue(const std::span<const std::byte>& value);

private:
	Tag mTag;
	std::vector<std::byte> mValue;
};
