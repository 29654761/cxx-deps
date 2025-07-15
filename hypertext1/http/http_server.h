#pragma once

#include "http_session.h"

#include <sys2/socket.h>
#include <sys2/ssl_socket.h>
#include <sys2/blocking_queue.hpp>
#include <sys2/signal.h>
#include <sys2/callback.hpp>
#include <spdlog/spdlogger.hpp>
#include <thread>
#include <map>
#include <shared_mutex>


class http_server
{
	friend http_session;
public:
	typedef void(*session_created_t)(void* ctx,http_session_ptr session);
	typedef void(*session_destroyed_t)(void* ctx, http_session_ptr session);
	typedef void(*session_message_t)(void* ctx, http_session_ptr session,const http_request& request);

	http_server(spdlogger_ptr log);
	virtual ~http_server();

	
	bool listen(int port);
	bool ssl_listen(int port, const std::string& cert, const std::string& pkey);
	bool start();
	void stop();

private:
	void run_accepter();
	void run_timer();
	void run_queue();

	void set_session(SOCKET skt, http_session_ptr session);
	bool remove_session(SOCKET skt);
	http_session_ptr get_session(SOCKET skt);
	void clear_sessions();
	void all_sessions(std::vector<http_session_ptr>& sessions);

public:
	sys::callback<session_created_t> session_created_event;
	sys::callback<session_destroyed_t> session_destroyed_event;
	sys::callback<session_message_t> session_message_event;
protected:
	spdlogger_ptr log_;
	bool active_ = false;
	sys::socket_ptr socket_;
	sys::socket_ptr ssl_socket_;
	sys::ssl_context_ptr ssl_context_;
	std::thread* acceptor_=nullptr;
	std::thread* timer_ = nullptr;
	std::thread* queue_ = nullptr;

	sys::signal signal_;

	std::shared_mutex mutex_;
	std::map<SOCKET, http_session_ptr> session_map_;

	sys::blocking_queue<int64_t> remove_queue_;
};

