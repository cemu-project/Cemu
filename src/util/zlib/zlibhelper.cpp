#include <zlib.h>
namespace zlibhelper
{
	std::optional<std::vector<uint8>> decompress(const std::vector<uint8>& compressed)
	{
		int err;
		std::vector<uint8> decompressed;
		size_t outWritten = 0;
		z_stream stream;
		stream.zalloc = Z_NULL;
		stream.zfree = Z_NULL;
		stream.opaque = Z_NULL;
		stream.avail_in = compressed.size();
		stream.next_in = (Bytef*)compressed.data();
		err = inflateInit2(&stream, 32); // 32 is a zlib magic value to enable header detection
		if (err != Z_OK)
			return {};

		do
		{
			decompressed.resize(decompressed.size() + 1024);
			const auto availBefore = decompressed.size() - outWritten;
			stream.avail_out = availBefore;
			stream.next_out = decompressed.data() + outWritten;
			err = inflate(&stream, Z_NO_FLUSH);
			if (!(err == Z_OK || err == Z_STREAM_END))
			{
				inflateEnd(&stream);
				return {};
			}
			outWritten += availBefore - stream.avail_out;
		}
		while (err != Z_STREAM_END);

		inflateEnd(&stream);
		decompressed.resize(stream.total_out);

		return decompressed;
	}
} // namespace zlibhelper
