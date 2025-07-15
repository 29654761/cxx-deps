/**
 * @file rtp_session.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#include "rtp_session.h"
#include <sys2/util.h>

namespace litertp
{
	rtp_session::rtp_session(bool webrtc, bool stun, bool ice_lite)
		:active_(false), signal_times_(0)
	{
		cname_= sys::util::random_string(20);

		webrtc_ = webrtc;
		stun_ = stun;
		ice_lite_ = ice_lite;

		if (webrtc_)
		{
			ice_ufrag_ = sys::util::random_string(5);
			ice_pwd_ = sys::util::random_string(25);
			ice_options_ = "trickle";
		}

		
	}

	rtp_session::~rtp_session()
	{
		stop();
	}

	bool rtp_session::start()
	{
		std::lock_guard<std::mutex> lk(mutex_);
		bool expected = false;
		signal_times_ = 0;
		signal_.notify();
		if(!active_.compare_exchange_strong(expected, true))
			return true;
		active_ = true;
		timer_ = std::make_shared<std::thread>(&rtp_session::run, this);
		return true;
	}

	void rtp_session::stop()
	{
		std::lock_guard<std::mutex> lk(mutex_);
		active_ = false;
		signal_.notify();
		if (timer_) {
			timer_->join();
			timer_.reset();
		}

		clear();
	}

	void rtp_session::clear()
	{
		clear_media_streams();
		clear_transports();
	}

	media_stream_ptr rtp_session::create_media_stream(const std::string& mid,media_type_t mt, uint32_t ssrc, const char* local_address, int local_rtp_port, int local_rtcp_port, bool reuse_port)
	{
		std::unique_lock<std::shared_mutex>lk(streams_mutex_);
		
		auto itr = std::find_if(streams_.begin(), streams_.end(), [&mid](auto& item) {
			return item->mid() == mid;
			});

		if (itr != streams_.end())
		{
			return *itr;
		}

		auto tp = create_udp_transport(local_rtp_port,reuse_port);
		if (!tp)
		{
			return nullptr;
		}
		bool reuse_rtcp_port = reuse_port;
		if (local_rtp_port == local_rtcp_port) {
			reuse_rtcp_port = true;
		}
		auto tp2 = create_udp_transport(local_rtcp_port, reuse_rtcp_port);
		if (!tp2)
		{
			remote_transport(local_rtp_port);
			return nullptr;
		}

		media_stream_ptr m = std::make_shared<media_stream>(mt, ssrc, mid, cname_,ice_options_, ice_ufrag_, ice_pwd_, local_address, tp, tp2);
		
		if (mt != media_type_application)
		{
			m->on_frame_received.add(s_litertp_on_frame, this);
		}
		else
		{
			m->on_data_channel_open.add(s_litertp_on_data_channel_open, this);
			m->on_data_channel_close.add(s_litertp_on_data_channel_close, this);
			m->on_data_channel_message.add(s_litertp_on_data_channel_message, this);
		}
		m->on_received_require_keyframe.add(s_litertp_on_received_require_keyframe, this);

		streams_.push_back(m);

		return m;
	}

	media_stream_ptr rtp_session::create_media_stream(const std::string& mid, media_type_t mt, uint32_t ssrc, const char* local_address, port_range_ptr ports, bool rtcp_mux)
	{
		for (int i = 0; i < ports->count(); i++)
		{
			if (rtcp_mux)
			{
				int rtp_port = ports->get_next_port();
				auto m=create_media_stream(mid,mt, ssrc, local_address, rtp_port, rtp_port,false);
				if (m)
				{
					return m;
				}
			}
			else 
			{
				int rtp_port = ports->get_next_even_port();
				int rtcp_port = rtp_port + 1;
				auto m = create_media_stream(mid,mt, ssrc, local_address, rtp_port, rtcp_port, false);
				if (m)
				{
					return m;
				}
			}
		}

		return nullptr;
	}

	media_stream_ptr rtp_session::create_media_stream(const std::string& mid, media_type_t mt, uint32_t ssrc, int port, litertp_on_send_packet on_send_packet, void* ctx,bool reuse_port)
	{
		std::unique_lock<std::shared_mutex>lk(streams_mutex_);

		
		auto itr = std::find_if(streams_.begin(), streams_.end(), [&mid](auto& item) {
			return item->mid() == mid;
			});
		if (itr != streams_.end())
		{
			return *itr;
		}
		

		auto tp = create_custom_transport(port, on_send_packet, ctx, reuse_port);
		if (!tp)
		{
			return nullptr;
		}
		
		media_stream_ptr m = std::make_shared<media_stream>(mt, ssrc, mid, cname_,ice_options_, ice_ufrag_, ice_pwd_,"0.0.0.0", tp, tp,true);

		if (mt != media_type_application)
		{
			m->on_frame_received.add(s_litertp_on_frame, this);
		}
		else
		{
			m->on_data_channel_open.add(s_litertp_on_data_channel_open, this);
			m->on_data_channel_close.add(s_litertp_on_data_channel_close, this);
			m->on_data_channel_message.add(s_litertp_on_data_channel_message, this);
		}
		
		m->on_received_require_keyframe.add(s_litertp_on_received_require_keyframe, this);
		streams_.push_back(m);

		return m;
	}

	media_stream_ptr rtp_session::create_application_stream(const std::string& mid, const std::string& protos, int port)
	{
		std::unique_lock<std::shared_mutex>lk(streams_mutex_);

		auto itr = std::find_if(streams_.begin(), streams_.end(), [&mid](auto& item) {
			return item->mid() == mid;
		});

		if (itr != streams_.end())
		{
			return *itr;
		}

		auto tp = create_udp_transport(port, false);
		if (!tp)
		{
			return nullptr;
		}
		

		media_stream_ptr m = std::make_shared<media_stream>(mid,protos, ice_options_, ice_ufrag_, ice_pwd_, tp);
		m->on_data_channel_open.add(s_litertp_on_data_channel_open, this);
		m->on_data_channel_close.add(s_litertp_on_data_channel_close, this);
		m->on_data_channel_message.add(s_litertp_on_data_channel_message, this);

		streams_.push_back(m);

		return m;
	}

	media_stream_ptr rtp_session::create_application_stream(const std::string& mid, const std::string& protos, port_range_ptr ports)
	{
		for (int i = 0; i < ports->count(); i++)
		{
			int port = ports->get_next_port();
			auto m = create_application_stream(mid, protos,port);
			if (m)
			{
				return m;
			}
		}

		return nullptr;
	}


	media_stream_ptr rtp_session::get_media_stream(media_type_t mt)
	{
		std::shared_lock<std::shared_mutex>lk(streams_mutex_);
		auto itr = std::find_if(streams_.begin(), streams_.end(), [mt](auto& item) {
			return item->media_type() == mt;
			});
		if (itr == streams_.end())
		{
			return nullptr;
		}

		return *itr;
	}

	media_stream_ptr rtp_session::get_media_stream(const std::string& mid)
	{
		std::shared_lock<std::shared_mutex>lk(streams_mutex_);
		auto itr = std::find_if(streams_.begin(), streams_.end(), [&mid](auto& item) {
			return item->mid() == mid;
			});
		if (itr == streams_.end())
		{
			return nullptr;
		}

		return *itr;
	}

	std::vector<media_stream_ptr> rtp_session::get_media_streams()
	{
		std::vector<media_stream_ptr> vec;
		std::shared_lock<std::shared_mutex> lk(streams_mutex_);
		if (streams_.size() > 0)
		{
			vec.reserve(streams_.size());
			for (auto itr = streams_.begin();itr!=streams_.end();itr++)
			{
				vec.push_back(*itr);
			}
		}
		return vec;
	}

	bool rtp_session::remove_media_stream(const std::string& mid)
	{
		bool ret = false;
		{
			std::unique_lock<std::shared_mutex>lk(streams_mutex_);
			auto itr = std::find_if(streams_.begin(), streams_.end(), [&mid](auto& item) {
				return item->mid() == mid;
			});
			if (itr != streams_.end())
			{
				streams_.erase(itr);
				ret = true;
			}
		}

		if (ret) {
			clear_no_ref_transports();
		}
		return ret;
	}

	void rtp_session::clear_media_streams()
	{
		{
			std::unique_lock<std::shared_mutex>lk(streams_mutex_);
			streams_.clear();
		}
		clear_no_ref_transports();
	}


	void rtp_session::clear_local_candidates()
	{
		auto ms = get_media_streams();
		for (auto m : ms)
		{
			m->clear_local_candidates();
		}
	}

	void rtp_session::clear_remote_candidates()
	{
		auto ms = get_media_streams();
		for (auto m : ms)
		{
			m->clear_remote_candidates();
		}
	}

	bool rtp_session::receive_rtp_packet(int port, const uint8_t* rtp_packet, int size)
	{
		auto tp = get_transport(port);
		if (!tp)
		{
			return false;
		}
		return tp->receive_rtp_packet(rtp_packet, size);
	}

	bool rtp_session::receive_rtcp_packet(int port, const uint8_t* rtcp_packet, int size)
	{
		auto tp = get_transport(port);
		if (!tp)
		{
			return false;
		}
		return tp->receive_rtcp_packet(rtcp_packet, size);
	}

	litertp::sdp rtp_session::create_offer()
	{
		litertp::sdp sdp;
		if (webrtc_)
		{
			sdp.attrs.push_back(sdp_pair("msid-semantic", "WMS", ":"));
		}
		if (ice_lite_)
		{
			sdp.attrs.push_back(litertp::sdp_pair("ice-lite","",":"));
		}
		//sdp.bundle = local_group_bundle();
		auto streams=get_media_streams();
		for (auto stream : streams)
		{
			stream->set_sdp_type(sdp_type_offer);
			auto sdpm = stream->get_local_sdp();
			sdp.medias.push_back(sdpm);
		}

		return sdp;
	}

	litertp::sdp rtp_session::create_answer()
	{
		litertp::sdp sdp;
		if (webrtc_)
		{
			sdp.attrs.push_back(sdp_pair("msid-semantic", "WMS", ":"));
			sdp.attrs.push_back(sdp_pair("extmap-allow-mixed", "",":"));
		}
		//sdp.bundle = local_group_bundle();
		auto streams = get_media_streams();
		for (auto stream : streams)
		{
			stream->set_sdp_type(sdp_type_answer);
			if(!stream->negotiate())
			{
				continue;
			}

			auto sdpm_local = stream->get_local_sdp();
			//auto sdpm_remote = stream->get_remote_sdp();

			if (!stream->exchange())
			{
				continue;
			}

			sdp.medias.push_back(sdpm_local);
			/*
			sdpm_remote.rtcp_address = sdpm_local.rtcp_address;
			sdpm_remote.rtcp_address_type = sdpm_local.rtcp_address_type;
			sdpm_remote.rtcp_port = sdpm_local.rtcp_port;
			sdpm_remote.rtcp_mux = sdpm_local.rtcp_mux;
			sdpm_remote.rtp_address = sdpm_local.rtp_address;
			sdpm_remote.rtp_address_type = sdpm_local.rtp_address_type;
			sdpm_remote.rtp_network_type = sdpm_local.rtp_network_type;
			sdpm_remote.rtp_port = sdpm_local.rtp_port;

			sdpm_remote.candidates = sdpm_local.candidates;
			sdpm_remote.ice_ufrag = sdpm_local.ice_ufrag;
			sdpm_remote.ice_pwd = sdpm_local.ice_pwd;
			sdpm_remote.ice_options = sdpm_local.ice_options;
			sdpm_remote.fingerprint_sign = sdpm_local.fingerprint_sign;
			sdpm_remote.fingerprint = sdpm_local.fingerprint;
			sdpm_remote.setup = sdpm_local.setup;
			sdpm_remote.trans_mode = sdpm_local.trans_mode;
			sdpm_remote.ssrcs = sdpm_local.ssrcs;
			sdpm_remote.ssrc_group = sdpm_local.ssrc_group;
			sdpm_remote.msid = sdpm_local.msid;
			sdp.medias.push_back(sdpm_remote);
			*/


		}


		return sdp;
	}

	bool rtp_session::set_remote_sdp(const litertp::sdp& sdp, sdp_type_t sdp_type)
	{
		for (auto& m : sdp.medias)
		{
			auto s=get_media_stream(m.mid);
			if (s)
			{
				if (!s->has_remote_sdp())
				{
					s->set_remote_ice_lite(sdp.has_attribute("ice-lite"));
					if (!s->set_remote_sdp(m, sdp_type))
					{
						return false;
					}
				}
			}
		}

		signal_times_ = 0;
		signal_.notify();

		return true;
	}

	bool rtp_session::set_local_sdp(const litertp::sdp& sdp)
	{
		clear();

		for (auto& m : sdp.medias)
		{
			auto s = create_media_stream(m.mid,m.media_type,m.get_default_ssrc(), m.rtp_address.c_str(), m.rtp_port, m.rtcp_port,true);
			if (!s)
			{
				clear();
				return false;
			}

			for (auto fmt : m.rtpmap)
			{
				if (m.media_type == media_type_audio)
				{
					s->add_local_audio_track(fmt.second.codec, fmt.second.pt, fmt.second.frequency, fmt.second.channels);
				}
				else if (m.media_type == media_type_video)
				{
					s->add_local_video_track(fmt.second.codec, fmt.second.pt, fmt.second.frequency, fmt.second.rtcp_fb.size()>0);
				}
			}
			if (m.is_security()) {
				s->enable_dtls();
			}
			else {
				s->disable_dtls();
			}
			s->set_local_sdp(m);
		}

		return true;
	}

	void rtp_session::require_keyframe()
	{
		auto streams = get_media_streams();
		for (auto stream : streams)
		{
			if (stream->media_type() == media_type_video)
			{
				uint32_t ssrc_media=stream->get_remote_ssrc();
				stream->send_rtcp_keyframe(ssrc_media);
				
			}
		}
	}

	bool rtp_session::require_keyframe(const std::string& mid)
	{
		auto streams = get_media_streams();
		for (auto stream : streams)
		{
			if (stream->media_type() == media_type_video&&stream->mid()==mid)
			{
				uint32_t ssrc_media = stream->get_remote_ssrc();
				stream->send_rtcp_keyframe(ssrc_media);
				return true;
			}
		}
		return false;
	}

	int rtp_session::get_rtp_port(media_type_t mt)
	{
		auto ms=get_media_stream(mt);
		if (!ms||!ms->transport_rtp_) {
			return 0;
		}
		return ms->transport_rtp_->port_;
	}
	int rtp_session::get_rtcp_port(media_type_t mt)
	{
		auto ms = get_media_stream(mt);
		if (!ms || !ms->transport_rtcp_) {
			return 0;
		}
		return ms->transport_rtcp_->port_;
	}

	transport_ptr rtp_session::create_udp_transport(int port, bool reuse_port)
	{
		std::unique_lock<std::shared_mutex>lk(transports_mutex_);

		auto itr = transports_.find(port);
		if (itr != transports_.end())
		{
			if (!reuse_port)
			{
				return nullptr;
			}
			transport_udp* p = dynamic_cast<transport_udp*>(itr->second.get());
			if (!p)
			{
				return nullptr;
			}
			return itr->second;
		}

		transport_ptr tp = std::make_shared<transport_udp>(port);
		tp->connected_event_.add(s_transport_connected, this);
		tp->disconnected_event_.add(s_transport_disconnected, this);
		if (!tp->start())
		{
			return nullptr;
		}

		tp->ice_ufrag_local_ = this->ice_ufrag_;
		tp->ice_pwd_local_ = this->ice_pwd_;

		transports_.insert(std::make_pair(port, tp));
		return tp;
	}

	transport_ptr rtp_session::create_custom_transport(int port, litertp_on_send_packet on_send_packet, void* ctx,bool reuse_port)
	{
		std::unique_lock<std::shared_mutex>lk(transports_mutex_);

		auto itr = transports_.find(port);
		if (itr != transports_.end())
		{
			if (!reuse_port)
			{
				return nullptr;
			}
			transport_custom* p = dynamic_cast<transport_custom*>(itr->second.get());
			if (!p)
			{
				return nullptr;
			}
			return itr->second;
		}

		transport_ptr tp = std::make_shared<transport_custom>(port, on_send_packet, ctx);
		tp->disconnected_event_.add(s_transport_disconnected, this);
		if (!tp->start())
		{
			return nullptr;
		}

		transports_.insert(std::make_pair(port, tp));
		return tp;
	}

	transport_ptr rtp_session::get_transport(int port)
	{
		std::shared_lock<std::shared_mutex>lk(transports_mutex_);

		auto itr = transports_.find(port);
		if (itr == transports_.end())
		{
			return nullptr;
		}

		return itr->second;
	}

	void rtp_session::remote_transport(int port)
	{
		std::unique_lock<std::shared_mutex>lk(transports_mutex_);
		transports_.erase(port);
	}

	void rtp_session::clear_no_ref_transports()
	{
		std::unique_lock<std::shared_mutex>lk(transports_mutex_);
		for (auto itr = transports_.begin(); itr != transports_.end();)
		{
			if (itr->second.use_count() <= 1)
			{
				itr=transports_.erase(itr);
			}
			else
			{
				itr++;
			}
		}
	}

	void rtp_session::clear_transports()
	{
		std::unique_lock<std::shared_mutex>lk(transports_mutex_);
		for (auto itr = transports_.begin(); itr != transports_.end(); itr++)
		{
			itr->second->stop();
		}
		transports_.clear();
	}



	void rtp_session::s_litertp_on_frame(void* ctx, const char* mid, uint32_t ssrc, uint16_t pt, int frequency, int channels, const av_frame_t* frame)
	{
		rtp_session* p = (rtp_session*)ctx;
		p->on_frame.invoke(mid,ssrc, pt, frequency, channels, frame);
	}

	void rtp_session::s_litertp_on_received_require_keyframe(void* ctx, uint32_t ssrc, int mode)
	{
		rtp_session* p = (rtp_session*)ctx;
		p->on_received_require_keyframe.invoke(ssrc,mode);

	}

	void rtp_session::s_transport_connected(void* ctx, int port)
	{
		rtp_session* p = (rtp_session*)ctx;
		p->on_connected.invoke();
	}
	void rtp_session::s_transport_disconnected(void* ctx, int port)
	{
		rtp_session* p = (rtp_session*)ctx;
		p->on_disconnected.invoke();
	}

	void rtp_session::s_litertp_on_data_channel_open(void* ctx, const char* mid)
	{
		rtp_session* p = (rtp_session*)ctx;
		p->on_data_channel_open.invoke(mid);
	}

	void rtp_session::s_litertp_on_data_channel_close(void* ctx, const char* mid)
	{
		rtp_session* p = (rtp_session*)ctx;
		p->on_data_channel_close.invoke(mid);
	}

	void rtp_session::s_litertp_on_data_channel_message(void* ctx, const char* mid, const uint8_t* message, size_t size)
	{
		rtp_session* p = (rtp_session*)ctx;
		p->on_data_channel_message.invoke(mid, message, size);
	}


	void rtp_session::run()
	{
		int sleep = 5000;
		int stun_secs = 0;
		int stun_interval = 20000;
		int rtcp_secs = 0;
		int rtcp_interval = 5000;

		while (active_)
		{
			if (signal_times_ >= 5)
			{
				sleep = 5000;
				stun_interval = 20000;
			}
			else
			{
				sleep = 1000;
				stun_interval = 1000;
				stun_secs = 1000;
				signal_times_++;
			}


			signal_.wait(sleep);
			if (!active_)
				break;

			if (rtcp_secs >= rtcp_interval)
			{
				auto ms = get_media_streams();
				for (auto m : ms)
				{
					if (m->media_type() == media_type_audio || m->media_type() == media_type_video) 
					{
						m->run_rtcp_stats();
					}
					if (m->is_timeout())
					{
						this->on_disconnected.invoke();
					}
				}
				rtcp_secs = 0;
			}

			if (stun_)
			{
				if (stun_secs >= stun_interval) 
				{
					auto ms = get_media_streams();
					for (auto m : ms)
					{
						m->run_stun_request();
					}
					stun_secs = 0;
				}

				stun_secs += sleep;
			}

			rtcp_secs += sleep;
			



		}
	}

	bool rtp_session::local_group_bundle()
	{
		auto ms = get_media_streams();
		if (ms.size() <= 1) {
			return false;
		}
		auto itr_begin = ms.begin();
		if (itr_begin == ms.end())
		{
			return false;
		}
		auto sdpm_begin = (*itr_begin)->get_local_sdp();
		for (auto itr = ms.begin(); itr != ms.end(); itr++)
		{
			auto sdpm = (*itr)->get_local_sdp();
			if (sdpm_begin.rtp_port != sdpm.rtp_port)
			{
				return false;
			}
		}

		return true;
	}
}

