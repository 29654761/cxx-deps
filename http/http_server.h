#pragma once

#include "http_connection.h"
#include <spdlog/spdlogger.hpp>


class http_server;
typedef std::shared_ptr<http_server> http_server_ptr;

class http_server :public std::enable_shared_from_this<http_server>
{
public:
	typedef std::shared_ptr<asio::ip::tcp::acceptor> acceptor_ptr;
	typedef std::shared_ptr<asio::ip::tcp::socket> socket_ptr;
	typedef std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket>> ssl_socket_ptr;
	typedef std::function<void(http_connection_ptr, const http_request&)> message_handler ;

	http_server(asio::io_context& ioc);
	virtual ~http_server();

	void set_logger(spdlogger_ptr log) { log_ = log; }
	void set_message_handler(message_handler handler) { message_handler_ = handler; }
	
	bool listen(int port);
	bool ssl_listen(int port, std::shared_ptr<asio::ssl::context> ssl);
	bool start();
	void stop();

private:
	void handle_accepted(http_server_ptr svr,const asio::error_code& ec, asio::ip::tcp::socket socket);
	void handle_ssl_accepted(http_server_ptr svr,const asio::error_code& ec, asio::ip::tcp::socket socket);
	void handle_message(http_server_ptr svr, http_connection_ptr con,const http_request& request);
	void handle_close(http_server_ptr svr, http_connection_ptr con);
	void handle_timer(http_server_ptr svr, const std::error_code& ec);
private:
	void add_connection(http_connection_ptr con);
	void remove_connection(http_connection_ptr con);
	void all_connections(std::vector<http_connection_ptr>& cons);
	void clear_connections();
protected:
	spdlogger_ptr log_;
	asio::io_context& io_context_;
	std::shared_ptr<asio::ssl::context> ssl_context_;
	acceptor_ptr acceptor_;
	acceptor_ptr ssl_acceptor_;
	message_handler message_handler_;

	std::mutex connections_mutex_;
	std::vector<http_connection_ptr> connections_;


	asio::steady_timer timer_;
};

