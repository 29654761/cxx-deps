/**
 * @file custom_transport.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */


#pragma once
#include "transport.h"

namespace rtpx
{
	class custom_transport :public transport
	{
	public:

		typedef std::function<bool(const uint8_t*, size_t, const asio::ip::udp::endpoint&)> send_hander;

		custom_transport(asio::io_context& ioc,uint16_t local_port, send_hander sender,spdlogger_ptr log);
		~custom_transport();

		virtual bool open();
		virtual void close();
		virtual bool send(const uint8_t* data, size_t size, const asio::ip::udp::endpoint& endpoint);

	private:

		send_hander sender_;
	};


}

