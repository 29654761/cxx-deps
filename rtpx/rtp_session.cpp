/**
 * @file rtp_session.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#include "rtp_session.h"
#include "rtpx_core.h"
#include <sys2/util.h>

namespace rtpx {

	rtp_session::rtp_session(asio::io_context& ioc,spdlogger_ptr log, bool stun, bool is_port_mux)
		:ioc_(ioc), timer_(ioc), active_(false)
	{
		log_ = log;
		stun_ = stun;
		is_port_mux_ = is_port_mux;
		cname_ = sys::util::random_string(20);
		ice_ufrag_ = sys::util::random_string(5);
		ice_pwd_ = sys::util::random_string(25);
		ice_options_ = "trickle";
	}

	rtp_session::~rtp_session()
	{
	}

	void rtp_session::start()
	{
		bool expected = false;
		if (!active_.compare_exchange_strong(expected, true))
		{
			return;
		}

		auto self = shared_from_this();
		timer_count_ = 0;
		timer_interval_ = 1;
		timer_.expires_after(std::chrono::seconds(timer_interval_));
		timer_.async_wait(std::bind(&rtp_session::handle_timer, self, std::placeholders::_1));
	}

	void rtp_session::stop()
	{
		active_ = false;
		std::error_code ec;
		timer_.cancel(ec);
		
		clear_media_streams();
	}

	bool rtp_session::create_offer(sdp& offer)
	{
		sdp_role_ = sdp_type_offer;
		setup_ = sdp_setup_actpass;

		offer.attrs.push_back(sdp_pair("msid-semantic", "WMS", ":"));
		offer.attrs.push_back(sdp_pair("extmap-allow-mixed", "", ":"));

		auto ms = media_streams();
		for (auto m : ms)
		{
			sdp_media sdpm;
			if (!m->create_offer(sdpm))
				return false;

			offer.medias.push_back(sdpm);
		}

		return true;
	}

	bool rtp_session::create_answer(sdp& answer)
	{
		if (sdp_role_ != sdp_type_answer)
		{
			if (log_)
			{
				log_->error("Create asnwer failed: The SDP role must be answer,but now is {}", sdp_role_)->flush();
			}
			return false;
		}
		answer.attrs.push_back(sdp_pair("msid-semantic", "WMS", ":"));
		answer.attrs.push_back(sdp_pair("extmap-allow-mixed", "", ":"));

		auto ms = media_streams();
		for (auto m : ms)
		{
			sdp_media sdpm;
			if (!m->create_answer(sdpm))
				return false;

			answer.medias.push_back(sdpm);
		}

		start();

		return true;
	}

	bool rtp_session::set_remote_sdp(const sdp& remote_sdp, sdp_type_t type)
	{
		remote_ice_lite_ = remote_sdp.has_attribute("ice-lite");


		if (type == sdp_type_offer)
		{
			sdp_role_ = sdp_type_answer;

			std::map<std::string, uint16_t> sctp_streams;//For resume sctp connection
			{
				std::unique_lock<std::shared_mutex> lk(media_streams_mutex_);
				for (auto& m : media_streams_)
				{
					if (m->media_type() == media_type_application)
					{
						sctp_streams.insert(std::make_pair(m->mid(), m->sctp_stream_id()));
					}
					m->close();
				}
				media_streams_.clear();
			}

			
			for (const auto& m : remote_sdp.medias)
			{
				rtp_trans_mode_t tm = rtp_trans_mode_inactive;
				if (m.trans_mode == rtp_trans_mode_sendrecv) {
					tm = rtp_trans_mode_sendrecv;
				}
				else if (m.trans_mode == rtp_trans_mode_sendonly)
				{
					tm = rtp_trans_mode_recvonly;
				}
				else if (m.trans_mode == rtp_trans_mode_recvonly)
				{
					tm = rtp_trans_mode_sendonly;
				}
				auto ms=create_media_stream(m.media_type, m.mid, m.is_security(),m.msid_sid,m.msid_tid,tm,m.rtcp_mux);
				if (!ms)
				{
					return false;
				}
				else
				{
					if (!ms->set_remote_sdp(m, type)) {
						return false;
					}

					// Resume sctp
					if (ms->media_type() == media_type_application)
					{
						auto itr = sctp_streams.find(ms->mid());
						if (itr != sctp_streams.end())
						{
							ms->set_sctp_stream_id(itr->second);
						}
					}
				}
			}
		}
		else if (type == sdp_type_answer)
		{
			if (sdp_role_ != sdp_type_offer)
			{
				if (log_)
				{
					log_->error("Set remote SDP failed: The SDP role must be offer,but now is {}", sdp_role_)->flush();
				}
				return false;
			}

			for (auto itr = media_streams_.begin(); itr != media_streams_.end();)
			{
				auto itr2 = std::find_if(remote_sdp.medias.begin(), remote_sdp.medias.end(), [itr](const sdp_media& a) {
					return a.mid == (*itr)->mid();
				});

				if (itr2 != remote_sdp.medias.end()) 
				{
					if ((*itr)->set_remote_sdp(*itr2, type)) 
					{
						itr++;
					}
					else
					{
						(*itr)->close();
						itr = media_streams_.erase(itr);
					}
				}
				else
				{
					(*itr)->close();
					itr=media_streams_.erase(itr);
				}
			}

			start();
		}
		else
		{
			if (log_)
			{
				log_->error("Set remote SDP failed: Unknow SDP type {}", type)->flush();
			}
			return false;
		}

		media_stream_ptr fst_ms;
		{
			std::shared_lock<std::shared_mutex> lk(media_streams_mutex_);
			auto itr = media_streams_.begin();
			if (itr != media_streams_.end())
			{
				fst_ms = (*itr);
			}
		}
		if (!fst_ms)
		{
			if (log_)
			{
				log_->error("Set remote SDP failed: No media streams.")->flush();
			}
			return false;
		}

		setup_ = fst_ms->local_setup();
		dtls_role_ = dtls_role(fst_ms->remote_setup());
		ice_role_ = stun_ice_role();



		auto tps = transports();
		for (auto tp : tps)
		{
			tp->set_dtls_role(dtls_role_);
			tp->set_ice_role(ice_role_);
		}
		return true;
	}

	media_stream_ptr rtp_session::create_media_stream(media_type_t mt, const std::string& mid,bool security, const std::string& ms_stream_id, const std::string& ms_track_id, rtp_trans_mode_t trans_mode, bool rtcp_mux)
	{
		transport_ptr rtp, rtcp;
		if (!create_transport_pair(security,rtcp_mux, rtp, rtcp)) {
			return nullptr;
		}

		return create_media_stream(mt, mid, security, rtp, rtcp, ms_stream_id, ms_track_id,trans_mode);
	}

	media_stream_ptr rtp_session::create_media_stream(media_type_t mt, const std::string& mid,bool security, custom_transport::send_hander sender, const std::string& ms_stream_id, const std::string& ms_track_id, rtp_trans_mode_t trans_mode, bool rtcp_mux)
	{
		transport_ptr rtp, rtcp;
		if (!create_transport_pair(security, rtcp_mux,sender, rtp, rtcp)) {
			return nullptr;
		}

		return create_media_stream(mt, mid, security, rtp, rtcp, ms_stream_id, ms_track_id, trans_mode);
	}

	bool rtp_session::remove_media_stream_by_mid(const std::string& mid)
	{
		media_stream_ptr ms;
		{
			std::unique_lock<std::shared_mutex> lock(media_streams_mutex_);
			auto itr = std::find_if(media_streams_.begin(), media_streams_.end(), [&mid](const media_stream_ptr a) {
				return a->mid() == mid;
			});
			if (itr != media_streams_.end())
			{
				ms = *itr;
				(*itr)->close();
				media_streams_.erase(itr);
			}
		}

		try_remove_transport(ms->local_rtp_port());
		if (ms->local_rtp_port()!= ms->local_rtcp_port())
		{
			try_remove_transport(ms->local_rtcp_port());
		}

		return true;
	}

	std::vector<media_stream_ptr> rtp_session::media_streams() const
	{
		std::shared_lock<std::shared_mutex> lock(media_streams_mutex_);
		return media_streams_;
	}

	media_stream_ptr rtp_session::get_media_stream_by_mid(const std::string& mid)
	{
		std::shared_lock<std::shared_mutex> lock(media_streams_mutex_);
		for (auto itr = media_streams_.begin(); itr != media_streams_.end(); itr++)
		{
			if ((*itr)->mid() == mid)
			{
				return *itr;
			}
		}
		return nullptr;
	}

	media_stream_ptr rtp_session::get_media_stream_by_msid(const std::string& sid, const std::string& tid)
	{
		std::shared_lock<std::shared_mutex> lock(media_streams_mutex_);
		for (auto itr = media_streams_.begin(); itr != media_streams_.end(); itr++)
		{
			if (sid.size() > 0 && sid != "-")
			{
				if ((*itr)->msid_sid() != sid)
					continue;
			}
			if (tid.size() > 0 && tid != "-")
			{
				if ((*itr)->msid_tid() != tid)
					continue;
			}

			return (*itr);
		}
		return nullptr;
	}

	media_stream_ptr rtp_session::get_media_stream_by_remote_ssrc(uint32_t ssrc)
	{
		std::shared_lock<std::shared_mutex> lock(media_streams_mutex_);
		for (auto itr = media_streams_.begin(); itr != media_streams_.end(); itr++)
		{
			if ((*itr)->remote_rtp_ssrc() == ssrc||(*itr)->remote_rtx_ssrc()==ssrc)
			{
				return *itr;
			}
		}
		return nullptr;
	}

	media_stream_ptr rtp_session::get_media_stream_by_local_ssrc(uint32_t ssrc)
	{
		std::shared_lock<std::shared_mutex> lock(media_streams_mutex_);
		for (auto itr = media_streams_.begin(); itr != media_streams_.end(); itr++)
		{
			if ((*itr)->local_rtp_ssrc() == ssrc || (*itr)->local_rtx_ssrc() == ssrc)
			{
				return *itr;
			}
		}
		return nullptr;
	}

	void rtp_session::clear_media_streams()
	{
		{
			std::unique_lock<std::shared_mutex> lock(media_streams_mutex_);
			for (auto m : media_streams_)
			{
				m->close();
			}
			media_streams_.clear();
		}
		{
			std::unique_lock<std::shared_mutex> lock(transports_mutex_);
			for (auto& tp : transports_)
			{
				tp->close();
			}
			transports_.clear();
		}
	}

	void rtp_session::require_keyframe()
	{
		auto ms = media_streams();
		for (auto m : ms)
		{
			if (m->media_type() == media_type_video)
			{
				uint32_t ssrc_media = m->remote_rtp_ssrc();
				m->send_rtcp_keyframe(ssrc_media);
			}
		}
	}

	bool rtp_session::require_keyframe(const std::string& mid)
	{
		auto ms = media_streams();
		for (auto m : ms)
		{
			if (m->media_type() == media_type_video&&m->mid()==mid)
			{
				uint32_t ssrc_media = m->remote_rtp_ssrc();
				m->send_rtcp_keyframe(ssrc_media);
				return true;
			}
		}
		return false;
	}

	media_stream_ptr rtp_session::create_media_stream(media_type_t mt, const std::string& mid,bool security, transport_ptr rtp, transport_ptr rtcp,
		const std::string& ms_stream_id, const std::string& ms_track_id, rtp_trans_mode_t trans_mode)
	{
		std::unique_lock<std::shared_mutex> lock(media_streams_mutex_);

		auto itr=std::find_if(media_streams_.begin(), media_streams_.end(), [&mid](const media_stream_ptr& a) {
			return a->mid() == mid;
		});
		if (itr != media_streams_.end())
		{
			if (log_)
			{
				log_->error("Create media stream failed: mid conflict. mid={}",mid)->flush();
			}
			return nullptr;
		}


		auto ms = std::make_shared<media_stream>(ioc_, mt,trans_mode, rtp, rtcp,log_);
		ms->set_mid(mid);
		ms->set_security(security);
		if (ms_stream_id.size() > 0) {
			ms->set_msid_sid(ms_stream_id);
		}
		if (ms_track_id.size() > 0) {
			ms->set_msid_tid(ms_track_id);
		}
		ms->set_local_ice_options(ice_ufrag_, ice_pwd_, ice_options_);
		ms->set_cname(cname_);
		auto self = shared_from_this();
		ms->on_frame = std::bind(&rtp_session::handle_ms_frame, self, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		ms->on_receive_require_keyframe = std::bind(&rtp_session::handle_ms_request_keyframe, self, std::placeholders::_1);
		if (on_send_require_keyframe)
		{
			ms->on_send_require_keyframe = on_send_require_keyframe;
		}

		
		media_streams_.push_back(ms);
		return ms;
	}

	transport_ptr rtp_session::create_transport(uint16_t port, bool security)
	{
		auto tp = get_transport(port);
		if(tp) {
			return tp; // already exists
		}

		std::unique_lock<std::shared_mutex> lock(transports_mutex_);

		auto itr = std::find_if(transports_.begin(), transports_.end(), [port](const transport_ptr& a) {
			return a->local_port() == port;
		});
		if (itr != transports_.end()) {
			return (*itr);
		}

		tp = std::make_shared<udp_transport>(ioc_,port, log_);
		tp->enable_dtls(security);
		setup_transport(tp);
		if (!tp->open()) {
			return nullptr;
		}
		transports_.push_back(tp);

		return tp;
	}

	transport_ptr rtp_session::create_transport(uint16_t port, bool security, custom_transport::send_hander sender)
	{
		auto tp = get_transport(port);
		if (tp) {
			return tp; // already exists
		}

		std::unique_lock<std::shared_mutex> lock(transports_mutex_);

		auto itr = std::find_if(transports_.begin(), transports_.end(), [port](const transport_ptr& a) {
			return a->local_port() == port;
		});
		if (itr != transports_.end()) {
			return (*itr);
		}

		tp = std::make_shared<custom_transport>(ioc_, port, sender, log_);
		tp->enable_dtls(security);
		setup_transport(tp);
		if (!tp->open()) {
			return nullptr;
		}
		transports_.push_back(tp);
		return tp;
	}

	void rtp_session::setup_transport(transport_ptr tp)
	{
		auto self = shared_from_this();
		tp->on_rtp_packet = std::bind(&rtp_session::handle_rtp_packet, self, std::placeholders::_1, std::placeholders::_2);
		tp->on_dtls_completed = std::bind(&rtp_session::handle_dtls_completed, self, tp->local_port(), std::placeholders::_1);
		tp->on_stun_completed = std::bind(&rtp_session::handle_stun_completed, self,tp->local_port(), std::placeholders::_1);
		tp->on_rtcp_app = std::bind(&rtp_session::handle_rtcp_app, self, std::placeholders::_1, std::placeholders::_2);
		tp->on_rtcp_bye = std::bind(&rtp_session::handle_rtcp_bye, self, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		tp->on_rtcp_sr = std::bind(&rtp_session::handle_rtcp_sr, self, std::placeholders::_1, std::placeholders::_2);
		tp->on_rtcp_rr = std::bind(&rtp_session::handle_rtcp_rr, self, std::placeholders::_1, std::placeholders::_2);
		tp->on_rtcp_sdes = std::bind(&rtp_session::handle_rtcp_sdes, self, std::placeholders::_1, std::placeholders::_2);
		tp->on_rtcp_fb = std::bind(&rtp_session::handle_rtcp_fb, self, std::placeholders::_1, std::placeholders::_2);

		tp->on_sctp_connected = std::bind(&rtp_session::handle_sctp_connected, self, tp->local_port());
		tp->on_sctp_disconnected = std::bind(&rtp_session::handle_sctp_disconnected, self, tp->local_port());
		tp->on_sctp_heartbeat = std::bind(&rtp_session::handle_sctp_heartbeat, self, tp->local_port());
		tp->on_sctp_stream_opened = std::bind(&rtp_session::handle_sctp_stream_opened, self, tp->local_port(), std::placeholders::_1);
		tp->on_sctp_stream_closed = std::bind(&rtp_session::handle_sctp_stream_closed, self, tp->local_port(), std::placeholders::_1);
		tp->on_sctp_stream_message = std::bind(&rtp_session::handle_sctp_stream_message, self, tp->local_port(), std::placeholders::_1, std::placeholders::_2);

	}


	transport_ptr rtp_session::get_transport(uint16_t port) const
	{
		std::shared_lock<std::shared_mutex> lock(transports_mutex_);
		auto itr = std::find_if(transports_.begin(), transports_.end(), [port](const transport_ptr& a) {
			return a->local_port() == port;
		});

		if (itr == transports_.end())
		{
			return nullptr;
		}
		return *itr;
	}

	bool rtp_session::try_remove_transport(uint16_t port)
	{
		std::shared_lock<std::shared_mutex> lock(transports_mutex_);
		auto itr = std::find_if(transports_.begin(), transports_.end(), [port](const transport_ptr& a) {
			return a->local_port() == port;
		});
		if(itr == transports_.end())
		{
			return false;
		}

		if (itr->use_count() > 1)
		{
			return false;
		}

		(*itr)->close();
		transports_.erase(itr);
		return true;
	}


	std::vector<transport_ptr> rtp_session::transports() const
	{
		std::shared_lock<std::shared_mutex> lock(transports_mutex_);
		return transports_;
	}

	bool rtp_session::create_transport_pair(bool security,bool rtcp_mux, transport_ptr& rtp, transport_ptr& rtcp)
	{
		if (is_port_mux_)
		{
			if(mux_port_>0)
			{
				auto tp = create_transport(mux_port_, security);
				if (tp)
				{
					rtp = tp;
					rtcp = rtp;
					return true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				rtpx_core* core = rtpx_core::instance();
				for (int i = 0; i < core->ports().count(); i++)
				{
					uint16_t port = core->ports().get_next_port();
					auto tp = create_transport(port, security);
					if (tp)
					{
						rtp = tp;
						rtcp = rtp;
						mux_port_ = port;
						return true;
					}
				}

				return false;
			}
		}
		else
		{
			rtpx_core* core = rtpx_core::instance();
			if (rtcp_mux)
			{
				for (int i = 0; i < core->ports().count(); i++)
				{
					uint16_t port = core->ports().get_next_port();
					auto tp = create_transport(port, security);
					if (!tp)
					{
						continue;
					}

					rtp = tp;
					rtcp = tp;
					return true;
				}
			}
			else
			{
				for (int i = 0; i < core->ports().count(); i++)
				{
					uint16_t port = core->ports().get_next_even_port();
					auto tp = create_transport(port, security);
					if (!tp)
					{
						continue;
					}
					auto tp2 = create_transport(port + 1, security);
					if (!tp2)
					{
						tp.reset();
						try_remove_transport(port);
						continue;
					}


					rtp = tp;
					rtcp = tp2;
					return true;
				}
			}
			return false;
		}
	}

	bool rtp_session::create_transport_pair(bool security,bool rtcp_mux, custom_transport::send_hander sender, transport_ptr& rtp, transport_ptr& rtcp)
	{
		if (is_port_mux_)
		{
			if (mux_port_ > 0)
			{
				auto tp = create_transport(mux_port_,security,sender);
				if (tp)
				{
					rtp = tp;
					rtcp = rtp;
					return true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				rtpx_core* core = rtpx_core::instance();
				for (int i = 0; i < core->ports().count(); i++)
				{
					uint16_t port = core->ports().get_next_port();
					auto tp = create_transport(port, security, sender);
					if (tp)
					{
						rtp = tp;
						rtcp = rtp;
						mux_port_ = port;
						return true;
					}
				}

				return false;
			}
		}
		else
		{
			rtpx_core* core = rtpx_core::instance();
			for (int i = 0; i < core->ports().count(); i++)
			{
				uint16_t port = core->ports().get_next_even_port();
				auto tp = create_transport(port, security, sender);
				if (!tp)
				{
					continue;
				}
				auto tp2 = create_transport(port + 1, security, sender);
				if (!tp2)
				{
					tp.reset();
					try_remove_transport(port);
					continue;
				}


				rtp = tp;
				rtcp = tp2;
				return true;
			}

			return false;
		}
	}


	void rtp_session::handle_rtp_packet(packet_ptr pkt, const asio::ip::udp::endpoint& ep)
	{
		auto ms=this->get_media_stream_by_remote_ssrc(pkt->header()->ssrc);
		if (ms)
		{
			ms->receive_rtp_packet(pkt,ep);
		}
	}

	void rtp_session::handle_stun_completed(uint16_t local_port, const asio::ip::udp::endpoint& ep)
	{
		update_.update();
		auto ms = media_streams();
		for (auto m : ms)
		{
			if (m->local_rtp_port() == local_port || m->local_rtcp_port() == local_port)
			{
				m->receive_stun_completed(ep);
			}
		}
	}

	void rtp_session::handle_dtls_completed(uint16_t local_port, dtls_role_t dtls_role)
	{
		update_.update();
		auto ms = media_streams();
		for (auto m : ms)
		{
			if (m->local_rtp_port() == local_port||m->local_rtcp_port()==local_port)
			{
				m->receive_dtls_completed(dtls_role);
			}
		}

		if (on_connected)
		{
			on_connected();
		}
	}



	void rtp_session::handle_rtcp_app(const rtcp_app* app, const asio::ip::udp::endpoint& endpoint)
	{
		auto ms = this->get_media_stream_by_remote_ssrc(app->ssrc);
		if (ms)
		{
			ms->receive_rtcp_app(app,endpoint);
		}
	}

	void rtp_session::handle_rtcp_bye(uint32_t ssrc, const char* reason, const asio::ip::udp::endpoint& endpoint)
	{
		auto ms = this->get_media_stream_by_remote_ssrc(ssrc);
		if (ms)
		{
			ms->receive_rtcp_bye(ssrc,reason,endpoint);
		}

		if (on_disconnected)
		{
			on_disconnected();
		}
	}

	void rtp_session::handle_rtcp_sr(const rtcp_sr* sr, const asio::ip::udp::endpoint& endpoint)
	{
		update_.update();
		auto ms = this->get_media_stream_by_remote_ssrc(sr->ssrc);
		if (ms)
		{
			ms->receive_rtcp_sr(sr,endpoint);
		}
	}

	void rtp_session::handle_rtcp_rr(const rtcp_rr* rr, const asio::ip::udp::endpoint& endpoint)
	{
		update_.update();
		for (unsigned int i = 0; i < rr->header.common.count; i++)
		{
			auto ms = this->get_media_stream_by_local_ssrc(rr->reports[i].ssrc);
			if (ms)
			{
				ms->receive_rtcp_rr(rr->reports+i, endpoint);
			}
		}
	}

	void rtp_session::handle_rtcp_sdes(const rtcp_sdes_entry* entry, const asio::ip::udp::endpoint& endpoint)
	{
		auto ms = this->get_media_stream_by_local_ssrc(entry->id);
		if (ms)
		{
			ms->receive_rtcp_sdes(entry,endpoint);
		}
	}

	void rtp_session::handle_rtcp_fb(const rtcp_fb* fb, const asio::ip::udp::endpoint& endpoint)
	{
		auto ms = this->get_media_stream_by_local_ssrc(fb->ssrc_media);
		if (ms)
		{
			ms->receive_rtcp_feedback(fb,endpoint);
		}
	}

	void rtp_session::handle_timer(const std::error_code& ec)
	{
		if (ec||!active_)
			return;


		auto ms=media_streams();
		if (stun_)
		{
			for (auto m : ms)
			{
				if (!m->send_stun_request())
				{
					m->detect_candidates();
				}
			}
		}

		for (auto m : ms)
		{
			if (m->media_type() == media_type_audio || m->media_type() == media_type_video) {
				m->send_rtcp_report();
			}
		}

		if (is_timeout())
		{
			if (on_disconnected)
			{
				on_disconnected();
			}
		}

		if (timer_count_ < 5)
		{
			timer_count_++;
		}
		else
		{
			timer_interval_ = 5;
		}

		auto self = shared_from_this();
		timer_.expires_after(std::chrono::seconds(timer_interval_));
		timer_.async_wait(std::bind(&rtp_session::handle_timer, self, std::placeholders::_1));
	}


	void rtp_session::handle_ms_frame(const std::string& mid, const sdp_format& fmt, const av_frame_t& frame)
	{
		if (on_frame)
		{
			on_frame(mid, fmt, frame);
		}
	}

	void rtp_session::handle_ms_request_keyframe(const std::string& mid)
	{
		if (on_receive_require_keyframe)
		{
			on_receive_require_keyframe(mid);
		}
	}

	void rtp_session::handle_sctp_connected(uint16_t local_port)
	{
		auto ms=media_streams();
		for (auto m : ms)
		{
			if (m->media_type() == media_type_application && m->local_rtp_port() == local_port)
			{
				m->sctp_connected();

				if (m->sdp_role() == sdp_type_offer) {
					m->sctp_open_stream();
				}
			}
		}
	}

	void rtp_session::handle_sctp_disconnected(uint16_t local_port)
	{
		auto ms = media_streams();
		for (auto m : ms)
		{
			if (m->media_type() == media_type_application && m->local_rtp_port() == local_port)
			{
				m->sctp_disconnected();
			}
		}
	}

	void rtp_session::handle_sctp_heartbeat(uint16_t local_port)
	{
		auto ms = media_streams();
		for (auto m : ms)
		{
			if (m->media_type() == media_type_application && m->local_rtp_port() == local_port)
			{
				m->sctp_heartbeat();
			}
		}
	}


	void rtp_session::handle_sctp_stream_opened(uint16_t local_port,uint16_t stream_id)
	{
		if (on_data_channel_opened) 
		{
			auto ms = media_streams();
			for (auto m : ms)
			{
				if (m->media_type() == media_type_application && m->local_rtp_port() == local_port)
				{
					m->set_sctp_stream_id(stream_id);
					on_data_channel_opened(m->mid());
					break;
				}
			}
			
		}
	}

	void rtp_session::handle_sctp_stream_closed(uint16_t local_port,uint16_t stream_id)
	{
		if (on_data_channel_closed) 
		{
			auto ms = media_streams();
			for (auto m : ms)
			{
				if (m->media_type() == media_type_application && m->sctp_stream_id() == stream_id)
				{
					on_data_channel_closed(m->mid());
					break;
				}
			}
		}
	}

	void rtp_session::handle_sctp_stream_message(uint16_t local_port,uint16_t stream_id, const std::vector<uint8_t>& message)
	{
		if (on_data_channel_message) 
		{
			auto ms = media_streams();
			for (auto m : ms)
			{
				if (m->media_type() == media_type_application && m->sctp_stream_id() == stream_id)
				{
					on_data_channel_message(m->mid(),message);
					break;
				}
			}
		}
	}

	dtls_role_t rtp_session::dtls_role(sdp_setup_t remote_setup)
	{
		if (setup_ == sdp_setup_active && (remote_setup == sdp_setup_actpass || remote_setup == sdp_setup_passive))
		{
			return dtls_role_client;
		}
		else if (setup_ == sdp_setup_passive && (remote_setup == sdp_setup_actpass || remote_setup == sdp_setup_active))
		{
			return dtls_role_server;
		}
		else if (setup_ == sdp_setup_actpass && (remote_setup == sdp_setup_active))
		{
			return dtls_role_server;
		}
		else if (setup_ == sdp_setup_actpass && (remote_setup == sdp_setup_passive))
		{
			return dtls_role_client;
		}
		else
		{
			return dtls_role_client;
		}
	}

	stun_ice_role_t rtp_session::stun_ice_role()
	{
		if (remote_ice_lite_)
			return stun_ice_controlling;

		if (sdp_role_ == sdp_type_offer)
			return stun_ice_controlling;
		else
			return stun_ice_controlled;
	}
}

