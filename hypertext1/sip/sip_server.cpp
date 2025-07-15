#include "sip_server.h"
#include "sip_tcp_session.h"
#include "sip_udp_session.h"
#include <sys2/uri.h>
#include <sys2/util.h>

sip_server::sip_server()
{
}

sip_server::~sip_server()
{
	stop();
}




bool sip_server::start(const std::string& addr, int port, int worknum)
{
	if (active_)
	{
		return false;
	}
	active_ = true;

	SOCKET tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (!sys::socket::ready(tcp))
	{
		stop();
		return false;
	}
	sockaddr_in addrin = { 0 };
	sys::socket::ep2addr(AF_INET, addr.c_str(), port, (sockaddr*)&addrin);
	sys::socket::set_reuseaddr(tcp,true);
	if (::bind(tcp,(const sockaddr*)&addrin, sizeof(addrin))<0)
	{
		sys::socket::close(tcp);
		stop();
		return false;
	}

	if (listen(tcp,0x7FFFFFFF)<0)
	{
		sys::socket::close(tcp);
		stop();
		return false;
	}

	SOCKET udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (!sys::socket::ready(udp))
	{
		sys::socket::close(tcp);
		stop();
		return false;
	}
	sys::socket::set_reuseaddr(udp, true);
	if (::bind(udp, (const sockaddr*)&addrin, sizeof(addrin)) < 0)
	{
		sys::socket::close(tcp);
		sys::socket::close(udp);
		stop();
		return false;
	}

	if (!ios_.open(worknum,"sip"))
	{
		sys::socket::close(tcp);
		sys::socket::close(udp);
		stop();
		return false;
	}

	tcp_socket_ =ios_.bind(tcp);
	if (!tcp_socket_)
	{
		stop();
		return false;
	}
	tcp_socket_->accept_event.add(s_on_accept_event_t, this);
	tcp_socket_->post_accept();

	udp_socket_ = ios_.bind(udp);
	if (!udp_socket_)
	{
		stop();
		return false;
	}
	udp_socket_->read_event.add(s_on_read_event_t, this);
	udp_socket_->post_read_from();


	timer_ = std::make_shared<std::thread>(&sip_server::run_timer,this);
	sys::util::set_thread_name("sip.svr.timer", timer_.get());
	return true;
}

void sip_server::stop()
{
	active_ = false;
	signal_.notify();
	if (tcp_socket_)
	{
		tcp_socket_->close();
		tcp_socket_.reset();
	}
	if (udp_socket_)
	{
		udp_socket_->close();
		udp_socket_.reset();
	}
	clear_sessions();
	ios_.close();
	if (timer_)
	{
		timer_->join();
		timer_.reset();
	}



}

sip_session_ptr sip_server::create_session(const voip_uri& url, bool tcp)
{
	if (url.host.empty())
		return nullptr;

	int port = url.port != 0 ? url.port : 5060;

	sockaddr_storage addr = {};
	sys::socket::ep2addr(url.host.c_str(), port, (sockaddr*)&addr);
	
	if (tcp)
	{
		SOCKET skt = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (!sys::socket::ready(skt))
			return nullptr;
		if (!sys::socket::connect(skt, (const sockaddr*)&addr,sizeof(addr)))
		{
			sys::socket::close(skt);
			return nullptr;
		}

		sys::async_socket_ptr askt = ios_.bind(skt, (const sockaddr*)&addr, sizeof(addr));
		if (!askt)
		{
			sys::socket::close(skt);
			return nullptr;
		}
		auto sess = std::make_shared<sip_tcp_session>(this, log_, askt, (const sockaddr*)&addr, (socklen_t)sizeof(addr));
		if (!sess->open())
			return nullptr;
		set_session(sess->id(), sess);
		
		return sess;
	}
	else
	{
		auto sess = std::make_shared<sip_udp_session>(this, log_, udp_socket_, (const sockaddr*)&addr, (socklen_t)sizeof(addr));
		if (!sess->open())
			return nullptr;
		set_session(sess->id(), sess);
		return sess;
	}
}


void sip_server::on_close_session(sip_session_ptr session)
{
	remove_session(session->id());
}


/*
void sip_server::run_accepter()
{
	while (active_)
	{
		sys::socket_selector sel;
		if (tcp_socket_) {
			sel.add_to_read(tcp_socket_->handle());
		}

		int idx=sel.wait();
		if (idx <= 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		if (sel.is_readable(tcp_socket_->handle()))
		{
			auto client = tcp_socket_->accept();
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
			auto sess = std::make_shared<sip_tcp_session>(this, log_, client);
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
*/

void sip_server::run_timer()
{
	while (active_)
	{
		signal_.wait(30000);
		std::vector<sip_session_ptr> ctxs;
		all_sessions(ctxs);

		for (auto itr = ctxs.begin(); itr != ctxs.end(); itr++)
		{
			if ((*itr)->is_timeout())
			{
				if (log_) {
					log_->warn("SIP Session timeout")->flush();
				}
				remove_session((*itr)->id());
			}
		}
	}
}



void sip_server::set_session(const std::string& id, sip_session_ptr session)
{
	std::unique_lock<std::shared_mutex> lk(mutex_);
	session_map_[id] = session;
}


bool sip_server::remove_session(const std::string& id)
{
	std::unique_lock<std::shared_mutex> lk(mutex_);

	auto itr=session_map_.find(id);
	if (itr == session_map_.end())
	{
		return false;
	}
	session_destroyed_event.invoke(itr->second);
	itr->second->close();
	session_map_.erase(id);
	if (log_) {
		log_->debug("SIP Session removed")->flush();
	}
	return true;
}

sip_session_ptr sip_server::get_session(const std::string& id)
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	auto itr = session_map_.find(id);
	if (itr == session_map_.end())
	{
		return nullptr;
	}
	return itr->second;
}

void sip_server::clear_sessions()
{
	std::unique_lock<std::shared_mutex> lk(mutex_);
	session_map_.clear();
}

void sip_server::all_sessions(std::vector<sip_session_ptr>& sessions)
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	for (auto itr = session_map_.begin(); itr != session_map_.end(); itr++)
	{
		sessions.push_back(itr->second);
	}
}


void sip_server::s_on_accept_event_t(void* ctx, sys::async_socket_ptr socket, sys::async_socket_ptr newsocket, const sockaddr* addr, socklen_t addr_size)
{
	sip_server* p = (sip_server*)ctx;
	auto sess = std::make_shared<sip_tcp_session>(p,p->log_,newsocket,addr,addr_size);
	if (sess->open())
	{
		p->set_session(sess->id(), sess);
		p->session_created_event.invoke(sess);
	}
	socket->post_accept();
}

void sip_server::s_on_read_event_t(void* ctx, sys::async_socket_ptr socket, const char* buffer, uint32_t size, const sockaddr* addr, socklen_t addr_size)
{
	sip_server* p = (sip_server*)ctx;
	std::string ip;
	int port = 0;
	sys::socket::addr2ep(addr, &ip, &port);
	std::string id = "udp$" + ip + ":" + std::to_string(port);

	sip_session_ptr session;
	{
		std::shared_lock<std::shared_mutex> glk(p->mutex_);
		auto itr = p->session_map_.find(id);
		if (itr != p->session_map_.end())
		{
			session = itr->second;
		}
	}

	if (!session)
	{
		std::unique_lock<std::shared_mutex> ulk(p->mutex_);
		auto itr = p->session_map_.find(id);
		if (itr != p->session_map_.end())
		{
			session = itr->second;
		}
		else
		{
			session = std::make_shared<sip_udp_session>(p, p->log_, socket, addr, addr_size);
			if (session->open())
			{
				p->session_map_.insert(std::make_pair(id,session));
				p->session_created_event.invoke(session);
			}
		}
	}

	if (session)
	{
		session->on_data_received(buffer, size, addr, addr_size);
	}

	socket->post_read_from();
}



