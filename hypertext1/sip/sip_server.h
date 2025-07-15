#pragma once

#include "sip_session.h"

#include <sys2/async_socket.h>
#include <sys2/blocking_queue.hpp>
#include <sys2/signal.h>
#include <sys2/callback.hpp>
#include <spdlog/spdlogger.hpp>
#include <thread>
#include <map>
#include <shared_mutex>

class sip_server
{
public:
	typedef void(*session_created_t)(void* ctx,sip_session_ptr session);
	typedef void(*session_destroyed_t)(void* ctx, sip_session_ptr session);
	typedef void(*session_message_t)(void* ctx, sip_session_ptr session,const sip_message& message);

	sip_server();
	virtual ~sip_server();

	void set_logger(spdlogger_ptr log) { log_ = log; }

	bool start(const std::string& addr, int port,int worknum);
	void stop();

	sip_session_ptr create_session(const voip_uri& url, bool tcp);

	void on_close_session(sip_session_ptr session);
private:
	void run_timer();

	void set_session(const std::string& id, sip_session_ptr session);
	bool remove_session(const std::string& id);
	sip_session_ptr get_session(const std::string& id);
	void clear_sessions();
	void all_sessions(std::vector<sip_session_ptr>& sessions);



	static void s_on_accept_event_t(void* ctx, sys::async_socket_ptr socket, sys::async_socket_ptr newsocket, const sockaddr* addr, socklen_t addr_size);
	static void s_on_read_event_t(void* ctx, sys::async_socket_ptr socket, const char* buffer, uint32_t size, const sockaddr* addr, socklen_t addr_size);
public:
	sys::callback<session_created_t> session_created_event;
	sys::callback<session_destroyed_t> session_destroyed_event;
	sys::callback<session_message_t> session_message_event;
protected:
	spdlogger_ptr log_;
	bool active_ = false;
	sys::async_socket_ptr tcp_socket_;
	sys::async_socket_ptr udp_socket_;
	sys::async_service ios_;
	std::shared_ptr<std::thread> timer_ = nullptr;
	sys::signal signal_;
	std::shared_mutex mutex_;
	std::map<std::string, sip_session_ptr> session_map_;
};

