#pragma once

class PatchGroup;

#include "GraphicPackError.h"

struct PatchContext_t
{
	struct UnresolvedSymbol
	{
		sint32 lineNumber;
		PatchGroup* patchGroup;
		std::string symbolName;

		UnresolvedSymbol(sint32 _lineNumber, PatchGroup* _patchGroup, std::string _symbolName) : lineNumber(_lineNumber), patchGroup(_patchGroup), symbolName(_symbolName) {};

		bool operator < (const UnresolvedSymbol &other) const 
		{ 
			if (lineNumber == other.lineNumber)
				return symbolName.compare(other.symbolName);
			return lineNumber < other.lineNumber; 
		}
	};

	class GraphicPack2* graphicPack;
	//MEMPTR<void> codeCaveStart;
	//MEMPTR<void> codeCaveEnd;
	const RPLModule* matchedModule;
	std::unordered_map<std::string, uint32> map_values;
	// error information
	//std::unordered_set<std::string> unresolvedSymbols;
	std::set<UnresolvedSymbol> unresolvedSymbols;
	//std::unordered_multiset<sint32, std::greater<std::string>> unresolvedSymbols;
	// error handler
	PatchErrorHandler errorHandler{};
};

enum class PATCH_RESOLVE_RESULT
{
	RESOLVED, // successfully resolved any expressions or relocations
	EXPRESSION_ERROR, // syntax error in expression (usually this should be detected during the parsing stage already)
	VALUE_ERROR, // expression evaluated but the result is not useable (e.g. branch target out of range)
	UNKNOWN_VARIABLE, // variable not known or referencing unresolved variable (try again)
	VARIABLE_CONFLICT, // a variable or label with the same name was already defined
	INVALID_ADDRESS, // attempted to relocate address that is not within any known section
	UNDEFINED_ERROR, // unexpected error
};

enum class EXPRESSION_RESOLVE_RESULT
{
	AVAILABLE,
	EXPRESSION_ERROR,
	UNKNOWN_VARIABLE
};

class PatchEntry
{
public:
	PatchEntry() {};
	virtual ~PatchEntry() {};

	// apply relocation or evaluate any expressions for this entry
	virtual PATCH_RESOLVE_RESULT resolve(PatchContext_t& ctx) = 0;
};

// represents symbol assignment (always treated like an address)
// <symbolName> = <expression>
class PatchEntryCemuhookSymbolValue : public PatchEntry
{
public:
	PatchEntryCemuhookSymbolValue(sint32 lineNumber, const char* symbolName, const sint32 symbolNameLen, const char* expressionStr, const sint32 expressionLen) : PatchEntry(), m_lineNumber(lineNumber)
	{
		m_symbolName.assign(symbolName, symbolNameLen);
		m_expressionString.assign(expressionStr, expressionLen);
	}

	sint32 getLineNumber() { return m_lineNumber; }

	PATCH_RESOLVE_RESULT resolve(PatchContext_t& ctx) override;

	std::string& getSymbolName() { return m_symbolName; }

private:
	sint32 m_lineNumber;
	std::string m_symbolName;
	std::string m_expressionString;
	uint32 m_resolvedValue;
	bool m_isResolved{};
};

enum class PATCHVARTYPE
{
	DOUBLE,
	INT, // 32bit signed integer
	UINT, // 32bit unsigned integer or pointer
	//BOOL, // boolean
};

// represents variable value assignment
// unlike Cemu symbols these are treated as a
// <symbolName> = <expression>
class PatchEntryVariableValue : public PatchEntry
{
public:

	PatchEntryVariableValue(sint32 lineNumber, const char* symbolName, const sint32 symbolNameLen, PATCHVARTYPE varType, const char* expressionStr, const sint32 expressionLen) : PatchEntry(), m_lineNumber(lineNumber), m_varType(varType)
	{
		m_symbolName.assign(symbolName, symbolNameLen);
		m_expressionString.assign(expressionStr, expressionLen);
	}

	sint32 getLineNumber() { return m_lineNumber; }

	PATCH_RESOLVE_RESULT resolve(PatchContext_t& ctx) override;

	std::string& getSymbolName() { return m_symbolName; }
	//uint32 getSymbolValue() { return m_resolvedValue; }

private:
	sint32 m_lineNumber;
	std::string m_symbolName;
	std::string m_expressionString;
	PATCHVARTYPE m_varType;
	std::variant<sint32, uint32, double> m_resolvedValue;
	bool m_isResolved{};
};

// represents a label
class PatchEntryLabel : public PatchEntry
{
public:
	PatchEntryLabel(sint32 lineNumber, const char* symbolName, const sint32 symbolNameLen) : PatchEntry(), m_lineNumber(lineNumber)
	{
		m_symbolName.assign(symbolName, symbolNameLen);
	}

	sint32 getLineNumber() { return m_lineNumber; }

	PATCH_RESOLVE_RESULT resolve(PatchContext_t& ctx) override;

	std::string& getSymbolName() { return m_symbolName; }
	uint32 getSymbolValue() { return m_relocatedAddress; }

	void setAssignedVA(uint32 virtualAddress)
	{
		m_address = virtualAddress;
	}

private:
	sint32 m_lineNumber;
	std::string m_symbolName;
	uint32 m_address;
	uint32 m_relocatedAddress;
	bool m_isResolved{};
};

// represents assembled code/data
class PatchEntryInstruction : public PatchEntry
{
public:
	PatchEntryInstruction(sint32 lineNumber, uint32 patchAddr, std::span<uint8> data, std::vector<PPCAssemblerReloc>& list_relocs) : PatchEntry(), m_lineNumber(lineNumber), m_addr(patchAddr), m_size((uint32)data.size()), m_relocs(list_relocs)
	{
		sint32 dataLength = (sint32)data.size();
		m_length = dataLength;
		m_data = new uint8[dataLength];
		m_dataWithRelocs = new uint8[dataLength];
		m_dataBackup = new uint8[dataLength];
		memcpy(m_data, data.data(), dataLength);
		memcpy(m_dataWithRelocs, data.data(), dataLength);
	}

	~PatchEntryInstruction()
	{
		if (m_data)
			delete[] m_data;
		if (m_dataWithRelocs)
			delete[] m_dataWithRelocs;
		if (m_dataBackup)
			delete[] m_dataBackup;
	}

	uint32 getAddr() const
	{
		return m_addr;
	}

	uint32 getRelocatedAddr()
	{
		return m_relocatedAddr;
	}
	uint32 getSize() const
	{
		return m_size;
	}

	PATCH_RESOLVE_RESULT resolve(PatchContext_t& ctx) override;
	PATCH_RESOLVE_RESULT resolveReloc(PatchContext_t& ctx, PPCAssemblerReloc* reloc);

	void applyPatch();
	void undoPatch();

private:
	uint8* m_data{}; // original unrelocated data
	uint8* m_dataWithRelocs{}; // data with relocs applied
	uint8* m_dataBackup{}; // original data before patch was applied
	sint32 m_length{};
	std::vector<PPCAssemblerReloc> m_relocs;
	uint32 m_lineNumber;
	uint32 m_addr;
	uint32 m_size;
	uint32 m_relocatedAddr;
	bool m_addrRelocated{};
};

class PatchGroup
{
	friend class GraphicPack2;

public:
	PatchGroup(class GraphicPack2* gp, const char* nameStr, sint32 nameLength) : graphicPack(gp)
	{
		name = std::string(nameStr, nameLength);
	}

	bool matchesCRC(uint32 crc)
	{
		for (auto& chk : list_moduleMatches)
		{
			if (chk == crc)
				return true;
		}
		return false;
	}

	uint32 getCodeCaveBase()
	{
		return codeCaveMem.GetMPTR();
	}

	uint32 getCodeCaveSize()
	{
		return codeCaveSize;
	}

	std::string_view getName()
	{
		return name;
	}

	void setApplied() { m_isApplied = true; }
	void resetApplied() { m_isApplied = false; }
	bool isApplied() const { return m_isApplied; }

private:
	class GraphicPack2* graphicPack;
	std::string name;
	std::vector<uint32> list_moduleMatches;
	std::vector<PatchEntry*> list_patches;
	uint32 codeCaveSize;
	MEMPTR<void> codeCaveMem;
	bool m_isApplied{};
};