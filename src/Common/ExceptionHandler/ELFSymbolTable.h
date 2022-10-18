#pragma once
#include <memory>
#include <elf.h>

class ELFSymbolTable
{
public:
	std::string_view OffsetToSymbol(uint64 ptr, uint64& fromStart) const;

	ELFSymbolTable();
	~ELFSymbolTable();
private:
	uint8* mappedExecutable = nullptr;
	size_t mappedExecutableSize = 0;

	Elf64_Ehdr* header = nullptr;

	Elf64_Shdr* shTable = nullptr;
	char* shStrTable = nullptr;

	Elf64_Sym* symTable = nullptr;
	uint64 symTableLen = 0;
	char* strTable = nullptr;

	uint16 FindSection(int type, const std::string_view& name);

	void* SectionPointer (uint16 index);
	void* SectionPointer(const Elf64_Shdr& section);

	// ownership of mapped memory, cannot copy.
	ELFSymbolTable(const ELFSymbolTable&) = delete;
};
