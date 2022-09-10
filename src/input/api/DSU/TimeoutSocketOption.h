#pragma once

#if BOOST_OS_WINDOWS
#include <boost/asio/detail/socket_option.hpp>
#include <winsock2.h> // For SOL_SOCKET, SO_RCVTIMEO
using TimeoutSocketOption = boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_RCVTIMEO>;
#elif BOOST_OS_LINUX || BOOST_OS_MACOS

// For making RCVTIME0 work on Linux and probably Mac too
class TimeoutSocketOption {
    timeval m_data;
public:
    constexpr explicit TimeoutSocketOption(int time_ms)
    : m_data(timeval{.tv_usec = time_ms * 1000}){
    }
    TimeoutSocketOption& operator=(int time_ms){
        m_data = timeval{.tv_usec = time_ms * 1000};
        return *this;
    }
    template <typename Protocol>
    int level(const Protocol&) const {
        return SOL_SOCKET;
    }

    template <typename Protocol>
    int name(const Protocol&) const {
        return SO_RCVTIMEO;
    }

    template <typename Protocol>
    const timeval* data(Protocol&) const {
        return &m_data;
    }

    template <typename Protocol>
    std::size_t size(const Protocol&) const {
        return sizeof(timeval);
    }
};
#endif