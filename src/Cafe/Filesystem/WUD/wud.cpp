#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "wud.h"
#include "Common/FileStream.h"

wud_t* wud_open(const fs::path& path)
{
	FileStream* fs = FileStream::openFile2(path);
	if( !fs )
		return nullptr;
	// allocate wud struct
	wud_t* wud = (wud_t*)malloc(sizeof(wud_t));
	memset(wud, 0x00, sizeof(wud_t));
	wud->fs = fs;
	// get size of file
	long long inputFileSize = wud->fs->GetSize();
	// determine whether the WUD is compressed or not
	wuxHeader_t wuxHeader = {0};
	if( wud->fs->readData(&wuxHeader, sizeof(wuxHeader_t)) != sizeof(wuxHeader_t))
	{
		// file is too short to be either
		wud_close(wud);
		return nullptr;
	}
	if( wuxHeader.magic0 == WUX_MAGIC_0 && wuxHeader.magic1 == WUX_MAGIC_1 )
	{
		// this is a WUX file
		wud->isCompressed = true;
		wud->sectorSize = wuxHeader.sectorSize;
		wud->uncompressedSize = wuxHeader.uncompressedSize;
		// validate header values
		if( wud->sectorSize < 0x100 || wud->sectorSize >= 0x10000000 )
		{
			wud_close(wud);
			return nullptr;
		}
		// calculate offsets and sizes
		wud->indexTableEntryCount = (unsigned int)((wud->uncompressedSize+(long long)(wud->sectorSize-1)) / (long long)wud->sectorSize);
		wud->offsetIndexTable = sizeof(wuxHeader_t);
		wud->offsetSectorArray = (wud->offsetIndexTable + (long long)wud->indexTableEntryCount*sizeof(unsigned int));
		// align to SECTOR_SIZE
		wud->offsetSectorArray = (wud->offsetSectorArray + (long long)(wud->sectorSize-1));
		wud->offsetSectorArray = wud->offsetSectorArray - (wud->offsetSectorArray%(long long)wud->sectorSize);
		// read index table
		unsigned int indexTableSize = sizeof(unsigned int) * wud->indexTableEntryCount;
		wud->indexTable = (unsigned int*)malloc(indexTableSize);
		wud->fs->SetPosition(wud->offsetIndexTable);
		if( wud->fs->readData(wud->indexTable, indexTableSize) != indexTableSize )
		{
			// could not read index table
			wud_close(wud);
			return nullptr;
		}
	}
	else
	{
		// uncompressed file
		wud->uncompressedSize = inputFileSize;
	}
	return wud;
}

void wud_close(wud_t* wud)
{
	delete wud->fs;
	if( wud->indexTable )
		free(wud->indexTable);
	free(wud);
}

bool wud_isWUXCompressed(wud_t* wud)
{
	return wud->isCompressed;
}

/*
 * Read data from WUD file
 * Can read up to 4GB at once
 */
unsigned int wud_readData(wud_t* wud, void* buffer, unsigned int length, long long offset)
{
	// make sure there is no out-of-bounds read
	long long fileBytesLeft = wud->uncompressedSize - offset;
	if( fileBytesLeft <= 0 )
		return 0;
	if( fileBytesLeft < (long long)length )
		length = (unsigned int)fileBytesLeft;
	// read data
	unsigned int readBytes = 0;
	if( wud->isCompressed == false )
	{
		// uncompressed read is straight forward
		wud->fs->SetPosition(offset);
		readBytes = (unsigned int)wud->fs->readData(buffer, length);
	}
	else
	{
		// compressed read must be handled on a per-sector level
		while( length > 0 )
		{
			unsigned int sectorOffset = (unsigned int)(offset % (long long)wud->sectorSize);
			unsigned int remainingSectorBytes = wud->sectorSize - sectorOffset;
			unsigned int sectorIndex = (unsigned int)(offset / (long long)wud->sectorSize);
			unsigned int bytesToRead = (remainingSectorBytes<length)?remainingSectorBytes:length; // read only up to the end of the current sector
			// look up real sector index
			sectorIndex = wud->indexTable[sectorIndex];
			wud->fs->SetPosition(wud->offsetSectorArray + (long long)sectorIndex * (long long)wud->sectorSize + (long long)sectorOffset);
			readBytes += (unsigned int)wud->fs->readData(buffer, bytesToRead);
			// progress read offset, write pointer and decrease length
			buffer = (void*)((char*)buffer + bytesToRead);
			length -= bytesToRead;
			offset += bytesToRead;

		}
	}
	return readBytes;
}

/*
 * Returns the size of the data
 * For .wud: Size of file
 * For .wux: Size of uncompressed data
 */
long long wud_getWUDSize(wud_t* wud)
{
	return wud->uncompressedSize;
}