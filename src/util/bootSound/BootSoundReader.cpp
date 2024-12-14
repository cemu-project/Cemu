#include "BootSoundReader.h"
#include "Cafe/CafeSystem.h"

BootSoundReader::BootSoundReader(FSCVirtualFile* bootsndFile, sint32 blockSize) : bootsndFile(bootsndFile), blockSize(blockSize)
{
	// crash if this constructor is invoked with a blockSize that has a different number of samples per channel
	cemu_assert(blockSize % (sizeof(sint16be) * 2) == 0);

	fsc_setFileSeek(bootsndFile, 0);
	fsc_readFile(bootsndFile, &muteBits, 4);
	fsc_readFile(bootsndFile, &loopPoint, 4);

	buffer.resize(blockSize / sizeof(sint16));
	bufferBE.resize(blockSize / sizeof(sint16be));

	// workaround: SM3DW has incorrect loop point
	const auto titleId = CafeSystem::GetForegroundTitleId();
	if(titleId == 0x0005000010145D00 || titleId == 0x0005000010145C00 || titleId == 0x0005000010106100)
		loopPoint = 113074;
}

sint16* BootSoundReader::getSamples()
{
	size_t totalRead = 0;
	const size_t loopPointOffset = 8 + loopPoint * 4;
	while (totalRead < blockSize)
	{
		auto read = fsc_readFile(bootsndFile, bufferBE.data(), blockSize - totalRead);
		if (read == 0)
		{
			cemuLog_log(LogType::Force, "failed to read PCM samples from bootSound.btsnd");
			return nullptr;
		}
		if (read % (sizeof(sint16be) * 2) != 0)
		{
			cemuLog_log(LogType::Force, "failed to play bootSound.btsnd: reading PCM data stopped at an odd number of samples (is the file corrupt?)");
			return nullptr;
		}

		std::copy_n(bufferBE.begin(), read / sizeof(sint16be), buffer.begin() + (totalRead / sizeof(sint16)));
		totalRead += read;
		if (totalRead < blockSize)
			fsc_setFileSeek(bootsndFile, loopPointOffset);
	}

	// handle case where the end of a block of samples lines up with the end of the file
	if(fsc_getFileSeek(bootsndFile) == fsc_getFileSize(bootsndFile))
		fsc_setFileSeek(bootsndFile, loopPointOffset);

	return buffer.data();
}
