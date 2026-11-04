/**
 * @file udp_transport.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */


#include "udp_transport.h"

#include <sys2/util.h>
#ifdef _WIN32
#include <mstcpip.h>
#endif

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

#ifdef _WIN32
		// Windows 默认开启 SIO_UDP_CONNRESET：对端回 ICMP Port Unreachable（如网关拒绝
		// STUN/RTCP 探测）会让下一次 recvFrom 报 WSAECONNRESET 且丢弃已到达的报文，
		// 媒体流随之"静默断流"。显式关闭（libuv/libre 同款做法）
		{
			BOOL conn_reset = FALSE;
			DWORD bytes_returned = 0;
			WSAIoctl(udp.native_handle(), SIO_UDP_CONNRESET, &conn_reset, sizeof(conn_reset),
				nullptr, 0, &bytes_returned, nullptr, nullptr);
		}
#endif

		skt_ = std::make_shared<asio::udp_socket>(std::move(udp), 1000);
		auto self = std::dynamic_pointer_cast<udp_transport>(shared_from_this());
		skt_->bind_data_received_handler(std::bind(&udp_transport::on_data_received, self,
			std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
		skt_->post_read();
		if (log_)
		{
			log_->info("UDP transport opened: udpv4 0.0.0.0:{}", local_port_)->flush();
		}
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

