#include "BootSoundReader.h"
#include "Cafe/CafeSystem.h"

BootSoundReader::BootSoundReader(FSCVirtualFile* bootsndFile, sint32 blockSize) : bootsndFile(bootsndFile), blockSize(blockSize)
{
	fsc_setFileSeek(bootsndFile, 0);
	fsc_readFile(bootsndFile, &muteBits, 4);
	fsc_readFile(bootsndFile, &loopPoint, 4);
	buffer.resize(blockSize / sizeof(sint16));
	bufferBE.resize(blockSize / sizeof(sint16be));
	if(blockSize % sizeof(sint16be) != 0)
		cemu_assert_suspicious();
}

sint16* BootSoundReader::getSamples()
{
	size_t totalRead = 0;
	while(totalRead < blockSize)
	{
		auto read = fsc_readFile(bootsndFile, bufferBE.data(), blockSize - totalRead);
		if (read % sizeof(sint16be) != 0)
			cemu_assert_suspicious();

		std::copy_n(bufferBE.begin(), read / sizeof(sint16be), buffer.begin() + totalRead);
		totalRead += read;
		if (totalRead < blockSize)
			fsc_setFileSeek(bootsndFile, loopPoint * 4);
	}

	return buffer.data();
}
