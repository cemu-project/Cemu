#include "BootSoundReader.h"
#include "Cafe/CafeSystem.h"

BootSoundReader::BootSoundReader(FSCVirtualFile* bootsndFile, sint32 blockSize) : bootsndFile(bootsndFile), blockSize(blockSize)
{
	fsc_setFileSeek(bootsndFile, 0);
	fsc_readFile(bootsndFile, &muteBits, 4);
	fsc_readFile(bootsndFile, &loopPoint, 4);
	buffer.resize(blockSize / sizeof(sint16));
	bufferBE.resize(blockSize / sizeof(sint16be));
	if(blockSize % (sizeof(sint16be) * 2) != 0)
		cemu_assert_suspicious();

	// workaround: SM3DW has incorrect loop point
	const auto titleId = CafeSystem::GetForegroundTitleId();
	if(titleId == 0x0005000010145D00 || titleId == 0x0005000010145C00 || titleId == 0x0005000010106100)
		loopPoint = 113074;
}

sint16* BootSoundReader::getSamples()
{
	size_t totalRead = 0;
	while(totalRead < blockSize)
	{
		auto read = fsc_readFile(bootsndFile, bufferBE.data(), blockSize - totalRead);
		if (read % (sizeof(sint16be) * 2) != 0)
			cemu_assert_suspicious();

		std::copy_n(bufferBE.begin(), read / sizeof(sint16be), buffer.begin() + (totalRead / sizeof(sint16)));
		totalRead += read;
		if (totalRead < blockSize)
			fsc_setFileSeek(bootsndFile, 8 + loopPoint * 4);
	}

	return buffer.data();
}
