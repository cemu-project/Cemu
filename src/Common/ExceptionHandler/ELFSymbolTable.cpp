#include "Common/ExceptionHandler/ELFSymbolTable.h"
#include "Common/FileStream.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

template <typename T>
std::span<T> ELFSymbolTable::SectionAsArray(const Elf64_Shdr* section)
{
	if (section == nullptr)
		return {};
	if (section->sh_size < sizeof(T))
		return {};
	if (section->sh_entsize != 0 && section->sh_entsize != sizeof(T))
		return {};

	return {(T*)(mappedExecutable + section->sh_offset), section->sh_size / sizeof(T)};
}


ELFSymbolTable::ELFSymbolTable()
{
	// create file handle
	int fd = open("/proc/self/exe", O_RDONLY);
	if (fd < 0)
		return;

	// retrieve file size.
	struct stat filestats;
	if (fstat(fd, &filestats) != 0)
	{
		close(fd);
		return;
	}
	mappedExecutableSize = filestats.st_size;

	// attempt to map the file
	mappedExecutable = static_cast<uint8*>(mmap(nullptr, mappedExecutableSize, PROT_READ, MAP_PRIVATE, fd, 0));
	close(fd);
	if (mappedExecutable == MAP_FAILED)
		return;

	// verify signature
	header = (Elf64_Ehdr*)(mappedExecutable);
	constexpr uint8 signature[] = {0x7f, 0x45, 0x4c, 0x46};
	for (size_t i = 0; i < 4; ++i)
	{
		if (signature[i] != header->e_ident[i])
		{
			return;
		}
	}

	shTable = {(Elf64_Shdr*)(mappedExecutable + header->e_shoff), header->e_shnum};

	shStrTable = SectionAsArray<char>(&shTable[header->e_shstrndx]);
	strTable = SectionAsArray<char>(FindSection(SHT_STRTAB, ".strtab"));
	symTable = SectionAsArray<Elf64_Sym>(FindSection(SHT_SYMTAB, ".symtab"));
}

ELFSymbolTable::~ELFSymbolTable()
{
	if (mappedExecutable)
		munmap(mappedExecutable, mappedExecutableSize);
}

Elf64_Shdr* ELFSymbolTable::FindSection(Elf64_Word type, const std::string_view& name)
{
	if (shTable.empty() || shStrTable.empty())
		return nullptr;

	for (auto& entry : shTable)
	{
		if(entry.sh_type == type && std::string_view{&shStrTable[entry.sh_name]} == name)
		{
			return &entry;
		}
	}
	return nullptr;
}

std::string_view ELFSymbolTable::OffsetToSymbol(uint64 ptr, uint64& fromStart) const
{
	if(symTable.empty() || strTable.empty())
	{
		fromStart = -1;
		return {};
	}

	for (auto entry = symTable.begin()+1; entry != symTable.end(); ++entry)
	{
		if (ELF64_ST_TYPE(entry->st_info) != STT_FUNC)
			continue;
		auto begin = entry->st_value;
		auto size = entry->st_size;
		if(ptr >= begin && ptr < begin+size)
		{
			fromStart = ptr-begin;
			return &strTable[entry->st_name];
		}
	}
	fromStart = -1;
	return {};
}
