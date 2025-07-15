#pragma once

#include "sip_connection.h"

#include <sys2/blocking_queue.hpp>
#include <sys2/signal.h>
#include <sys2/callback.hpp>
#include <spdlog/spdlogger.hpp>
#include <thread>
#include <map>
#include <shared_mutex>

class sip_server;
typedef std::shared_ptr<sip_server> sip_server_ptr;

class sip_server:public std::enable_shared_from_this<sip_server>
{
public:
	typedef std::shared_ptr<asio::ip::tcp::acceptor> acceptor_ptr;
	typedef std::shared_ptr<asio::ip::tcp::socket> tcp_socket_ptr;
	typedef std::shared_ptr<asio::ip::udp::socket> udp_socket_ptr;

	typedef std::function<void(sip_server_ptr, sip_connection_ptr, const sip_message&)> message_handler;
	typedef std::function<void(sip_server_ptr, sip_connection_ptr)> close_con_handler;

	sip_server(asio::io_context& ioc);
	virtual ~sip_server();

	void set_logger(spdlogger_ptr log) { log_ = log; }
	void set_message_handler(message_handler handler) { message_handler_ = handler; }
	void set_close_con_handler(close_con_handler handler) { close_con_handler_ = handler; }

	bool start(const std::string& addr, int port);
	void stop();

	sip_connection_ptr create_connection(const voip_uri& url, bool tcp);
private:
	void handle_message(sip_server_ptr svr,sip_connection_ptr con, const sip_message& msg);
	void handle_remove(sip_server_ptr svr, sip_connection_ptr con);
	void handle_accept(sip_server_ptr svr, const std::error_code& ec, asio::ip::tcp::socket newsocket);
	void handle_read(sip_server_ptr svr, udp_socket_ptr skt, const std::error_code& ec, std::size_t bytes);
	void handle_timer(sip_server_ptr svr, const std::error_code& ec);
private:
	bool add_connection(sip_connection_ptr con);
	void remove_connection(const std::string& id);
	void all_connections(std::vector<sip_connection_ptr>& cons);
	void clear_connections();
protected:
	asio::io_context& ioc_;
	spdlogger_ptr log_;
	acceptor_ptr tcp_socket_;
	udp_socket_ptr udp_socket_;

	std::array<char, 10240> udp_recv_buffer_;
	asio::ip::udp::endpoint udp_endpoint_;

	mutable std::mutex connections_mutex_;
	std::map<std::string, sip_connection_ptr> connections_;


	asio::steady_timer timer_;

	message_handler message_handler_;
	close_con_handler close_con_handler_;
};

