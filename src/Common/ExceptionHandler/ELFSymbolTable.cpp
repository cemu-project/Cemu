#include "Common/ExceptionHandler/ELFSymbolTable.h"
#include "Common/FileStream.h"
#include <sys/mman.h>
#include <fcntl.h>

uint16 ELFSymbolTable::FindSection(int type, const std::string_view& name)
{

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

template <typename T>
T* ELFSymbolTable::SectionPointer(uint16 index)
{
	return SectionPointer<T>(shTable[index]);
}

template <typename T>
T* ELFSymbolTable::SectionPointer(const Elf64_Shdr& section)
{
	return (T*)(mappedExecutable + section.sh_offset);
}

extern "C"
std::ptrdiff_t FindTextOffset(const ELFSymbolTable& table)
{
	return table.SymbolToOffset("FindTextOffset") - (std::ptrdiff_t) FindTextOffset;
}

ELFSymbolTable::ELFSymbolTable()
{
	// create file handle
	int err;
	int fd = open("/proc/self/exe", O_RDONLY);
	if (!fd)
		return;

	// retrieve file size.
	struct stat filestats;
	if ((err = fstat(fd, &filestats)))
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

	strTable = SectionPointer<char>(FindSection(SHT_STRTAB, ".strtab"));

	Elf64_Shdr& symTabShdr = shTable[FindSection(SHT_SYMTAB, ".symtab")];
	symTableLen = symTabShdr.sh_size / symTabShdr.sh_entsize;
	symTable = SectionPointer<Elf64_Sym>(symTabShdr);

	textOffset = FindTextOffset(*this);
}

ELFSymbolTable::~ELFSymbolTable()
{
	if (mappedExecutable)
		munmap(mappedExecutable, mappedExecutableSize);
}

std::ptrdiff_t ELFSymbolTable::SymbolToOffset(const std::string_view& name) const
{
	for (auto entry = symTable+1; entry < symTable+symTableLen; ++entry)
	{
		if (ELF64_ST_TYPE(entry->st_info) != STT_FUNC)
			continue;
		if (&strTable[entry->st_name] == name)
			return entry->st_value;
	}
	return 0;
}

std::string_view ELFSymbolTable::OffsetToSymbol(uint64 ptr) const
{
	for (auto entry = symTable+1; entry < symTable+symTableLen; ++entry)
	{
		if (ELF64_ST_TYPE(entry->st_info) != STT_FUNC)
			continue;
		auto begin = entry->st_value;
		auto size = entry->st_size;
		if(ptr >= begin && ptr < begin+size)
		{
			return &strTable[entry->st_name];
		}
	}

	return {};
}

uint64 ELFSymbolTable::ProcPtrToOffset(void* ptr) const
{
	return (uint64)((uint64)ptr + textOffset);
}


std::string_view ELFSymbolTable::ProcPtrToSymbol(void* ptr) const
{
	return OffsetToSymbol(ProcPtrToOffset(ptr));
}
