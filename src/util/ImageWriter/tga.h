#include "Common/FileStream.h"
#include <vector>

static bool tga_write_rgba(wchar_t* path, sint32 width, sint32 height, uint8* pixelData) 
{
	FileStream* fs = FileStream::createFile(path);
	if (fs == nullptr)
		return false;

	uint8_t header[18] = {0,0,2,0,0,0,0,0,0,0,0,0, (uint8)(width % 256), (uint8)(width / 256), (uint8)(height % 256), (uint8)(height / 256), 32, 0x20};
	fs->writeData(&header, sizeof(header));

	std::vector<uint8> tempPixelData;
	tempPixelData.resize(width * height * 4);

	// write one row at a time
	uint8* pOut = tempPixelData.data();
	for (sint32 y = 0; y < height; y++)
	{
		const uint8* rowIn = pixelData + y * width*4;
		for (sint32 x = 0; x < width; x++)
		{
			pOut[0] = rowIn[2];
			pOut[1] = rowIn[1];
			pOut[2] = rowIn[0];
			pOut[3] = rowIn[3];
			pOut += 4;
			rowIn += 4;
		}
	}
	fs->writeData(tempPixelData.data(), width * height * 4);
	delete fs;
	return true;
}
