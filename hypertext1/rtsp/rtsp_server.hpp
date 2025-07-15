#pragma once

#include "rtsp_session.h"

#include <sys2/socket.h>
#include <sys2/blocking_queue.hpp>
#include <sys2/signal.h>
#include <sys2/callback.hpp>
#include <spdlog/spdlogger.hpp>
#include <future>
#include <map>
#include <shared_mutex>

template<class TSession>
class rtsp_server
{
	friend rtsp_session;
public:
	typedef std::shared_ptr<TSession> TSessionPtr;
	typedef void (*session_created_t)(void* ctx, TSessionPtr sess);
	typedef void (*session_destroyed_t)(void* ctx, TSessionPtr sess);

	rtsp_server(spdlogger_ptr log)
	{
		log_ = log;
	}

	~rtsp_server()
	{
		stop();
	}

	bool start(int port)
	{
		if (active_)
		{
			return false;
		}
		active_ = true;
		socket_ = std::make_shared<sys::socket>(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (!socket_->ready())
		{
			stop();
			return false;
		}

		sockaddr_in addr = { 0 };
		sys::socket::ep2addr(AF_INET, "0.0.0.0", port, (sockaddr*)&addr);
		socket_->set_reuseaddr(true);
		if (!socket_->bind((const sockaddr*)&addr, sizeof(addr)))
		{
			stop();
			return false;
		}

		if (!socket_->listen(0x7FFFFFFF))
		{
			stop();
			return false;
		}
		remove_queue_.reset();
		remove_queue_.clear();
		acceptor_ = new std::thread(&rtsp_server::run_accepter, this);
		timer_ = new std::thread(&rtsp_server::run_timer, this);
		queue_ = new std::thread(&rtsp_server::run_queue, this);
		return true;

	}
	void stop()
	{
		active_ = false;
		signal_.notify();
		if (socket_) {
			socket_->close();
		}
		if (acceptor_.valid())
		{
			acceptor_.get();
		}
		if (timer_.valid())
		{
			timer_.get();
		}
		remove_queue_.close();
		if (queue_.valid())
		{
			queue_.get();
		}
		clear_sessions();
	}

	bool post_remove_session(SOCKET skt)
	{
		return remove_queue_.push(skt);
	}
private:
	void run_accepter()
	{
		while (active_)
		{
			auto client = socket_->accept();
			if (!client)
			{
				if (log_) {
					log_->warn("RTSP Server accept failed")->flush();
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				continue;
			}
			if (log_) {
				log_->info("RTSP Server accept new client")->flush();
			}
			auto sess = std::make_shared<rtsp_session>(this, log_, client);
			if (sess && sess->open())
			{
				set_session(sess->id(), sess);
				if (log_) {
					log_->info("RTSP Server accept new client finished")->flush();
				}
				session_created_event.invoke(sess);
			}
			else
			{
				if (log_) {
					log_->info("RTSP Server accept new client falied")->flush();
				}
			}
		}

		if (log_) {
			log_->warn("RTSP Server accept stoped")->flush();
		}
	}
	void run_timer()
	{
		while (active_)
		{
			signal_.wait(30000);
			std::vector<rtsp_session_ptr> ctxs;
			all_sessions(ctxs);

			for (auto itr = ctxs.begin(); itr != ctxs.end(); itr++)
			{
				if ((*itr)->is_timeout())
				{
					remove_queue_.push((*itr)->id());
					if (log_) {
						log_->warn("RTSP Session timeout")->flush();
					}
				}
			}
		}
	}
	void run_queue()
	{
		while (active_)
		{
			int64_t id = 0;
			if (!remove_queue_.pop(id))
			{
				break;
			}

			remove_session((SOCKET)id);
		}
	}

	void set_session(SOCKET skt, TSessionPtr session)
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		session_map_[skt] = session;
	}
	bool remove_session(SOCKET skt)
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		auto itr = session_map_.find(skt);
		if (itr == session_map_.end())
		{
			return false;
		}


		session_destroyed_event.invoke(itr->second);

		itr->second->close();
		session_map_.erase(skt);

		if (log_) {
			log_->debug("RTSP Session removed")->flush();
		}
		return true;
	}
	TSessionPtr get_session(SOCKET skt)
	{
		std::shared_lock<std::shared_mutex> lk(mutex_);
		auto itr = session_map_.find(skt);
		if (itr == session_map_.end())
		{
			return nullptr;
		}
		return itr->second;
	}
	void clear_sessions()
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		session_map_.clear();
	}
	void all_sessions(std::vector<TSessionPtr>& sessions);

public:
	sys::callback<session_created_t> session_created_event;
	sys::callback<session_destroyed_t> session_destroyed_event;
	sys::callback<session_on_message_t> on_message_event;
	sys::callback<session_on_media_data_t> on_media_data_event;
private:
	spdlogger_ptr log_;
	bool active_ = false;
	sys::socket_ptr socket_;
	std::future<void> acceptor_;
	std::future<void> timer_;
	std::future<void> queue_;

	sys::signal signal_;

	std::shared_mutex mutex_;
	std::map<SOCKET, TSessionPtr> session_map_;

	sys::blocking_queue<int64_t> remove_queue_;

};

