#include "TagV0.h"
#include "TLV.h"

#include <algorithm>

namespace
{

constexpr std::size_t kTagSize = 512u;
constexpr std::size_t kMaxBlockCount = kTagSize / sizeof(TagV0::Block);

constexpr uint8 kLockbyteBlock0 = 0xe;
constexpr uint8 kLockbytesStart0 = 0x0;
constexpr uint8 kLockbytesEnd0 = 0x2;
constexpr uint8 kLockbyteBlock1 = 0xf;
constexpr uint8 kLockbytesStart1 = 0x2;
constexpr uint8 kLockbytesEnd1 = 0x8;

constexpr uint8 kNDEFMagicNumber = 0xe1;

// These blocks are not part of the locked area
constexpr bool IsBlockLockedOrReserved(uint8 blockIdx)
{
	// Block 0 is the UID
	if (blockIdx == 0x0)
	{
		return true;
	}

	// Block 0xd is reserved
	if (blockIdx == 0xd)
	{
		return true;
	}

	// Block 0xe and 0xf contains lock / reserved bytes
	if (blockIdx == 0xe || blockIdx == 0xf)
	{
		return true;
	}

	return false;
}

} // namespace

TagV0::TagV0()
{
}

TagV0::~TagV0()
{
}

std::shared_ptr<TagV0> TagV0::FromBytes(const std::span<const std::byte>& data)
{
	// Version 0 tags need at least 512 bytes
	if (data.size() != kTagSize)
	{
		cemuLog_log(LogType::Force, "Error: Version 0 tags should be {} bytes in size", kTagSize);
		return {};
	}

	std::shared_ptr<TagV0> tag = std::make_shared<TagV0>();

	// Parse the locked area before continuing
	if (!tag->ParseLockedArea(data))
	{
		cemuLog_log(LogType::Force, "Error: Failed to parse locked area");
		return {};
	}

	// Now that the locked area is known, parse the data area
	std::vector<std::byte> dataArea;
	if (!tag->ParseDataArea(data, dataArea))
	{
		cemuLog_log(LogType::Force, "Error: Failed to parse data area");
		return {};
	}

	// The first few bytes in the dataArea make up the capability container
	std::copy_n(dataArea.begin(), tag->mCapabilityContainer.size(), std::as_writable_bytes(std::span(tag->mCapabilityContainer)).begin());
	if (!tag->ValidateCapabilityContainer())
	{
		cemuLog_log(LogType::Force, "Error: Failed to validate capability container");
		return {};
	}

	// The rest of the dataArea contains the TLVs
	tag->mTLVs = TLV::FromBytes(std::span(dataArea).subspan(tag->mCapabilityContainer.size()));
	if (tag->mTLVs.empty())
	{
		cemuLog_log(LogType::Force, "Error: Tag contains no TLVs");
		return {};
	}

	// Look for the NDEF tlv
	tag->mNdefTlvIdx = static_cast<size_t>(-1);
	for (std::size_t i = 0; i < tag->mTLVs.size(); i++)
	{
		if (tag->mTLVs[i].GetTag() == TLV::TAG_NDEF)
		{
			tag->mNdefTlvIdx = i;
			break;
		}
	}

	if (tag->mNdefTlvIdx == static_cast<size_t>(-1))
	{
		cemuLog_log(LogType::Force, "Error: Tag contains no NDEF TLV");
		return {};
	}

	// Append locked data
	for (const auto& [key, value] : tag->mLockedBlocks)
	{
		tag->mLockedArea.insert(tag->mLockedArea.end(), value.begin(), value.end());
	}

	return tag;
}

std::vector<std::byte> TagV0::ToBytes() const
{
	std::vector<std::byte> bytes(kTagSize);

	// Insert locked or reserved blocks
	for (const auto& [key, value] : mLockedOrReservedBlocks)
	{
		std::copy(value.begin(), value.end(), bytes.begin() + key * sizeof(Block));
	}

	// Insert locked area
	auto lockedDataIterator = mLockedArea.begin();
	for (const auto& [key, value] : mLockedBlocks)
	{
		std::copy_n(lockedDataIterator, sizeof(Block), bytes.begin() + key * sizeof(Block));
		lockedDataIterator += sizeof(Block);
	}

	// Pack the dataArea into a linear buffer
	std::vector<std::byte> dataArea;
	const auto ccBytes = std::as_bytes(std::span(mCapabilityContainer));
	dataArea.insert(dataArea.end(), ccBytes.begin(), ccBytes.end());
	for (const TLV& tlv : mTLVs)
	{
		const auto tlvBytes = tlv.ToBytes();
		dataArea.insert(dataArea.end(), tlvBytes.begin(), tlvBytes.end());
	}

	// Make sure the dataArea is block size aligned
	dataArea.resize((dataArea.size() + (sizeof(Block)-1)) & ~(sizeof(Block)-1));

	// The rest will be the data area
	auto dataIterator = dataArea.begin();
	for (uint8 currentBlock = 0; currentBlock < kMaxBlockCount; currentBlock++)
	{
		// All blocks which aren't locked make up the dataArea
		if (!IsBlockLocked(currentBlock))
		{
			std::copy_n(dataIterator, sizeof(Block), bytes.begin() + currentBlock * sizeof(Block));
			dataIterator += sizeof(Block);
		}
	}

	return bytes;
}

const TagV0::Block& TagV0::GetUIDBlock() const
{
	return mLockedOrReservedBlocks.at(0);
}

const std::vector<std::byte>& TagV0::GetNDEFData() const
{
	return mTLVs[mNdefTlvIdx].GetValue();
}

const std::vector<std::byte>& TagV0::GetLockedArea() const
{
	return mLockedArea;
}

void TagV0::SetNDEFData(const std::span<const std::byte>& data)
{
	// Update the ndef value
	mTLVs[mNdefTlvIdx].SetValue(data);
}

bool TagV0::ParseLockedArea(const std::span<const std::byte>& data)
{
	uint8 currentBlock = 0;

	// Start by parsing the first set of lock bytes
	for (uint8 i = kLockbytesStart0; i < kLockbytesEnd0; i++)
	{
		uint8 lockByte = uint8(data[kLockbyteBlock0 * sizeof(Block) + i]);

		// Iterate over the individual bits in the lock byte
		for (uint8 j = 0; j < 8; j++)
		{
			// Is block locked?
			if (lockByte & (1u << j))
			{
				Block blk;
				std::copy_n(data.begin() + currentBlock * sizeof(Block), sizeof(Block), blk.begin());

				// The lock bytes themselves are not part of the locked area
				if (!IsBlockLockedOrReserved(currentBlock))
				{
					mLockedBlocks.emplace(currentBlock, blk);
				}
				else
				{
					mLockedOrReservedBlocks.emplace(currentBlock, blk);
				}
			}

			currentBlock++;
		}
	}

	// Parse the second set of lock bytes
	for (uint8 i = kLockbytesStart1; i < kLockbytesEnd1; i++) {
		uint8 lockByte = uint8(data[kLockbyteBlock1 * sizeof(Block) + i]);

		// Iterate over the individual bits in the lock byte
		for (uint8 j = 0; j < 8; j++)
		{
			// Is block locked?
			if (lockByte & (1u << j))
			{
				Block blk;
				std::copy_n(data.begin() + currentBlock * sizeof(Block), sizeof(Block), blk.begin());

				// The lock bytes themselves are not part of the locked area
				if (!IsBlockLockedOrReserved(currentBlock))
				{
					mLockedBlocks.emplace(currentBlock, blk);
				}
				else
				{
					mLockedOrReservedBlocks.emplace(currentBlock, blk);
				}
			}

			currentBlock++;
		}
	}

	return true;
}

bool TagV0::IsBlockLocked(uint8 blockIdx) const
{
	return mLockedBlocks.contains(blockIdx) || IsBlockLockedOrReserved(blockIdx);
}

bool TagV0::ParseDataArea(const std::span<const std::byte>& data, std::vector<std::byte>& dataArea)
{
	for (uint8 currentBlock = 0; currentBlock < kMaxBlockCount; currentBlock++)
	{
		// All blocks which aren't locked make up the dataArea
		if (!IsBlockLocked(currentBlock))
		{
			auto blockOffset = data.begin() + sizeof(Block) * currentBlock;
			dataArea.insert(dataArea.end(), blockOffset, blockOffset + sizeof(Block));
		}
	}

	return true;
}

bool TagV0::ValidateCapabilityContainer()
{
	// NDEF Magic Number
	uint8 nmn = mCapabilityContainer[0];
	if (nmn != kNDEFMagicNumber)
	{
		cemuLog_log(LogType::Force, "Error: CC: Invalid NDEF Magic Number");
		return false;
	}

	// Version Number
	uint8 vno = mCapabilityContainer[1];
	if (vno >> 4 != 1)
	{
		cemuLog_log(LogType::Force, "Error: CC: Invalid Version Number");
		return false;
	}

	// Tag memory size
	uint8 tms = mCapabilityContainer[2];
	if (8u * (tms + 1) < kTagSize)
	{
		cemuLog_log(LogType::Force, "Error: CC: Incomplete tag memory size");
		return false;
	}

	return true;
}
