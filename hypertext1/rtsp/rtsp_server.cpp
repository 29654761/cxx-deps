#include "rtsp_server.h"


rtsp_server::rtsp_server(spdlogger_ptr log)
{
	log_ = log;
}

rtsp_server::~rtsp_server()
{
	stop();
}

bool rtsp_server::start(int port)
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
	acceptor_ = new std::thread(&rtsp_server::run_accepter,this);
	timer_ = new std::thread(&rtsp_server::run_timer,this);
	queue_=new std::thread(&rtsp_server::run_queue, this);
	return true;
}

void rtsp_server::stop()
{
	active_ = false;
	signal_.notify();
	socket_->close();
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

void rtsp_server::destory_session(rtsp_session_ptr session)
{
}

void rtsp_server::run_accepter()
{
	while (active_)
	{
		auto client=socket_->accept();
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
		auto sess=create_session(client);
		if (set_session(sess))
		{
			if (sess->start()) {
				if (log_) {
					log_->info("RTSP Server accept new client finished")->flush();
				}
			}
			else {
				if (log_) {
					log_->error("RTSP Server accept new client start error")->flush();
				}
			}
		}
		else
		{
			sess->close();
			if (log_) {
				log_->error("RTSP Server accept new client error")->flush();
			}
		}
	}
	if (log_) {
		log_->warn("RTSP Server accept stoped")->flush();
	}
}

void rtsp_server::run_timer()
{
	while (active_)
	{
		signal_.wait(30000);
		std::vector<std::shared_ptr<rtsp_session>> ctxs;
		all_sessions(ctxs);
		if (log_) {
			log_->info("RTSP Session count={}", ctxs.size())->flush();
		}
		for (auto itr = ctxs.begin(); itr != ctxs.end(); itr++)
		{
			if ((*itr)->is_timeout())
			{
				remove_queue_.push((*itr)->id());
				if (log_) {
					std::string url = (*itr)->url().to_string();
					log_->warn("RTSP Session timeout id={} url={}", (*itr)->id(), url)->flush();
				}
			}
		}
	}
}

void rtsp_server::run_queue()
{
	while (active_)
	{
		std::string id;
		if (!remove_queue_.pop(id))
		{
			if (log_) {
				log_->info("RTSP Server queue exited {}", id)->flush();
			}
			break;
		}

		remove_session(id);
	}
}

bool rtsp_server::set_session(std::shared_ptr<rtsp_session> session)
{
	std::unique_lock<std::shared_mutex> lk(mutex_);
	auto r=session_map_.insert(std::make_pair(session->id(),session));
	return r.second;
}


bool rtsp_server::remove_session(const std::string& id)
{
	if (log_) {
		log_->debug("RTSP Session begin removing id={}", id)->flush();
	}
	rtsp_session_ptr sess;
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);

		auto itr = session_map_.find(id);
		if (itr == session_map_.end())
		{
			if (log_) {
				log_->debug("RTSP Session remove faied id={}", id)->flush();
			}
			return false;
		}
		sess = itr->second;
		session_map_.erase(itr);
	}
	destory_session(sess);
	sess->close();
	if (log_) {
		log_->debug("RTSP Session removed id={}", id)->flush();
	}
	return true;
}

std::shared_ptr<rtsp_session> rtsp_server::get_session(const std::string& id)
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	auto itr = session_map_.find(id);
	if (itr == session_map_.end())
	{
		return nullptr;
	}
	return itr->second;
}

void rtsp_server::clear_sessions()
{
	std::unique_lock<std::shared_mutex> lk(mutex_);
	session_map_.clear();
}

void rtsp_server::all_sessions(std::vector<std::shared_ptr<rtsp_session>>& sessions)
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	for (auto itr = session_map_.begin(); itr != session_map_.end(); itr++)
	{
		sessions.push_back(itr->second);
	}
}


