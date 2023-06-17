#pragma once

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream_buffer.hpp>

#include "Common/unix/FilesystemAndroid.h"

class ContentUriIStream : public std::istream
{
   public:
    ContentUriIStream(const std::filesystem::path& path)
        : m_fd(FilesystemAndroid::openContentUri(path)),
          m_fileDescriptorStreamBuffer(m_fd, boost::iostreams::close_handle),
          std::istream(&m_fileDescriptorStreamBuffer) {}

    virtual ~ContentUriIStream() = default;

    inline bool isOpen() const
    {
        return m_fd != -1;
    }

   private:
    int m_fd;
    boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source> m_fileDescriptorStreamBuffer;
};
