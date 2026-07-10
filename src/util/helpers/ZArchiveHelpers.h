#pragma once

#include <zarchive/zarchivereader.h>

#if BOOST_PLAT_ANDROID
#include "Common/android/FilesystemAndroid.h"
#include "Common/android/FdStream.h"
#endif // BOOST_PLAT_ANDROID

namespace ZArchiveHelpers
{
	inline ZArchiveReader* OpenReader(const fs::path& path)
	{
#if BOOST_PLAT_ANDROID
		if (FilesystemAndroid::IsContentUri(path))
		{
			int fd = FilesystemAndroid::OpenContentUri(path);

			if (fd == -1)
				return nullptr;

			return ZArchiveReader::OpenFromStream(std::make_unique<FdStream>(fd));
		}
#endif // BOOST_PLAT_ANDROID

		return ZArchiveReader::OpenFromFile(path);
	}
} // namespace ZArchiveHelpers
