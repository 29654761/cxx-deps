/**
 * @file rtpx_core.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#include "rtpx_core.h"



namespace rtpx {

	rtpx_core* rtpx_core::s_instance = nullptr;

	rtpx_core::rtpx_core()
		:timer_(ioc_)
	{
	}

	rtpx_core::~rtpx_core()
	{
		cleanup();
	}

	rtpx_core* rtpx_core::instance()
	{
		if (!s_instance)
		{
			s_instance = new rtpx_core();
		}
		return s_instance;
	}

	void rtpx_core::release()
	{
		if (s_instance)
		{
			delete s_instance;
			s_instance = nullptr;
		}
	}

	bool rtpx_core::init(size_t work_nums)
	{
		if (active_)
			return true;
		active_ = true;
		log_ = std::make_shared<spdlogger>();
		log_->init("rtpx", nullptr, log_level_t::trace);


#ifdef RTPX_SSL
		srtp_err_status_t srtp = srtp_init();
		if (srtp != srtp_err_status_ok)
		{
			cleanup();
			return false;
		}

		cert_ = std::make_shared<cert>();
		if (!cert_->create_cert())
		{
			cleanup();
			return false;
		}

		//cert_->export_cert("E:\\cacert.pem");
		//cert_->export_key("E:\\privkey.pem");
#endif




		for (size_t i = 0; i < work_nums; i++)
		{
			auto wk = std::make_unique<std::thread>(&rtpx_core::run, this);
			workers_.push_back(std::move(wk));
		}

		timer_.expires_after(std::chrono::seconds(10));
		timer_.async_wait(std::bind(&rtpx_core::handle_timer, this, std::placeholders::_1));

		return true;
	}

	void rtpx_core::cleanup()
	{
		active_ = false;
		ioc_.stop();
		for (auto itr = workers_.begin(); itr != workers_.end(); itr++)
		{
			(*itr)->join();
		}
		workers_.clear();

		//sockets_.clear();
		//transports_.clear();


#ifdef RTPX_SSL
		srtp_shutdown();
		if (cert_)
		{
			cert_.reset();
		}
#endif

		log_.reset();
	}

	rtp_session_ptr rtpx_core::create_rtp_session(bool stun,bool port_mux)
	{
		return std::make_shared<rtp_session>(ioc_, log_, stun, port_mux);
	}

	bool rtpx_core::set_port_range(uint16_t min_port, uint16_t max_port)
	{
		std::unique_lock<std::shared_mutex> lock(mutex_);
		ports_.reset(min_port, max_port);
		return true;
	}
	
	void rtpx_core::run()
	{
		auto work = asio::make_work_guard(ioc_.get_executor());
		ioc_.run();
	}

	void rtpx_core::handle_timer(const std::error_code& ec)
	{
		/*
		if (ec||!active_)
			return;

		std::vector<std::string> tps;
		{
			std::shared_lock<std::shared_mutex> lock(mutex_);
			for (auto itr = transports_.begin(); itr != transports_.end(); itr++)
			{
				if (itr->second->is_timeout())
					tps.push_back(itr->first);
			}
		}
		if (tps.size() > 0)
		{
			std::unique_lock<std::shared_mutex> lock(mutex_);
			for (auto itr = tps.begin(); itr != tps.end(); itr++)
			{
				transports_.erase(*itr);
				if (log_)
				{
					log_->debug("RTPX Transport timeout. id={}",*itr)->flush();
				}
			}
		}


		timer_.expires_after(std::chrono::seconds(10));
		timer_.async_wait(std::bind(&rtpx_core::handle_timer, this, std::placeholders::_1));
		*/
	}

	/*
	void rtpx_core::handle_message(uint16_t local_port, asio::udp_socket_ptr skt, const uint8_t* buffer, size_t size, const asio::ip::udp::endpoint& endpoint)
	{
		printf("received\n");

		
		auto transport= create_transport(local_port, skt, endpoint);
		if (transport)
		{
			transport->received_message(buffer, size);
		}
	}

	media_stream_ptr rtpx_core::find_media_stream(uint32_t ssrc, uint16_t port, const const asio::ip::udp::endpoint& remote,bool is_rtp)
	{
		std::shared_lock<std::shared_mutex> lock(mutex_);
		auto itr = media_streams_.find(ssrc);
		if (itr != media_streams_.end())
			return itr->second;

		
		for (auto itr2 = media_streams_.begin(); itr2 != media_streams_.end(); itr2++)
		{
			if (is_rtp)
			{
				if (itr2->second->remote_rtp_endpoint() == remote)
				{
					return itr2->second;
				}
			}
			else
			{
				if (itr2->second->remote_rtcp_endpoint() == remote)
				{
					return itr2->second;
				}
			}
		}

		for (auto itr2 = media_streams_.begin(); itr2 != media_streams_.end(); itr2++)
		{
			if (is_rtp)
			{
				if (itr2->second->local_rtp_port() == port)
				{
					return itr2->second;
				}
			}
			else
			{
				if (itr2->second->local_rtcp_port() == port)
				{
					return itr2->second;
				}
			}
		}

		return nullptr;
	}

	asio::udp_socket_ptr rtpx_core::create_socket(uint16_t port)
	{
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			auto itr = sockets_.find(port);
			if (itr != sockets_.end())
			{
				return nullptr;
			}
		}

		{
			std::unique_lock<std::shared_mutex> lk(mutex_);
			return _create_socket(port);
		}
	}


	bool rtpx_core::destroy_socket(uint16_t port)
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		return _destroy_socket(port);
	}
	*/
	cert_ptr rtpx_core::get_cert()
	{
#ifdef RTPX_SSL
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			if (cert_ && !cert_->is_timeout()) {
				return cert_;
			}
		}
		{
			std::unique_lock<std::shared_mutex> lk(mutex_);
			if (cert_ && !cert_->is_timeout()) {
				return cert_;
			}

			cert_ = std::make_shared<cert>();
			if (!cert_->create_cert())
			{
				cert_.reset();
				return nullptr;
			}

			return cert_;
		}
#else
		return nullptr;
#endif
	}

	dtls_ptr rtpx_core::get_dtls()
	{
#ifdef RTPX_SSL
		cert_ptr cert = get_cert();
		if (!cert)
		{
			return nullptr;
		}
		dtls_ptr dtls = std::make_shared<rtpx::dtls>(cert);
		if (!dtls->open())
		{
			return nullptr;
		}

		return dtls;
#else
		return nullptr;
#endif
	}

	/*
	bool rtpx_core::register_media_stream(media_stream_ptr ms)
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);

		if (!_create_socket(ms->local_rtp_port()))
		{
			return false;
		}
		if (!_create_socket(ms->local_rtcp_port()))
		{
			free_idle_sockets();
			return false;
		}

		auto r=media_streams_.insert(std::make_pair(ms->remote_rtp_ssrc(), ms));
		if (!r.second)
		{
			free_idle_sockets();
			return false;
		}
		if (ms->remote_rtx_ssrc() > 0) {
			r = media_streams_.insert(std::make_pair(ms->remote_rtx_ssrc(), ms));
			if (!r.second) {
				media_streams_.erase(ms->remote_rtp_ssrc());

				free_idle_sockets();
				return false;
			}
		}


		return true;
	}

	void rtpx_core::unregister_media_stream(media_stream_ptr ms)
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		media_streams_.erase(ms->remote_rtp_ssrc());
		media_streams_.erase(ms->remote_rtx_ssrc());
		free_idle_sockets();
	}

	std::shared_ptr<transport> rtpx_core::create_transport(uint16_t port,asio::udp_socket_ptr skt,const asio::ip::udp::endpoint& remote_endpoint)
	{
		std::string key = transport::make_key(remote_endpoint);
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			auto itr = transports_.find(key);
			if (itr != transports_.end())
			{
				return itr->second;
			}
		}

		{
			std::unique_lock<std::shared_mutex> lk(mutex_);
			auto itr = transports_.find(key);
			if (itr != transports_.end())
			{
				return itr->second;
			}



			auto tp=std::make_shared<transport>(port,skt,remote_endpoint,log_);
			tp->set_require_media_stream_handler(std::bind(&rtpx_core::find_media_stream, this,
				std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
			auto r= transports_.insert(std::make_pair(key, tp));
			return r.first->second;
		}
	}

	transport_ptr rtpx_core::create_transport(uint16_t port, transport::send_handler sender, const asio::ip::udp::endpoint& remote_endpoint)
	{
		std::string key = transport::make_key(remote_endpoint);
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			auto itr = transports_.find(key);
			if (itr != transports_.end())
			{
				return itr->second;
			}
		}

		{
			std::unique_lock<std::shared_mutex> lk(mutex_);
			auto itr = transports_.find(key);
			if (itr != transports_.end())
			{
				return itr->second;
			}

			auto tp = std::make_shared<transport>(port,sender, remote_endpoint, log_);
			tp->set_require_media_stream_handler(std::bind(&rtpx_core::find_media_stream, this,
				std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
			auto r = transports_.insert(std::make_pair(key, tp));
			return r.first->second;
		}
	}

	bool rtpx_core::receive_message(const std::string& remote, const uint8_t* buffer, size_t size)
	{
		transport_ptr tp;
		{
			std::unique_lock<std::shared_mutex> lock(mutex_);
			auto itr=transports_.find(remote);
			if (itr != transports_.end())
			{
				tp = itr->second;
			}
		}

		if (!tp)
			return false;

		tp->received_message(buffer, size);
		return true;
	}

	asio::udp_socket_ptr rtpx_core::_create_socket(uint16_t port)
	{
		auto itr = sockets_.find(port);
		if (itr != sockets_.end())
		{
			return nullptr;
		}

		std::error_code ec;
		asio::ip::udp::socket udp(ioc_);
		asio::ip::udp::endpoint ep(asio::ip::udp::v4(), port);
		udp.open(ep.protocol(), ec);
		if (ec)
		{
			return nullptr;
		}
		udp.set_option(asio::socket_base::send_buffer_size(1024 * 1024), ec);
		udp.set_option(asio::socket_base::send_buffer_size(1024 * 1024), ec);
		udp.bind(ep, ec);
		if (ec)
		{
			return nullptr;
		}
		auto skt = std::make_shared<asio::udp_socket>(std::move(udp), 1000);
		skt->set_data_received_handler(std::bind(&rtpx_core::handle_message, this, port, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
		skt->post_read();

		auto r = sockets_.insert(std::make_pair(port, skt));
		return r.first->second;
	}

	bool rtpx_core::_destroy_socket(uint16_t port)
	{
		auto itr = sockets_.find(port);
		if (itr != sockets_.end())
		{
			itr->second->close();
			sockets_.erase(itr);
			return true;
		}
		else
		{
			return false;
		}
	}

	void rtpx_core::free_idle_sockets()
	{
		for (auto itr = sockets_.begin(); itr != sockets_.end();)
		{
			if (is_port_idle(itr->first))
			{
				itr->second->close();
				free_transports(itr->first);
				itr=sockets_.erase(itr);
			}
			else
			{
				itr++;
			}
		}
	}

	void rtpx_core::free_transports(uint16_t local_port)
	{
		for (auto itr = transports_.begin(); itr != transports_.end();)
		{
			if (itr->second->local_port() == local_port)
			{
				itr = transports_.erase(itr);
			}
			else
			{
				itr++;
			}
		}
	}

	bool rtpx_core::is_port_idle(uint16_t local_port)
	{
		for (auto itr = media_streams_.begin(); itr != media_streams_.end(); itr++)
		{
			if (itr->second->local_rtp_port() == local_port || itr->second->local_rtcp_port() == local_port)
			{
				return false;
			}
		}
		return true;
	}
	*/
}

