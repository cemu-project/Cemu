#include "Cafe/GraphicPack/GraphicPack2.h"
#include "Cafe/GraphicPack/CemuPatchBinaryV2.h"
#include "Common/FileStream.h"
#include "util/helpers/StringParser.h"
#include "Cemu/PPCAssembler/ppcAssembler.h"
#include "Cafe/OS/RPL/rpl_structs.h"

#include <openssl/sha.h>

sint32 GraphicPack2::GetLengthWithoutComment(const char* str, size_t length)
{
	sint32 index = 0;
	bool isInString = false;
	while (index < length)
	{
		const char c = str[index];
		if (c == '\"')
			isInString = !isInString;
		else if (c == '#' || c == ';')
		{
			if (!isInString)
				return index;
		}
		index++;
	}
	return (sint32)length;
}

void GraphicPack2::LogPatchesSyntaxError(sint32 lineNumber, std::string_view errorMsg)
{
	cemuLog_log(LogType::Force, "Syntax error while parsing patch for graphic pack '{}':", _pathToUtf8(this->GetRulesPath()));
	if(lineNumber >= 0)
		cemuLog_log(LogType::Force, fmt::format("Line {0}: {1}", lineNumber, errorMsg));
	else
		cemuLog_log(LogType::Force, fmt::format("{0}", errorMsg));
	list_patchGroups.clear();
}

enum class RAW_HEX_PARSE_RESULT
{
	NO_MATCH,
	MATCH,
	PARSE_ERROR
};

static constexpr uint32 CEMU_PATCH_BINARY_MAGIC = 0x43504231; // CPB1
static constexpr uint32 CEMU_PATCH_BINARY_V2_MAGIC = CemuPatchBinaryV2::kMagic;
static constexpr uint16 CEMU_PATCH_BINARY_V2_VERSION = CemuPatchBinaryV2::kVersion;
static constexpr uint16 CEMU_PATCH_BINARY_V2_HEADER_SIZE = CemuPatchBinaryV2::kHeaderSize;
static constexpr uint8 CEMU_PATCH_BINARY_ENTRY_LABEL = 1;
static constexpr uint8 CEMU_PATCH_BINARY_ENTRY_DATA = 2;
static constexpr uint32 CEMU_PATCH_BINARY_MAX_GROUPS = 1024;
static constexpr uint32 CEMU_PATCH_BINARY_MAX_ENTRIES = 65536;
static constexpr uint32 CEMU_PATCH_BINARY_MAX_RELOCS = 1000000;
static constexpr uint32 CEMU_PATCH_BINARY_MAX_NAME_SIZE = 255;
static constexpr uint32 CEMU_PATCH_BINARY_MAX_EXPRESSION_SIZE = 4096;

static bool _isValidBinaryPatchText(std::string_view text, size_t maximumSize)
{
	if (text.empty() || text.size() > maximumSize)
		return false;
	for (size_t index = 0; index < text.size();)
	{
		const auto lead = static_cast<uint8>(text[index]);
		if (lead < 0x80)
		{
			if (lead < 0x20 || lead == 0x7f)
				return false;
			++index;
			continue;
		}
		size_t continuation{};
		uint32 codepoint{};
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

static bool _isHexDigit(char c)
{
	return (c >= '0' && c <= '9') ||
		(c >= 'a' && c <= 'f') ||
		(c >= 'A' && c <= 'F');
}

static uint8 _hexDigitToValue(char c)
{
	if (c >= '0' && c <= '9')
		return (uint8)(c - '0');
	if (c >= 'a' && c <= 'f')
		return (uint8)(c - 'a' + 10);
	return (uint8)(c - 'A' + 10);
}

static RAW_HEX_PARSE_RESULT _parseRawHexPatchData(std::string_view text, std::vector<uint8>& outData, std::string& errorMsg)
{
	auto isWhitespace = [](char c) { return c == ' ' || c == '\t'; };
	size_t index = 0;
	bool parsedAnyValue = false;
	while (index < text.size())
	{
		while (index < text.size() && (isWhitespace(text[index]) || text[index] == ','))
			index++;
		if (index >= text.size())
			break;
		if (index + 2 > text.size() || text[index] != '0' || (text[index + 1] != 'x' && text[index + 1] != 'X'))
		{
			if (parsedAnyValue)
				errorMsg = "Unexpected characters in raw binary patch data";
			return parsedAnyValue ? RAW_HEX_PARSE_RESULT::PARSE_ERROR : RAW_HEX_PARSE_RESULT::NO_MATCH;
		}
		index += 2;
		size_t digitStart = index;
		uint32 value = 0;
		while (index < text.size() && _isHexDigit(text[index]))
		{
			if ((index - digitStart) >= 8)
			{
				errorMsg = "Raw binary patch values must fit in 32 bits";
				return RAW_HEX_PARSE_RESULT::PARSE_ERROR;
			}
			value = (value << 4) | _hexDigitToValue(text[index]);
			index++;
		}
		size_t digitCount = index - digitStart;
		if (digitCount == 0)
		{
			errorMsg = "Expected hexadecimal digits after 0x";
			return RAW_HEX_PARSE_RESULT::PARSE_ERROR;
		}
		uint32 byteCount = (uint32)((digitCount + 1) / 2);
		for (sint32 byteIndex = (sint32)byteCount - 1; byteIndex >= 0; byteIndex--)
			outData.emplace_back((uint8)((value >> (byteIndex * 8)) & 0xFF));
		parsedAnyValue = true;
		if (index < text.size() && !isWhitespace(text[index]) && text[index] != ',')
		{
			errorMsg = "Unexpected characters after raw binary patch value";
			return RAW_HEX_PARSE_RESULT::PARSE_ERROR;
		}
	}
	return parsedAnyValue ? RAW_HEX_PARSE_RESULT::MATCH : RAW_HEX_PARSE_RESULT::NO_MATCH;
}

static bool _isValidBinaryRelocType(uint8 relocType)
{
	switch ((PPCASM_RELOC)relocType)
	{
	case PPCASM_RELOC::U32_MASKED_IMM:
	case PPCASM_RELOC::BRANCH_S16:
	case PPCASM_RELOC::BRANCH_S26:
	case PPCASM_RELOC::FLOAT:
	case PPCASM_RELOC::DOUBLE:
	case PPCASM_RELOC::U32:
	case PPCASM_RELOC::U16:
	case PPCASM_RELOC::U8:
		return true;
	default:
		return false;
	}
}

static uint32 _getBinaryRelocSize(PPCASM_RELOC relocType)
{
	switch (relocType)
	{
	case PPCASM_RELOC::DOUBLE:
		return sizeof(betype<double>);
	case PPCASM_RELOC::U16:
		return sizeof(betype<uint16>);
	case PPCASM_RELOC::U8:
		return sizeof(betype<uint8>);
	case PPCASM_RELOC::U32_MASKED_IMM:
	case PPCASM_RELOC::BRANCH_S16:
	case PPCASM_RELOC::BRANCH_S26:
	case PPCASM_RELOC::FLOAT:
	case PPCASM_RELOC::U32:
		return sizeof(betype<uint32>);
	default:
		return 0;
	}
}

static bool _readBinaryPatchString(MemStreamReader& patchesStream, std::string& str,
	size_t maximumSize)
{
	uint16 strLength = patchesStream.readBE<uint16>();
	if (patchesStream.hasError() || strLength == 0 || strLength > maximumSize)
		return false;
	auto strData = patchesStream.readDataNoCopy(strLength);
	if (patchesStream.hasError())
		return false;
	str.assign((const char*)strData.data(), strData.size());
	return _isValidBinaryPatchText(str, maximumSize);
}

void GraphicPack2::CancelParsingPatches()
{
	// unload everything, set error flag
	cemu_assert_debug(false);
}

void GraphicPack2::AddPatchGroup(PatchGroup* group)
{
	if (group->list_moduleMatches.empty() && !group->m_isRpxOnlyTarget)
	{
		LogPatchesSyntaxError(-1, fmt::format("Group \"{}\" has no moduleMatches definition", group->name));
		CancelParsingPatches();
		delete group;
		return;
	}
	// calculate code cave size
	uint32 codeCaveMaxAddr = 0;
	uint32 codeCavePatchedBytes = 0;
	for (auto& itr : group->list_patches)
	{
		PatchEntryInstruction* patchData = dynamic_cast<PatchEntryInstruction*>(itr);
		if (patchData)
		{
			uint32 patchAddr = patchData->getAddr();
			if (patchAddr < 0x00100000)
			{
				// everything in low 1MB of memory we consider part of the code cave
				codeCaveMaxAddr = std::max(codeCaveMaxAddr, patchAddr + patchData->getSize());
				codeCavePatchedBytes += patchData->getSize();
			}
		}

	}
	if (codeCavePatchedBytes < (codeCaveMaxAddr / 8))
	{
		// if less than 1/8th of the code cave is filled print a warning
		cemuLog_log(LogType::Force, "Graphic pack patches: Code cave for group [{}] in gfx pack \"{}\" ranges from 0 to 0x{:x} but has only few instructions. Is this intentional?", group->name, this->m_name, codeCaveMaxAddr);
	}
	group->codeCaveSize = codeCaveMaxAddr;
	list_patchGroups.emplace_back(group);
}

void GraphicPack2::ParseCemuhookPatchesTxtInternal(MemStreamReader& patchesStream)
{
	sint32 lineNumber = 0;
	PatchGroup* currentGroup = nullptr;
	while (true)
	{
		auto lineStr = patchesStream.readLine();
		lineNumber++;
		if (patchesStream.hasError())
			break;
		// trim comment
		size_t lineLength = GetLengthWithoutComment(lineStr.data(), lineStr.size());

		StringTokenParser parser(lineStr.data(), (sint32)lineLength);

		// skip whitespaces at the beginning
		parser.skipWhitespaces();
		// parse line
		if (parser.isEndOfString())
			continue;
		if (parser.compareCharacter(0, '['))
		{
			// group
			parser.skipCharacters(1);
			// find end of group name
			const char* groupNameStr = parser.getCurrentPtr();
			sint32 groupNameLength = parser.skipToCharacter(']');
			if (groupNameLength < 0)
			{

				LogPatchesSyntaxError(lineNumber, "Expected ']'");
				CancelParsingPatches();
				return;
			}
			parser.skipCharacters(1); // skip the ']'
			parser.skipWhitespaces();
			if (!parser.isEndOfString())
			{
				LogPatchesSyntaxError(lineNumber, "Unexpected characters after ']'");
				CancelParsingPatches();
				return;
			}
			// begin new group
			if (currentGroup)
			{
				AddPatchGroup(currentGroup);
			}
			currentGroup = new PatchGroup(this, groupNameStr, groupNameLength);
		}
		else if (parser.compareCharacter(0, '0') && parser.compareCharacterI(1, 'x'))
		{
			// if the line starts with a hex address then it is a patched location
			uint32 patchedAddress;
			if (!parser.parseU32(patchedAddress))
			{
				LogPatchesSyntaxError(lineNumber, "Malformed address");
				CancelParsingPatches();
				return;
			}
			if (parser.matchWordI("=") == false)
			{
				LogPatchesSyntaxError(lineNumber, "Expected '=' after address");
				CancelParsingPatches();
				return;
			}
			parser.skipWhitespaces();
			parser.trimWhitespaces();
			// raw binary patch data
			std::string instrText(parser.getCurrentPtr(), parser.getCurrentLen());
			std::vector<uint8> rawPatchData;
			std::string rawPatchError;
			RAW_HEX_PARSE_RESULT rawPatchResult = _parseRawHexPatchData(instrText, rawPatchData, rawPatchError);
			if (rawPatchResult == RAW_HEX_PARSE_RESULT::PARSE_ERROR)
			{
				LogPatchesSyntaxError(lineNumber, rawPatchError);
				CancelParsingPatches();
				return;
			}
			if (rawPatchResult == RAW_HEX_PARSE_RESULT::MATCH)
			{
				if (currentGroup == nullptr)
				{
					LogPatchesSyntaxError(lineNumber, "Raw binary patch data specified outside of a group");
					CancelParsingPatches();
					return;
				}
				std::vector<PPCAssemblerReloc> noRelocs;
				currentGroup->list_patches.emplace_back(new PatchEntryInstruction(lineNumber, patchedAddress, { rawPatchData.data(), rawPatchData.size() }, noRelocs));
				continue;
			}
			// assemble instruction
			PPCAssemblerInOut ctx{};
			ctx.virtualAddress = patchedAddress;
			if (!ppcAssembler_assembleSingleInstruction(instrText.c_str(), &ctx))
			{
				LogPatchesSyntaxError(lineNumber, fmt::format("Error in assembler: {}", ctx.errorMsg));
				CancelParsingPatches();
				return;
			}
			currentGroup->list_patches.emplace_back(new PatchEntryInstruction(lineNumber, patchedAddress, { ctx.outputData.data(), ctx.outputData.size() }, ctx.list_relocs));
		}
		else if (parser.matchWordI("moduleMatches"))
		{
			if (currentGroup == nullptr)
			{
				LogPatchesSyntaxError(lineNumber, "Specified 'ModuleMatches' outside of a group");
				CancelParsingPatches();
				return;
			}
			if (parser.matchWordI("=") == false)
			{
				LogPatchesSyntaxError(lineNumber, "Expected '=' after ModuleMatches");
				CancelParsingPatches();
				return;
			}
			// read the checksums
			while (true)
			{
				uint32 checksum = 0;
				if (parser.parseU32(checksum) == false)
				{
					LogPatchesSyntaxError(lineNumber, "Invalid value for ModuleMatches");
					CancelParsingPatches();
					return;
				}
				currentGroup->list_moduleMatches.emplace_back(checksum);
				if (parser.matchWordI(",") == false)
					break;
			}
			parser.skipWhitespaces();
			if (!parser.isEndOfString())
			{
				LogPatchesSyntaxError(lineNumber, "Unexpected character in line");
				CancelParsingPatches();
				return;
			}
			continue;
		}
		else
		{
			// Cemuhook requires that user defined symbols start with _ but we are more lenient and allow them to start with letters too
			// the downside is that there is some ambiguity and parsing gets a little bit more complex
			
			// check for <symbolName> = pattern
			StringTokenParser bakParser;
			const char* symbolStr;
			sint32 symbolLen;
			parser.storeParserState(&bakParser);
			if (parser.parseSymbolName(symbolStr, symbolLen) && parser.matchWordI("="))
			{
				// matches pattern: <symbolName> = ...
				parser.skipWhitespaces();
				parser.trimWhitespaces();
				const char* expressionStr = parser.getCurrentPtr();
				sint32 expressionLen = parser.getCurrentLen();
				// create entry for symbol value assignment
				currentGroup->list_patches.emplace_back(new PatchEntryCemuhookSymbolValue(lineNumber, symbolStr, symbolLen, expressionStr, expressionLen));
				continue;
			}
			else
			{
				LogPatchesSyntaxError(lineNumber, fmt::format("Invalid syntax"));
				CancelParsingPatches();
				return;
			}
		}
	}
	if (currentGroup)
		AddPatchGroup(currentGroup);
}

bool GraphicPack2::ParseCemuBinaryPatchesInternal(MemStreamReader& patchesStream)
{
	uint32 magic = patchesStream.readBE<uint32>();
	if (!patchesStream.hasError() && magic == CEMU_PATCH_BINARY_V2_MAGIC)
	{
		const auto version = patchesStream.readBE<uint16>();
		const auto headerSize = patchesStream.readBE<uint16>();
		const auto featureFlags = patchesStream.readBE<uint32>();
		const auto payloadSize = patchesStream.readBE<uint32>();
		const auto expectedDigest = patchesStream.readDataNoCopy(SHA256_DIGEST_LENGTH);
		if (patchesStream.hasError() || version != CEMU_PATCH_BINARY_V2_VERSION ||
			headerSize != CEMU_PATCH_BINARY_V2_HEADER_SIZE || featureFlags != 0 || payloadSize == 0)
		{
			LogPatchesSyntaxError(-1, "Invalid CPB2 header or unsupported feature flags");
			CancelParsingPatches();
			return false;
		}
		auto payload = patchesStream.readDataNoCopy(payloadSize);
		if (patchesStream.hasError() || !patchesStream.isEndOfStream())
		{
			LogPatchesSyntaxError(-1, "CPB2 payload length does not match the file");
			CancelParsingPatches();
			return false;
		}
		std::array<unsigned char, SHA256_DIGEST_LENGTH> actualDigest{};
		SHA256(payload.data(), payload.size(), actualDigest.data());
		if (!std::equal(actualDigest.begin(), actualDigest.end(), expectedDigest.begin(), expectedDigest.end()))
		{
			LogPatchesSyntaxError(-1, "CPB2 payload SHA-256 mismatch");
			CancelParsingPatches();
			return false;
		}
		MemStreamReader payloadStream(payload.data(), static_cast<sint32>(payload.size()));
		return ParseCemuBinaryPatchesInternal(payloadStream);
	}
	if (patchesStream.hasError() || magic != CEMU_PATCH_BINARY_MAGIC)
	{
		LogPatchesSyntaxError(-1, "Invalid binary patch file header");
		CancelParsingPatches();
		return false;
	}

	uint32 groupCount = patchesStream.readBE<uint32>();
	if (patchesStream.hasError() || groupCount == 0 || groupCount > CEMU_PATCH_BINARY_MAX_GROUPS)
	{
		LogPatchesSyntaxError(-1, "Invalid group count in binary patch file");
		CancelParsingPatches();
		return false;
	}

	uint32 totalEntryCount{};
	uint32 totalRelocCount{};
	std::unordered_set<std::string> groupNames;
	for (uint32 groupIndex = 0; groupIndex < groupCount; groupIndex++)
	{
		std::string groupName;
		if (!_readBinaryPatchString(patchesStream, groupName, CEMU_PATCH_BINARY_MAX_NAME_SIZE) ||
			!groupNames.emplace(groupName).second)
		{
			LogPatchesSyntaxError(-1, "Invalid or duplicate group name in binary patch file");
			CancelParsingPatches();
			return false;
		}

		auto currentGroup = std::make_unique<PatchGroup>(this, groupName.data(), (sint32)groupName.size());

		uint32 moduleMatchCount = patchesStream.readBE<uint32>();
		if (patchesStream.hasError())
		{
			LogPatchesSyntaxError(-1, "Unexpected end of binary patch file while reading moduleMatches");
			CancelParsingPatches();
			return false;
		}
		if (moduleMatchCount == 0 || moduleMatchCount > CEMU_PATCH_BINARY_MAX_ENTRIES)
		{
			LogPatchesSyntaxError(-1, "Invalid moduleMatches count in binary patch group");
			CancelParsingPatches();
			return false;
		}
		std::unordered_set<uint32> moduleMatches;
		for (uint32 moduleMatchIndex = 0; moduleMatchIndex < moduleMatchCount; moduleMatchIndex++)
		{
			const auto moduleMatch = patchesStream.readBE<uint32>();
			if (patchesStream.hasError() || !moduleMatches.emplace(moduleMatch).second)
			{
				LogPatchesSyntaxError(-1, "Invalid or duplicate moduleMatch in binary patch file");
				CancelParsingPatches();
				return false;
			}
			currentGroup->list_moduleMatches.emplace_back(moduleMatch);
		}

		uint32 entryCount = patchesStream.readBE<uint32>();
		if (patchesStream.hasError() || entryCount > CEMU_PATCH_BINARY_MAX_ENTRIES - totalEntryCount)
		{
			LogPatchesSyntaxError(-1, "Invalid or excessive entry count in binary patch file");
			CancelParsingPatches();
			return false;
		}
		totalEntryCount += entryCount;
		std::unordered_set<std::string> labelNames;

		for (uint32 entryIndex = 0; entryIndex < entryCount; entryIndex++)
		{
			uint8 entryType = patchesStream.readBE<uint8>();
			if (patchesStream.hasError())
			{
				LogPatchesSyntaxError(-1, "Unexpected end of binary patch file while reading entry type");
				CancelParsingPatches();
				return false;
			}

			if (entryType == CEMU_PATCH_BINARY_ENTRY_LABEL)
			{
				uint32 labelAddress = patchesStream.readBE<uint32>();
				std::string labelName;
				if (!_readBinaryPatchString(patchesStream, labelName, CEMU_PATCH_BINARY_MAX_NAME_SIZE) ||
					!labelNames.emplace(labelName).second)
				{
					LogPatchesSyntaxError(-1, "Invalid or duplicate label entry in binary patch file");
					CancelParsingPatches();
					return false;
				}
				PatchEntryLabel* patchLabel = new PatchEntryLabel((sint32)entryIndex + 1, labelName.data(), (sint32)labelName.size());
				patchLabel->setAssignedVA(labelAddress);
				currentGroup->list_patches.emplace_back(patchLabel);
			}
			else if (entryType == CEMU_PATCH_BINARY_ENTRY_DATA)
			{
				uint32 patchAddress = patchesStream.readBE<uint32>();
				uint32 dataSize = patchesStream.readBE<uint32>();
				uint32 relocCount = patchesStream.readBE<uint32>();
				if (patchesStream.hasError())
				{
					LogPatchesSyntaxError(-1, "Invalid data entry header in binary patch file");
					CancelParsingPatches();
					return false;
				}
				if (relocCount > CEMU_PATCH_BINARY_MAX_RELOCS - totalRelocCount)
				{
					LogPatchesSyntaxError(-1, "Excessive relocation count in binary patch file");
					CancelParsingPatches();
					return false;
				}
				totalRelocCount += relocCount;
				auto patchData = patchesStream.readDataNoCopy(dataSize);
				if (patchesStream.hasError())
				{
					LogPatchesSyntaxError(-1, "Unexpected end of binary patch file while reading patch data");
					CancelParsingPatches();
					return false;
				}
				std::vector<PPCAssemblerReloc> relocs;
				for (uint32 relocIndex = 0; relocIndex < relocCount; relocIndex++)
				{
					uint8 relocTypeRaw = patchesStream.readBE<uint8>();
					uint32 byteOffset = patchesStream.readBE<uint32>();
					uint8 bitOffset = patchesStream.readBE<uint8>();
					uint8 bitCount = patchesStream.readBE<uint8>();
					std::string expression;
					if (patchesStream.hasError() || !_isValidBinaryRelocType(relocTypeRaw) ||
						!_readBinaryPatchString(patchesStream, expression, CEMU_PATCH_BINARY_MAX_EXPRESSION_SIZE))
					{
						LogPatchesSyntaxError(-1, "Invalid relocation entry in binary patch file");
						CancelParsingPatches();
						return false;
					}

					PPCASM_RELOC relocType = (PPCASM_RELOC)relocTypeRaw;
					uint32 relocSize = _getBinaryRelocSize(relocType);
					if (byteOffset > dataSize || relocSize > (dataSize - byteOffset))
					{
						LogPatchesSyntaxError(-1, "Relocation range is outside of patch data");
						CancelParsingPatches();
						return false;
					}
					if ((relocType == PPCASM_RELOC::U32_MASKED_IMM &&
						(bitCount == 0 || bitCount > 32 || bitOffset >= 32 || ((uint32)bitOffset + bitCount) > 32)) ||
						(relocType != PPCASM_RELOC::U32_MASKED_IMM && (bitOffset != 0 || bitCount != 0)))
					{
						LogPatchesSyntaxError(-1, "Invalid bit range in binary patch relocation");
						CancelParsingPatches();
						return false;
					}
					relocs.emplace_back(relocType, expression, byteOffset, bitOffset, bitCount);
				}
				currentGroup->list_patches.emplace_back(new PatchEntryInstruction((sint32)entryIndex + 1, patchAddress, patchData, relocs));
			}
			else
			{
				LogPatchesSyntaxError(-1, "Unknown entry type in binary patch file");
				CancelParsingPatches();
				return false;
			}
		}

		AddPatchGroup(currentGroup.release());
	}

	if (patchesStream.hasError() || !patchesStream.isEndOfStream())
	{
		LogPatchesSyntaxError(-1, "Trailing or malformed data in binary patch file");
		CancelParsingPatches();
		return false;
	}

	return true;
}

static inline uint32 INVALID_ORIGIN = 0xFFFFFFFF;

bool GraphicPack2::ParseCemuPatchesTxtInternal(MemStreamReader& patchesStream)
{
	sint32 lineNumber = 0;
	PatchGroup* currentGroup = nullptr;

	struct  
	{
		void reset()
		{
			currentOrigin = INVALID_ORIGIN;
			codeCaveOrigin = 0;
		}

		void setOrigin(uint32 origin)
		{
			currentOrigin = origin;
		}

		void setOriginCodeCave()
		{
			currentOrigin = codeCaveOrigin;
		}

		bool isValidOrigin()
		{
			return currentOrigin != INVALID_ORIGIN;
		}

		void incrementOrigin(uint32 size)
		{
			currentOrigin += size;
			if (currentOrigin <= 32 * 1024 * 1024)
				codeCaveOrigin = std::max(codeCaveOrigin, currentOrigin);
		}

		uint32 currentOrigin{};
		uint32 codeCaveOrigin{};
	}originInfo;
	// labels dont get emitted immediately, instead they are assigned a VA after the next alignment zone
	std::vector<PatchEntryLabel*> scheduledLabels; 
	// this is to prevent code like this from putting alignment bytes after the label. (The label 'sticks' to the data after it)
	// .byte 123
	// Label:
	// BLR

	auto flushLabels = [&]()
	{
		// flush remaining labels
		for (auto& itr : scheduledLabels)
		{
			itr->setAssignedVA(originInfo.currentOrigin);
			currentGroup->list_patches.emplace_back(itr);
		}
		scheduledLabels.clear();
	};

	while (true)
	{
		size_t lineLength;
		auto lineStr = patchesStream.readLine();
		lineNumber++;
		if (patchesStream.hasError())
			break;
		// trim comment
		lineLength = GetLengthWithoutComment(lineStr.data(), lineStr.size());

		StringTokenParser parser(lineStr.data(), (sint32)lineLength);

		// skip whitespaces at the beginning
		parser.skipWhitespaces();
		// parse line
		if (parser.isEndOfString())
			continue;
		if (parser.compareCharacter(0, '['))
		{
			// group
			parser.skipCharacters(1);
			// find end of group name
			const char* groupNameStr = parser.getCurrentPtr();
			sint32 groupNameLength = parser.skipToCharacter(']');
			if (groupNameLength < 0)
			{
				LogPatchesSyntaxError(lineNumber, "Expected ']'");
				CancelParsingPatches();
				return false;
			}
			parser.skipCharacters(1); // skip the ']'
			parser.skipWhitespaces();
			if (!parser.isEndOfString())
			{
				LogPatchesSyntaxError(lineNumber, "Unexpected characters after ']'");
				CancelParsingPatches();
				return false;
			}
			// begin new group
			if (currentGroup)
			{
				flushLabels();
				AddPatchGroup(currentGroup);
			}
			currentGroup = new PatchGroup(this, groupNameStr, groupNameLength);
			// reset origin tracking
			originInfo.reset();
			continue;
		}
		else if (parser.matchWordI("moduleMatches"))
		{
			if (currentGroup == nullptr)
			{
				LogPatchesSyntaxError(lineNumber, "Specified 'ModuleMatches' outside of a group");
				CancelParsingPatches();
				return false;
			}
			if (parser.matchWordI("=") == false)
			{
				LogPatchesSyntaxError(lineNumber, "Expected '=' after ModuleMatches");
				CancelParsingPatches();
				return false;
			}
			// read the checksums
			while (true)
			{
                if (parser.matchWordI("rpx"))
                {
                   	currentGroup->m_isRpxOnlyTarget = true;
                   	break;
                }
			
				uint32 checksum = 0;
				if (parser.parseU32(checksum) == false)
				{
					LogPatchesSyntaxError(lineNumber, "Invalid value for ModuleMatches");
					CancelParsingPatches();
					return false;
				}
				currentGroup->list_moduleMatches.emplace_back(checksum);
				if (parser.matchWordI(",") == false)
					break;
			}
			parser.skipWhitespaces();
			if (!parser.isEndOfString())
			{
				LogPatchesSyntaxError(lineNumber, "Unexpected character");
				CancelParsingPatches();
				return false;
			}
			continue;
		}

		// if a line starts with <hex_address> = then it temporarily overwrites the origin for the current line
		uint32 overwriteOrigin = INVALID_ORIGIN;
		if (parser.compareCharacter(0, '0') && parser.compareCharacterI(1, 'x'))
		{
			StringTokenParser addressParserBackup;
			parser.storeParserState(&addressParserBackup);
			uint32 patchedAddress;
			if (parser.parseU32(patchedAddress))
			{
				if (parser.matchWordI("="))
				{
					parser.skipWhitespaces();
					parser.trimWhitespaces();
					overwriteOrigin = patchedAddress;
				}
				else
				{
					parser.restoreParserState(&addressParserBackup);
				}
			}
			else
			{
				parser.restoreParserState(&addressParserBackup);
			}
		}
		// check for known directives
		if (parser.matchWordI(".origin"))
		{
			// .origin = <origin> directive
			if (overwriteOrigin != INVALID_ORIGIN)
			{
				LogPatchesSyntaxError(lineNumber, fmt::format(".origin directive must appear alone without <address> = prefix."));
				CancelParsingPatches();
				return false;
			}
			if (!parser.matchWordI("="))
			{
				LogPatchesSyntaxError(lineNumber, fmt::format("Missing '=' after .origin"));
				CancelParsingPatches();
				return false;
			}
			// parse origin
			uint32 originAddress;
			if (parser.matchWordI("codecave"))
			{
				// keyword codecave means we set the origin to the end of the current known codecave size
				originInfo.setOriginCodeCave();
			}
			else if(parser.parseU32(originAddress))
			{
				// hex address
				originInfo.setOrigin(originAddress);
			}
			else
			{
				LogPatchesSyntaxError(lineNumber, fmt::format("\'.origin =\' must be followed by the keyword codecave or a valid address"));
				CancelParsingPatches();
				return false;
			}
			continue;
		}
		else if (parser.matchWordI(".callback"))
		{
		    if (parser.matchWordI("entry"))
    		{
                const char* symbolStr;
    			sint32 symbolLen;
    		    if (parser.parseSymbolName(symbolStr, symbolLen))
    		    {
    				currentGroup->list_callbacks.push_back(std::make_pair(std::string(symbolStr, static_cast<size_t>(symbolLen)), GPCallbackType::Entry));
    				continue;
    		    }
    		    else
    		    {
                    LogPatchesSyntaxError(lineNumber, "'.callback' must reference a symbol after the type");
                    CancelParsingPatches();
                    return false;
    		    }
    		}
		    else
		    {
				LogPatchesSyntaxError(lineNumber, "Unrecognized type for '.callback'");
				CancelParsingPatches();
				return false;
			}
		}

		std::vector<uint8> rawPatchData;
		std::string rawPatchError;
		RAW_HEX_PARSE_RESULT rawPatchResult = _parseRawHexPatchData(std::string_view(parser.getCurrentPtr(), parser.getCurrentLen()), rawPatchData, rawPatchError);
		if (rawPatchResult == RAW_HEX_PARSE_RESULT::PARSE_ERROR)
		{
			LogPatchesSyntaxError(lineNumber, rawPatchError);
			CancelParsingPatches();
			return false;
		}
		if (rawPatchResult == RAW_HEX_PARSE_RESULT::MATCH)
		{
			if (currentGroup == nullptr)
			{
				LogPatchesSyntaxError(lineNumber, "Raw binary patch data specified outside of a group");
				CancelParsingPatches();
				return false;
			}
			uint32 patchAddress;
			if (overwriteOrigin != INVALID_ORIGIN)
			{
				patchAddress = overwriteOrigin;
			}
			else if (originInfo.isValidOrigin())
			{
				patchAddress = originInfo.currentOrigin;
				originInfo.incrementOrigin((uint32)rawPatchData.size());
			}
			else
			{
				LogPatchesSyntaxError(lineNumber, "Trying to emit raw binary patch data but no address specified. (Declare .origin or prefix line with <address> = )");
				CancelParsingPatches();
				return false;
			}
			for (auto& itr : scheduledLabels)
			{
				itr->setAssignedVA(patchAddress);
				currentGroup->list_patches.emplace_back(itr);
			}
			scheduledLabels.clear();
			std::vector<PPCAssemblerReloc> noRelocs;
			currentGroup->list_patches.emplace_back(new PatchEntryInstruction(lineNumber, patchAddress, { rawPatchData.data(), rawPatchData.size() }, noRelocs));
			continue;
		}

		// next we attempt to parse symbol assignment
		// symbols can be labels or variables. The type is determined by what comes after the symbol name
		// <symbolName> = <expression> defines a variable
		// <symbolName>: defines a label

		StringTokenParser bakParser;
		const char* symbolStr;
		sint32 symbolLen;
		parser.storeParserState(&bakParser);

		// check for pattern <symbolName>:
		if (parser.parseSymbolName(symbolStr, symbolLen) && parser.matchWordI(":"))
		{
			// label
			parser.skipWhitespaces();
			if (!parser.isEndOfString())
			{
				LogPatchesSyntaxError(lineNumber, fmt::format("Unexpected characters after label"));
				CancelParsingPatches();
				return false;
			}
			uint32 labelAddress;
			if (overwriteOrigin != INVALID_ORIGIN)
				labelAddress = overwriteOrigin;
			else
			{
				if (!originInfo.isValidOrigin())
				{
					LogPatchesSyntaxError(lineNumber, fmt::format("Defined label has no address assigned or there is no active .origin"));
					CancelParsingPatches();
					return false;
				}
				labelAddress = originInfo.currentOrigin;
			}
			if (overwriteOrigin == INVALID_ORIGIN)
			{
				// if label is part of code flow, delay emitting it until the next data instruction
				// this is so we can avoid generating alignment padding, whose size is unknown in advance, between labels and data instructions
				scheduledLabels.emplace_back(new PatchEntryLabel(lineNumber, symbolStr, symbolLen));
			}
			else
			{
				PatchEntryLabel* patchLabel = new PatchEntryLabel(lineNumber, symbolStr, symbolLen);
				patchLabel->setAssignedVA(labelAddress);
				currentGroup->list_patches.emplace_back(patchLabel);
			}
			continue;
		}
		parser.restoreParserState(&bakParser);
		// check for pattern <symbolName> =
		if (parser.parseSymbolName(symbolStr, symbolLen) && parser.matchWordI("="))
		{
			// variable definition
			parser.skipWhitespaces();
			parser.trimWhitespaces();
			const char* expressionStr = parser.getCurrentPtr();
			sint32 expressionLen = parser.getCurrentLen();
			// create entry for symbol/variable value assignment
			currentGroup->list_patches.emplace_back(new PatchEntryVariableValue(lineNumber, symbolStr, symbolLen, PATCHVARTYPE::UINT, expressionStr, expressionLen));
			continue;
		}
		// if all patterns mismatch then we assume it's an assembly instruction
		parser.restoreParserState(&bakParser);
		std::string instrText(parser.getCurrentPtr(), parser.getCurrentLen());
		PPCAssemblerInOut ctx{};
		ctx.forceNoAlignment = overwriteOrigin != INVALID_ORIGIN; // dont auto-align when a fixed address is assigned
		if (overwriteOrigin != INVALID_ORIGIN)
			ctx.virtualAddress = overwriteOrigin;
		else if(originInfo.isValidOrigin())
			ctx.virtualAddress = originInfo.currentOrigin;
		else
		{
			LogPatchesSyntaxError(lineNumber, fmt::format("Trying to assemble line but no address specified. (Declare .origin or prefix line with <address> = )"));
			CancelParsingPatches();
			return false;
		}
		if (!ppcAssembler_assembleSingleInstruction(instrText.c_str(), &ctx))
		{
			LogPatchesSyntaxError(lineNumber, fmt::format("Error in assembler: {}", ctx.errorMsg));
			CancelParsingPatches();
			return false;
		}
		cemu_assert_debug(ctx.alignmentRequirement != 0);
		if (overwriteOrigin == INVALID_ORIGIN)
		{
			originInfo.incrementOrigin((sint32)ctx.alignmentPaddingSize); // alignment padding	
			originInfo.incrementOrigin((sint32)ctx.outputData.size()); // instruction size
		}
		// flush labels
		for (auto& itr : scheduledLabels)
		{
			itr->setAssignedVA(ctx.virtualAddressAligned);
			currentGroup->list_patches.emplace_back(itr);
		}
		scheduledLabels.clear();
		// append instruction
		currentGroup->list_patches.emplace_back(new PatchEntryInstruction(lineNumber, ctx.virtualAddressAligned, { ctx.outputData.data(), ctx.outputData.size() }, ctx.list_relocs));
	}
	flushLabels();

	if (currentGroup)
		AddPatchGroup(currentGroup);
	return true;
}
