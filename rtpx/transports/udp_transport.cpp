/**
 * @file udp_transport.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */


#include "udp_transport.h"

#include <sys2/util.h>

namespace rtpx
{
	udp_transport::udp_transport(asio::io_context& ioc, uint16_t local_port,spdlogger_ptr log)
		:transport(ioc,local_port,log)
	{
	}

	udp_transport::~udp_transport()
	{
		close();
	}

	bool udp_transport::open()
	{
		std::lock_guard<std::mutex> lk(mutex_);
		if (skt_)
			return true;

		std::error_code ec;
		asio::ip::udp::endpoint ep(asio::ip::udp::v4(), local_port_);
		asio::ip::udp::socket udp(ioc_);
		udp.open(ep.protocol(),ec);
		if (ec)
			return false;
		udp.set_option(asio::socket_base::send_buffer_size(1024 * 1024));
		udp.set_option(asio::socket_base::receive_buffer_size(1024 * 1024));
		udp.bind(ep, ec);
		if (ec)
			return false;

		skt_ = std::make_shared<asio::udp_socket>(std::move(udp), 1000);
		auto self = std::dynamic_pointer_cast<udp_transport>(shared_from_this());
		skt_->bind_data_received_handler(std::bind(&udp_transport::on_data_received, self,
			std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
		skt_->post_read();
		return true;
	}

	void udp_transport::close()
	{
		transport::close();
		std::lock_guard<std::mutex> lk(mutex_);
		if (skt_) {
			skt_->close();
			skt_.reset();
		}
	}

	bool udp_transport::send(const uint8_t* data, size_t size, const asio::ip::udp::endpoint& endpoint)
	{
		std::lock_guard<std::mutex> lk(mutex_);
		if (!skt_)
			return false;
		return skt_->send_to(data, size, endpoint);
	}

	void udp_transport::on_data_received(asio::udp_socket_ptr skt, const uint8_t* data, size_t size, const asio::ip::udp::endpoint& endpoint)
	{
		this->received_message(data, size, endpoint);
	}
}

