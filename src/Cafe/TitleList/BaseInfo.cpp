#include "BaseInfo.h"

#include "config/CemuConfig.h"
#include "Cafe/Filesystem/fsc.h"
#include "Cafe/Filesystem/FST/FST.h"

sint32 BaseInfo::GetLanguageIndex(std::string_view language)
{
	if (language == "ja")
		return (sint32)CafeConsoleLanguage::JA;
	else if (language == "en")
		return (sint32)CafeConsoleLanguage::EN;
	else if (language == "fr")
		return (sint32)CafeConsoleLanguage::FR;
	else if (language == "de")
		return (sint32)CafeConsoleLanguage::DE;
	else if (language == "it")
		return (sint32)CafeConsoleLanguage::IT;
	else if (language == "es")
		return (sint32)CafeConsoleLanguage::ES;
	else if (language == "zhs")
		return (sint32)CafeConsoleLanguage::ZH;
	else if (language == "ko")
		return (sint32)CafeConsoleLanguage::KO;
	else if (language == "nl")
		return (sint32)CafeConsoleLanguage::NL;
	else if (language == "pt")
		return (sint32)CafeConsoleLanguage::PT;
	else if (language == "ru")
		return (sint32)CafeConsoleLanguage::RU;
	else if (language == "zht")
		return (sint32)CafeConsoleLanguage::ZH;
	return -1;
}


std::unique_ptr<uint8[]> BaseInfo::ReadFSCFile(std::string_view filename, uint32& size) const
{
	size = 0;
	sint32 fscStatus = 0;
	// load and parse meta.xml
	FSCVirtualFile* file = fsc_open(const_cast<char*>(std::string(filename).c_str()), FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &fscStatus);
	if (file)
	{
		size = fsc_getFileSize(file);
		auto buffer = std::make_unique<uint8[]>(size);
		fsc_readFile(file, buffer.get(), size);
		fsc_close(file);
		return buffer;
	}

	return nullptr;
}

std::unique_ptr<uint8[]> BaseInfo::ReadVirtualFile(FSTVolume* volume, std::string_view filename, uint32& size) const
{
	size = 0;
	FSTFileHandle fileHandle;
	if (!volume->OpenFile(filename, fileHandle, true))
		return nullptr;

	size = volume->GetFileSize(fileHandle);
	auto buffer = std::make_unique<uint8[]>(size);
	volume->ReadFile(fileHandle, 0, size, buffer.get());

	return buffer;
}

