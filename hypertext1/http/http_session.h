#pragma once

#include "http_request.h"
#include "http_response.h"
#include <spdlog/spdlogger.hpp>
#include <sys2/socket.h>
#include <sys2/ssl_socket.h>
#include <sys2/mutex_callback.hpp>
#include <memory>
#include <atomic>


class http_server;


class http_session:public std::enable_shared_from_this<http_session>
{
public:
	http_session(http_server* server,spdlogger_ptr log, sys::socket_ptr socket);
	http_session(http_server* server, spdlogger_ptr log, sys::ssl_socket_ptr socket);
	virtual ~http_session();

	SOCKET id()const;

	virtual bool open();
	virtual void close();
	bool is_timeout()const;

	virtual bool send_response(http_response& response);
	virtual bool send_response(const std::string& status,const std::string& message,const std::string& content="",const std::string& content_type="application/json;charset=UTF-8");
	virtual bool send_response(const http_request& request, const std::string& status, const std::string& message, const std::string& context="",const std::string& content_type="application/json;charset=UTF-8");
	
	std::string get_client_address(const http_request& request);
protected:
	virtual void on_message(const http_request& request);
	void print_message(const http_request& request,const http_response& response);
private:

	void run();
	

	bool read_line(std::string& s);
	bool read_size(std::string& buffer, int size);
	bool read_size(char* buffer, int size);

	int read_buffer(char* buffer, int size);
protected:
	spdlogger_ptr log_;
	http_server* server_ = nullptr;
	bool active_ = true;
	int64_t id_=0;
	sys::socket_ptr socket_;
	sys::ssl_socket_ptr ssl_socket_;
	std::atomic<time_t> updated_time_;
	std::thread* worker_;

	
};

typedef std::shared_ptr<http_session> http_session_ptr;
