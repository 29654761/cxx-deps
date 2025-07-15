#pragma once

#include "rtsp_session.h"

#include <sys2/socket.h>
#include <sys2/blocking_queue.hpp>
#include <sys2/signal.h>
#include <spdlog/spdlogger.hpp>
#include <thread>
#include <map>
#include <shared_mutex>


class rtsp_server
{
	friend rtsp_session;
public:
	rtsp_server(spdlogger_ptr log);
	virtual ~rtsp_server();

	virtual bool start(int port);
	virtual void stop();

	virtual rtsp_session_ptr create_session(std::shared_ptr<sys::socket> socket) = 0;
	virtual void destory_session(rtsp_session_ptr session);
private:
	void run_accepter();
	void run_timer();
	void run_queue();

	bool set_session(std::shared_ptr<rtsp_session> session);
	bool remove_session(const std::string& id);
	std::shared_ptr<rtsp_session> get_session(const std::string& id);
	void clear_sessions();
	void all_sessions(std::vector<std::shared_ptr<rtsp_session>>& sessions);
private:
	spdlogger_ptr log_;
	bool active_ = false;
	sys::socket_ptr socket_;
	std::thread* acceptor_=nullptr;
	std::thread* timer_ = nullptr;
	std::thread* queue_ = nullptr;

	sys::signal signal_;

	std::shared_mutex mutex_;
	std::map<std::string, std::shared_ptr<rtsp_session>> session_map_;

	sys::blocking_queue<std::string> remove_queue_;
};

