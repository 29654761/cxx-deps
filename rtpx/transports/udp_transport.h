/**
 * @file udp_transport.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */


#pragma once
#include "transport.h"
#include <asio_udp_socket.h>

namespace rtpx
{
	class udp_transport :public transport
	{
	public:
		udp_transport(asio::io_context& ioc,uint16_t local_port,spdlogger_ptr log);
		~udp_transport();

		virtual bool open();
		virtual void close();
		virtual bool send(const uint8_t* data, size_t size, const asio::ip::udp::endpoint& endpoint);
	private:
		void on_data_received(asio::udp_socket_ptr skt, const uint8_t* data, size_t size, const asio::ip::udp::endpoint& endpoint);
	private:
		std::mutex mutex_;
		asio::udp_socket_ptr skt_;
	};


}

