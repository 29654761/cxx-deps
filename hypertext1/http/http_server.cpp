#include "http_server.h"
#include <sys2/util.h>

http_server::http_server(spdlogger_ptr log)
{
	log_ = log;
}

http_server::~http_server()
{
	stop();
}

bool http_server::listen(int port)
{
	socket_ = std::make_shared<sys::socket>(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (!socket_->ready())
	{
		return false;
	}
	sockaddr_in6 addr = { 0 };
	sys::socket::ep2addr(AF_INET6, nullptr, port, (sockaddr*)&addr);
	socket_->set_reuseaddr(true);
	if (!socket_->bind((const sockaddr*)&addr, sizeof(addr)))
	{
		socket_.reset();
		return false;
	}

	if (!socket_->listen(0x7FFFFFFF))
	{
		socket_.reset();
		return false;
	}

	return true;
}

bool http_server::ssl_listen(int port, const std::string& cert, const std::string& pkey)
{
	ssl_socket_ = std::make_shared<sys::socket>(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (!ssl_socket_->ready())
	{
		return false;
	}

	ssl_context_ = std::make_shared<sys::ssl_context>();
	if (!ssl_context_->load(cert, pkey))
	{
		ssl_socket_.reset();
		return false;
	}
	sockaddr_in6 addr = { 0 };
	sys::socket::ep2addr(AF_INET6, nullptr, port, (sockaddr*)&addr);
	ssl_socket_->set_reuseaddr(true);
	if (!ssl_socket_->bind((const sockaddr*)&addr, sizeof(addr)))
	{
		ssl_socket_.reset();
		ssl_context_.reset();
		return false;
	}

	if (!ssl_socket_->listen(0x7FFFFFFF))
	{
		ssl_socket_.reset();
		ssl_context_.reset();
		return false;
	}

	return true;
}

bool http_server::start()
{
	if (active_)
	{
		return false;
	}
	active_ = true;
	
	remove_queue_.reset();
	remove_queue_.clear();

	acceptor_ = new std::thread(&http_server::run_accepter, this);
	sys::util::set_thread_name("http.acceptor",acceptor_);
	timer_ = new std::thread(&http_server::run_timer,this);
	sys::util::set_thread_name("http.timer", timer_);
	queue_=new std::thread(&http_server::run_queue, this);
	sys::util::set_thread_name("http.queue", queue_);
	return true;
}

void http_server::stop()
{
	active_ = false;
	signal_.notify();
	if (socket_) {
		socket_->close();
	}
	if (acceptor_)
	{
		acceptor_->join();
		delete acceptor_;
		acceptor_ = nullptr;
	}
	if (timer_)
	{
		timer_->join();
		delete timer_;
		timer_ = nullptr;
	}
	remove_queue_.close();
	if (queue_)
	{
		queue_->join();
		delete queue_;
		queue_ = nullptr;
	}
	clear_sessions();
}




void http_server::run_accepter()
{
	while (active_)
	{
		sys::socket_selector sel;
		if (socket_) {
			sel.add_to_read(socket_->handle());
		}
		if (ssl_socket_) {
			sel.add_to_read(ssl_socket_->handle());
		}

		int idx=sel.wait();
		if (idx <= 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		if (sel.is_readable(socket_->handle()))
		{
			auto client = socket_->accept();
			if (!client)
			{
				if (log_) {
					log_->warn("HTTP Server accept failed")->flush();
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				continue;
			}
			if (log_) {
				log_->info("HTTP Server accept new client")->flush();
			}
			auto sess = std::make_shared<http_session>(this, log_, client);
			if (sess->open())
			{
				set_session(sess->id(), sess);
				session_created_event.invoke(sess);
			}
		}
		else if (sel.is_readable(ssl_socket_->handle()))
		{
			auto client = ssl_socket_->accept();
			sys::ssl_socket_ptr sslclient = std::make_shared<sys::ssl_socket>(ssl_context_, client);
			if (!sslclient->ssl_accept())
			{
				if (log_) {
					log_->warn("HTTP Server accept ssl failed")->flush();
				}
				continue;
			}
			if (log_) {
				log_->info("HTTP Server accept new client")->flush();
			}
			auto sess = std::make_shared<http_session>(this, log_, sslclient);
			if (sess->open())
			{
				set_session(sess->id(), sess);
				session_created_event.invoke(sess);
			}
		}
		if (log_) {
			log_->info("HTTP Server accept new client finished")->flush();
		}
	}
	if (log_) {
		log_->warn("HTTP Server accept stoped")->flush();
	}
}

void http_server::run_timer()
{
	while (active_)
	{
		signal_.wait(30000);
		std::vector<http_session_ptr> ctxs;
		all_sessions(ctxs);

		for (auto itr = ctxs.begin(); itr != ctxs.end(); itr++)
		{
			if ((*itr)->is_timeout())
			{
				remove_queue_.push((*itr)->id());
				if (log_) {
					log_->warn("HTTP Session timeout")->flush();
				}
			}
		}
	}
}

void http_server::run_queue()
{
	while (active_)
	{
		int64_t id=0;
		if (!remove_queue_.pop(id))
		{
			break;
		}

		remove_session((SOCKET)id);
	}
}

void http_server::set_session(SOCKET skt, http_session_ptr session)
{
	std::unique_lock<std::shared_mutex> lk(mutex_);
	session_map_[skt] = session;
}


bool http_server::remove_session(SOCKET skt)
{
	std::unique_lock<std::shared_mutex> lk(mutex_);

	auto itr=session_map_.find(skt);
	if (itr == session_map_.end())
	{
		return false;
	}
	session_destroyed_event.invoke(itr->second);
	session_map_.erase(skt);
	if (log_) {
		log_->debug("HTTP Session removed")->flush();
	}
	return true;
}

http_session_ptr http_server::get_session(SOCKET skt)
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	auto itr = session_map_.find(skt);
	if (itr == session_map_.end())
	{
		return nullptr;
	}
	return itr->second;
}

void http_server::clear_sessions()
{
	std::unique_lock<std::shared_mutex> lk(mutex_);
	session_map_.clear();
}

void http_server::all_sessions(std::vector<http_session_ptr>& sessions)
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	for (auto itr = session_map_.begin(); itr != session_map_.end(); itr++)
	{
		sessions.push_back(itr->second);
	}
}


