#pragma once
#include <memory>
#include <elf.h>

class ELFSymbolTable
{
public:
	ptrdiff_t SymbolToOffset(const std::string_view& name) const;
	std::string_view OffsetToSymbol(uint64 ptr) const;
	uint64 ProcPtrToOffset(void* ptr) const;
	std::string_view ProcPtrToSymbol(void* ptr) const;

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

	std::ptrdiff_t textOffset = 0;

	uint16 FindSection(int type, const std::string_view& name);

	template <typename T>
	T* SectionPointer(const Elf64_Shdr& section);

	template <typename T>
	T* SectionPointer (uint16 index);

	// ownership of mapped memory, cannot copy.
	ELFSymbolTable(const ELFSymbolTable&) = delete;
};