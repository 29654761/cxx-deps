#pragma once
#include "sip_connection.h"
#include <sys2/ring_buffer.hpp>
#include <sys2/asio_queue.hpp>
#include <queue>


class sip_tcp_connection :public sip_connection
{
public:
	typedef std::shared_ptr<asio::ip::tcp::socket> socket_ptr;

	sip_tcp_connection(asio::io_context& ioc,socket_ptr socket);
	sip_tcp_connection(asio::io_context& ioc, const asio::ip::tcp::endpoint& ep);
	~sip_tcp_connection();

	virtual bool start();
	virtual void stop();

	virtual bool is_tcp()const { return true; }

	virtual bool send_message(const sip_message& message);
	virtual bool remote_address(std::string& ip, int& port)const;
	virtual bool local_address(std::string& ip, int& port)const;
private:
	void handle_read(sip_connection_ptr con, const std::error_code& ec, std::size_t bytes);
private:
	socket_ptr socket_;
	asio::ip::tcp::endpoint remote_ep_;
	bool active_ = false;
	std::array<char, 10240> recv_buffer_;
	sys::ring_buffer<char> recv_ring_buffer_;

	asio::sending_queue_ptr sending_queue_;
};

