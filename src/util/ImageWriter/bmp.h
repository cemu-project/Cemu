#include "Common/FileStream.h"

static void _bmp_write(FileStream* fs, sint32 width, sint32 height, uint32 bits, void* pixelData)
{
	BITMAPFILEHEADER bmp_fh;
	BITMAPINFOHEADER bmp_ih;

	bmp_fh.bfType = 0x4d42;
	bmp_fh.bfSize = 0;
	bmp_fh.bfReserved1 = 0;
	bmp_fh.bfReserved2 = 0;
	bmp_fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	bmp_ih.biSize = sizeof(bmp_ih);
	bmp_ih.biWidth = width;
	bmp_ih.biHeight = height;
	bmp_ih.biPlanes = 1;
	bmp_ih.biBitCount = bits;
	bmp_ih.biCompression = 0;
	bmp_ih.biSizeImage = 0;
	bmp_ih.biXPelsPerMeter = 0;
	bmp_ih.biYPelsPerMeter = 0;
	bmp_ih.biClrUsed = 0;
	bmp_ih.biClrImportant = 0;

	sint32 rowPitch = (width * bits / 8);
	rowPitch = (rowPitch + 3)&~3;

	uint8 padding[4] = { 0 };
	sint32 paddingLength = rowPitch - (width * bits / 8);

	bmp_ih.biSize = sizeof(BITMAPINFOHEADER);
	bmp_fh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + rowPitch * height;

	fs->writeData(&bmp_fh, sizeof(BITMAPFILEHEADER));
	fs->writeData(&bmp_ih, sizeof(BITMAPINFOHEADER));

	if (bits == 24 || bits == 32)
	{
		for (sint32 y = 0; y < height; y++)
		{
			void* rowInput = ((uint8*)pixelData) + rowPitch * (height - y - 1);
			fs->writeData(rowInput, width*bits/8);
			// write padding
			if(paddingLength > 0)
				fs->writeData(padding, paddingLength);
		}
	}	
}

static bool bmp_store8BitAs24(wchar_t* path, sint32 width, sint32 height, sint32 bytesPerRow, void* pixelData)
{
	FileStream* fs = FileStream::createFile(path);
	if (fs == nullptr)
		return false;

	uint8* pixelI = (uint8*)pixelData;
	uint8* pixelRGB = (uint8*)malloc(width * height * 3);
	for (sint32 y = 0; y < height; y++)
	{
		sint32 srcIdx = y * bytesPerRow;
		for (sint32 x = 0; x < width; x++)
		{
			sint32 dstIdx = x + y * width;
			pixelRGB[dstIdx * 3 + 0] = pixelI[srcIdx];
			pixelRGB[dstIdx * 3 + 1] = pixelI[srcIdx];
			pixelRGB[dstIdx * 3 + 2] = pixelI[srcIdx];
			srcIdx++;
		}
	}
	_bmp_write(fs, width, height, 24, pixelRGB);
	free(pixelRGB);
	delete fs;
	return true;
}

static bool bmp_store16BitAs24(wchar_t* path, sint32 width, sint32 height, sint32 bytesPerRow, void* pixelData)
{
	FileStream* fs = FileStream::createFile(path);
	if (fs == nullptr)
		return false;

	uint8* pixelI = (uint8*)pixelData;
	uint8* pixelRGB = (uint8*)malloc(width * height * 3);
	for (sint32 y = 0; y < height; y++)
	{
		sint32 srcIdx = y * bytesPerRow;
		for (sint32 x = 0; x < width; x++)
		{
			sint32 dstIdx = x + y * width;
			pixelRGB[dstIdx * 3 + 0] = pixelI[srcIdx + 0];
			pixelRGB[dstIdx * 3 + 1] = pixelI[srcIdx + 1];
			pixelRGB[dstIdx * 3 + 2] = 0;
			srcIdx += 2;
		}
	}
	_bmp_write(fs, width, height, 24, pixelRGB);
	free(pixelRGB);
	delete fs;
	return true;
}

static bool bmp_store24BitAs24(wchar_t* path, sint32 width, sint32 height, sint32 bytesPerRow, void* pixelData)
{
	FileStream* fs = FileStream::createFile(path);
	if (fs == nullptr)
		return false;

	uint8* pixelI = (uint8*)pixelData;
	uint8* pixelRGB = (uint8*)malloc(width * height * 3);
	for (sint32 y = 0; y < height; y++)
	{
		sint32 srcIdx = y * bytesPerRow;
		for (sint32 x = 0; x < width; x++)
		{
			sint32 dstIdx = x + y * width;
			pixelRGB[dstIdx * 3 + 0] = pixelI[srcIdx + 0];
			pixelRGB[dstIdx * 3 + 1] = pixelI[srcIdx + 1];
			pixelRGB[dstIdx * 3 + 2] = pixelI[srcIdx + 2];
			srcIdx += 3;
		}
	}
	_bmp_write(fs, width, height, 24, pixelRGB);
	free(pixelRGB);
	delete fs;
	return true;
}