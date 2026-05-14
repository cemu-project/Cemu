#pragma once

struct Image
{
	sint32* colors = nullptr;
	int width = 0;
	int height = 0;
	int channels = 0;

	Image(Image&& image);

	Image(const std::vector<uint8>& imageBytes);

	bool IsOk() const;

	~Image();
};
