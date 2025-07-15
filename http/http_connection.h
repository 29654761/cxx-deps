#pragma once

#include "http_request.h"
#include "http_response.h"
#include <spdlog/spdlogger.hpp>
#include <memory>
#include <asio.hpp>
#include <asio/ssl.hpp>
#include <sys2/asio_queue.hpp>
#include <sys2/ring_buffer.hpp>
#include <functional>
#include <queue>

class http_server;
typedef std::shared_ptr<http_server> http_server_ptr;

class http_connection;
typedef std::shared_ptr<http_connection> http_connection_ptr;



class http_connection :public std::enable_shared_from_this<http_connection>
{
public:
	typedef std::shared_ptr<asio::ip::tcp::socket> socket_ptr;
	typedef std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket>> ssl_socket_ptr;
	typedef std::function<void(http_connection_ptr, const http_request&)> message_handler;
	typedef std::function<void(http_connection_ptr)> close_handler;

	http_connection(asio::io_context& ioc, socket_ptr socket);
	http_connection(asio::io_context& ioc, ssl_socket_ptr socket);
	virtual ~http_connection();

	void set_logger(spdlogger_ptr log) { log_ = log; }

	virtual bool start();
	virtual void stop();
	bool is_timeout()const;

	virtual bool send_response(http_response& response);
	virtual bool send_response(const std::string& status,const std::string& message,const std::string& content="",const std::string& content_type="application/json;charset=UTF-8");
	virtual bool send_response(const http_request& request, const std::string& status, const std::string& message, const std::string& context="",const std::string& content_type="application/json;charset=UTF-8");
	
	std::string get_client_address(const http_request& request);

	void set_message_handler(message_handler handler) { message_handler_ = handler; }
	void set_close_handler(close_handler handler) { close_handler_ = handler; }
protected:
	void print_message(const http_request& request,const http_response& response);
	bool async_read();
	void handle_handshake(http_connection_ptr con, const asio::error_code& ec);
	void handle_read(http_connection_ptr con, const asio::error_code& ec, std::size_t bytes);
	
protected:
	spdlogger_ptr log_;
	socket_ptr socket_;
	ssl_socket_ptr ssl_socket_;
	std::array<char, 10240> recv_buffer_;
	sys::ring_buffer<char> recv_ring_buffer_;
	std::atomic<time_t> updated_time_;
	
	asio::sending_queue_ptr sending_queue_;

	message_handler message_handler_;
	close_handler close_handler_;

};


