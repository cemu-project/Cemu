#include "Common/ExceptionHandler/ELFSymbolTable.h"
#include "Common/FileStream.h"
#include <sys/mman.h>
#include <fcntl.h>

uint16 ELFSymbolTable::FindSection(int type, const std::string_view& name)
{
	if (!shTable || !shStrTable)
		return 0;

	for (uint16 i = 0; i < header->e_shnum; ++i)
	{
		auto& entry = shTable[i];
		if(entry.sh_type == type && std::string_view{&shStrTable[entry.sh_name]} == name)
		{
			return i;
		}
	}
	return 0;
}

void* ELFSymbolTable::SectionPointer(uint16 index)
{
	return SectionPointer(shTable[index]);
}

void* ELFSymbolTable::SectionPointer(const Elf64_Shdr& section)
{
	return (void*)(mappedExecutable + section.sh_offset);
}

ELFSymbolTable::ELFSymbolTable()
{
	// create file handle
	int fd = open("/proc/self/exe", O_RDONLY);
	if (!fd)
		return;

	// retrieve file size.
	struct stat filestats;
	if (fstat(fd, &filestats))
	{
		close(fd);
		return;
	}
	mappedExecutableSize = filestats.st_size;

	// attempt to map the file
	mappedExecutable = (uint8*)(mmap(nullptr, mappedExecutableSize, PROT_READ, MAP_PRIVATE, fd, 0));
	close(fd);
	if (!mappedExecutable)
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

	shTable = (Elf64_Shdr*)(mappedExecutable + header->e_shoff);

	Elf64_Shdr& shStrn = shTable[header->e_shstrndx];
	shStrTable = (char*)(mappedExecutable + shStrn.sh_offset);

	strTable = (char*)SectionPointer(FindSection(SHT_STRTAB, ".strtab"));

	Elf64_Shdr& symTabShdr = shTable[FindSection(SHT_SYMTAB, ".symtab")];
	if (symTabShdr.sh_entsize == 0)
		return;

	symTableLen = symTabShdr.sh_size / symTabShdr.sh_entsize;
	symTable = (Elf64_Sym*)(SectionPointer(symTabShdr));
}

ELFSymbolTable::~ELFSymbolTable()
{
	if (mappedExecutable)
		munmap(mappedExecutable, mappedExecutableSize);
}

std::string_view ELFSymbolTable::OffsetToSymbol(uint64 ptr, uint64& fromStart) const
{
	if(!symTable || !strTable)
	{
		fromStart = -1;
		return {};
	}

	for (auto entry = symTable+1; entry < symTable+symTableLen; ++entry)
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
