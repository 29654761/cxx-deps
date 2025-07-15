#pragma once

#include "sip_message.h"
#include <spdlog/spdlogger.hpp>
#include <sys2/async_socket.h>
#include <sys2/mutex_callback.hpp>
#include <memory>
#include <atomic>

class sip_server;


class sip_session:public std::enable_shared_from_this<sip_session>
{
public:
	sip_session(sip_server* server, spdlogger_ptr log, sys::async_socket_ptr socket, const sockaddr* addr, socklen_t addr_size);
	virtual ~sip_session();

	virtual std::string id()const = 0;

	virtual bool open();
	virtual void close();
	virtual bool is_timeout()const;
	virtual bool is_tcp()const = 0;
	virtual bool send_message(const sip_message& message) = 0;
	virtual void on_data_received(const char* buffer, uint32_t size, const sockaddr* addr, socklen_t addr_size) = 0;
	virtual void on_message(const sip_message& message);

	//bool send_request(sip_message& request);
	//bool send_response(sip_message& response);
	//bool send_response(const std::string& status,const std::string& message,const std::string& content="",const std::string& content_type="");
	
	std::string get_client_address(const sip_message& request);
	void get_remote_address(std::string& ip, int& port);
	bool get_local_address(std::string& ip, int& port);
protected:
	


	static void s_on_read_event_t(void* ctx, sys::async_socket_ptr socket, const char* buffer, uint32_t size, const sockaddr* addr, socklen_t addr_size);
	static void s_on_closed_event_t(void* ctx, sys::async_socket_ptr socket);
protected:
	spdlogger_ptr log_;
	sys::async_socket_ptr socket_;
	sockaddr_storage dst_addr_ = {};
	sip_server* server_ = nullptr;
	std::atomic<time_t> updated_time_;
};

typedef std::shared_ptr<sip_session> sip_session_ptr;
