#pragma once

#include <memory>
#include <span>
#include <map>

#include "TLV.h"

class TagV0
{
public:
	using Block = std::array<std::byte, 0x8>;

public:
	TagV0();
	virtual ~TagV0();

	static std::shared_ptr<TagV0> FromBytes(const std::span<const std::byte>& data);
	std::vector<std::byte> ToBytes() const;

	const Block& GetUIDBlock() const;
	const std::vector<std::byte>& GetNDEFData() const;
	const std::vector<std::byte>& GetLockedArea() const;

	void SetNDEFData(const std::span<const std::byte>& data);

private:
	bool ParseLockedArea(const std::span<const std::byte>& data);
	bool IsBlockLocked(uint8 blockIdx) const;
	bool ParseDataArea(const std::span<const std::byte>& data, std::vector<std::byte>& dataArea);
	bool ValidateCapabilityContainer();

	std::map<uint8, Block> mLockedOrReservedBlocks;
	std::map<uint8, Block> mLockedBlocks;
	std::array<uint8, 0x4> mCapabilityContainer;
	std::vector<TLV> mTLVs;
	std::size_t mNdefTlvIdx;
	std::vector<std::byte> mLockedArea;
};
