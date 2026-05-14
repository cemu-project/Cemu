#pragma once

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream_buffer.hpp>

class FdStream : private boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source>, public std::istream
{
  public:
	explicit FdStream(int fd)
		: boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source>(fd, boost::iostreams::close_handle), std::istream(this) {}

	bool IsValid()
	{
		return component()->is_open();
	}
};
