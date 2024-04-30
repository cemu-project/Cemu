#include <zlib.h>
namespace zlibhelper
{
	std::optional<std::vector<uint8>> decompress(const std::vector<uint8>& compressed)
	{
		int err;
		std::vector<uint8> decompressed;
		std::array<uint8, 64*1024> chunk;
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
			stream.avail_out = chunk.size();
			stream.next_out = chunk.data();
			err = inflate(&stream, Z_NO_FLUSH);
			if(!(err == Z_OK || err == Z_STREAM_END))
			{
				inflateEnd(&stream);
				return {};
			}
			decompressed.insert(decompressed.end(), chunk.begin(), chunk.begin() + chunk.size() - stream.avail_out);
		} while(err != Z_STREAM_END);

		inflateEnd(&stream);

		return decompressed;
	}
}