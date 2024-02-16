#include "Cafe/GraphicPack/GraphicPack2.h"
#include "Common/FileStream.h"
#include "util/helpers/StringParser.h"
#include "Cemu/PPCAssembler/ppcAssembler.h"
#include "Cafe/OS/RPL/rpl_structs.h"

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

void GraphicPack2::CancelParsingPatches()
{
	// unload everything, set error flag
	cemu_assert_debug(false);
}

void GraphicPack2::AddPatchGroup(PatchGroup* group)
{
	if (group->list_moduleMatches.empty())
	{
		LogPatchesSyntaxError(-1, fmt::format("Group \"{}\" has no moduleMatches definition", group->name));
		CancelParsingPatches();
		delete group;
		return;
	}
	// calculate code cave size
	uint32 codeCaveMaxAddr = 0;
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
			}
		}

	}
	uint32 numEstimatedCodeCaveInstr = codeCaveMaxAddr / 4;
	if (group->list_patches.size() < (numEstimatedCodeCaveInstr / 8))
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
			// assemble instruction
			std::string instrText(parser.getCurrentPtr(), parser.getCurrentLen());
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
			uint32 patchedAddress;
			if (!parser.parseU32(patchedAddress))
			{
				LogPatchesSyntaxError(lineNumber, "Malformed address");
				CancelParsingPatches();
				return false;
			}
			if (parser.matchWordI("=") == false)
			{
				LogPatchesSyntaxError(lineNumber, "Expected '=' after address");
				CancelParsingPatches();
				return false;
			}
			parser.skipWhitespaces();
			parser.trimWhitespaces();
			overwriteOrigin = patchedAddress;
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
