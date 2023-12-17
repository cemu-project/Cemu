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
	auto read = fsc_readFile(bootsndFile, bufferBE.data(), blockSize);
	if(read % sizeof(sint16be) != 0)
		cemu_assert_suspicious();

	auto readValues = read / sizeof(sint16be);
	if (readValues < blockSize)
		std::fill(buffer.begin() + readValues, buffer.end(), 0);

	std::copy_n(bufferBE.begin(), read / sizeof(sint16be), buffer.begin());
	return buffer.data();
}
