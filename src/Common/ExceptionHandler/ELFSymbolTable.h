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

	std::span<Elf64_Shdr> shTable{};
	std::span<char> shStrTable{};

	std::span<Elf64_Sym> symTable{};
	std::span<char> strTable{};

	Elf64_Shdr* FindSection(Elf64_Word type, const std::string_view& name);

	template <typename T>
	std::span<T> SectionAsArray(const Elf64_Shdr* section);

	// ownership of mapped memory, cannot copy.
	ELFSymbolTable(const ELFSymbolTable&) = delete;
};
