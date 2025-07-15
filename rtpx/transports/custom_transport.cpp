/**
 * @file custom_transport.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */


#include "custom_transport.h"

#include <sys2/util.h>

namespace rtpx
{
	custom_transport::custom_transport(asio::io_context& ioc, uint16_t local_port, send_hander sender,spdlogger_ptr log)
		:transport(ioc, local_port,log)
	{
		sender_ = sender;
	}

	custom_transport::~custom_transport()
	{
		close();
	}

	bool custom_transport::open()
	{
		return true;
	}

	void custom_transport::close()
	{
		transport::close();
	}

	bool custom_transport::send(const uint8_t* data, size_t size, const asio::ip::udp::endpoint& endpoint)
	{
		return sender_(data, size, endpoint);
	}
}

