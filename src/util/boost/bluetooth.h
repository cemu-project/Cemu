#pragma once

#include "platform/platform.h"

#include <boost/asio.hpp>

namespace boost
{
	namespace asio
	{
		template <typename Protocol>
		class device_endpoint
		{
		public:
			typedef Protocol protocol_type;
			typedef detail::socket_addr_type data_type;

			struct device_t
			{
				device_t(long long device_addr)
					: addr(device_addr)
				{
				}

				long long addr;
			};

			device_endpoint()
			{
				memset(&addr, 0x00, sizeof(addr));
			}

			device_endpoint(device_t device_address)
			{
				memset(&addr, 0x00, sizeof(addr));

				addr.addressFamily = AF_BLUETOOTH;
				addr.btAddr = id.addr;
				addr.serviceClassId = RFCOMM_PROTOCOL_UUID;
				addr.port = BT_PORT_ANY;
			}

			device_endpoint(const device_endpoint& other)
				: addr(other.addr)
			{
			}

			device_endpoint& operator=(const device_endpoint& other)
			{
				addr = other.addr;
				return *this;
			}

			protocol_type protocol() const
			{
				return protocol_type();
			}

			data_type* data()
			{
				return reinterpret_cast<data_type*>(&addr);
			}

			const data_type* data() const
			{
				return reinterpret_cast<const data_type*>(&addr);
			}

			size_t size() const
			{
				return sizeof(SOCKADDR_BTH);
			}

			size_t capacity() const
			{
				return size();
			}

		private:
			SOCKADDR_BTH addr;
		};

		class bluetooth
		{
		public:
			using endpoint = device_endpoint<bluetooth>;
			using socket = basic_stream_socket<bluetooth>;
			using acceptor = basic_socket_acceptor<bluetooth>;
			using iostream = basic_socket_iostream<bluetooth>;

			bluetooth() = default;

			int type() const
			{
				return SOCK_STREAM;
			}

			int protocol() const
			{
				return BTPROTO_RFCOMM;
			}

			int family() const
			{
				return AF_BLUETOOTH;
			}
		};
	}
}
