/**
 * @file media_stream.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#include "media_stream.h"
#include "log.h"



#include "senders/sender_audio.h"
#include "senders/sender_audio_aac.h"
#include "senders/sender_video_h264.h"
#include "senders/sender_video_h265.h"
#include "senders/sender_video_vp8.h"
#include "senders/sender_ps.h"
#include "senders/sender_rtx.h"

#include "receivers/receiver_audio.h"
#include "receivers/receiver_audio_aac.h"
#include "receivers/receiver_video_h264.h"
#include "receivers/receiver_video_h265.h"
#include "receivers/receiver_video_vp8.h"
#include "receivers/receiver_ps.h"
#include "receivers/receiver_rtx.h"

#include "proto/rtp_source.h"
#include "sdp/fmtp.h"
#include "util/time.h"

#include <sys2/util.h>
#include <sys2/string_util.h>
#include <string.h>

namespace litertp
{

	media_stream::media_stream(media_type_t media_type,uint32_t ssrc, const std::string& mid, const std::string& cname, const std::string& ice_options, const std::string& ice_ufrag, const std::string& ice_pwd,
		const std::string& local_address, transport_ptr transport_rtp, transport_ptr transport_rtcp, bool is_tcp)
	{
		local_sdp_media_.media_type = media_type;
		local_sdp_media_.ice_options = ice_options;
		local_sdp_media_.ice_ufrag = ice_ufrag;
		local_sdp_media_.ice_pwd = ice_pwd;
		local_sdp_media_.mid = mid;
		local_sdp_media_.has_rtcp = true;
		local_sdp_media_.rtcp_address = local_address;
		local_sdp_media_.rtcp_port = transport_rtcp->port_;
		local_sdp_media_.has_rtp_address = true;
		local_sdp_media_.rtp_address = local_address;
		local_sdp_media_.rtp_port = transport_rtp->port_;
		if (is_tcp)
		{
			local_sdp_media_.protos.erase("UDP");
			local_sdp_media_.protos.insert("TCP");
		}
		cname_ = cname;


		local_sdp_media_.rtcp_mux = (transport_rtp == transport_rtcp);

		if (ssrc == 0)
		{
			ssrc = sys::util::random_number<uint32_t>(0x10000, 0xFFFFFFFF);
		}

		ssrc_t ssrct;
		ssrct.ssrc = ssrc;
		ssrct.cname = cname_;
		ssrct.msid = local_sdp_media_.msid;
		local_sdp_media_.ssrcs.push_back(ssrct);

		if (media_type == media_type_video || media_type == media_type_audio)
		{
			//For RTX
			uint32_t ssrc2 = sys::util::random_number<uint32_t>(0x10000, 0xFFFFFFFF);
			while (ssrc2 == ssrc)
			{
				ssrc2 = sys::util::random_number<uint32_t>(0x10000, 0xFFFFFFFF);
			}
			ssrc_t ssrct2;
			ssrct2.ssrc = ssrc2;
			ssrct2.cname = cname_;
			ssrct2.msid = local_sdp_media_.msid;
			local_sdp_media_.ssrcs.push_back(ssrct2);

			local_sdp_media_.ssrc_group = "FID";
		}

		transport_rtp_ = transport_rtp;
		transport_rtp_->rtp_packet_event_.add(s_transport_rtp_packet, this);
		transport_rtp_->stun_message_event_.add(s_transport_stun_message, this);
		transport_rtp->raw_packet_event_.add(s_transport_raw_packet, this);
		transport_rtp->dtls_handshake_event_.add(s_transport_dtls_handshake, this);

		transport_rtcp_ = transport_rtcp;
		transport_rtcp_->rtcp_packet_event_.add(s_transport_rtcp_packet, this);
		if (transport_rtcp_ != transport_rtp_)
		{
			transport_rtcp_->stun_message_event_.add(s_transport_stun_message, this);
			transport_rtcp_->dtls_handshake_event_.add(s_transport_dtls_handshake, this);
		}

	}

	media_stream::media_stream(const std::string& mid, const std::string& protos, const std::string& ice_options, const std::string& ice_ufrag, const std::string& ice_pwd , transport_ptr transport)
	{
		local_sdp_media_.ice_options = ice_options;
		local_sdp_media_.ice_ufrag = ice_ufrag;
		local_sdp_media_.ice_pwd = ice_pwd;
		local_sdp_media_.media_type = media_type_t::media_type_application;
		local_sdp_media_.mid = mid;
		local_sdp_media_.rtp_port = transport->port_;
		//

		if (protos == "webrtc-datachannel")
		{
			local_sdp_media_.attrs.push_back(litertp::sdp_pair("sctp-port", "5000",":"));
			local_sdp_media_.attrs.push_back(litertp::sdp_pair("max-message-size", "262144", ":"));
			local_sdp_media_.app_data_type = protos;
			local_sdp_media_.protos.clear();
			local_sdp_media_.protos.insert("UDP");
			local_sdp_media_.protos.insert("DTLS");
			local_sdp_media_.protos.insert("SCTP");

			transport->init_sctp(5000);
			transport->sctp_->on_connected.add(&media_stream::s_sctp_connected, this);
			transport->sctp_->on_closed.add(&media_stream::s_sctp_closed, this);
			transport->sctp_->on_sctp_heartbeat.add(&media_stream::s_sctp_heartbeat, this);
			transport->sctp_->on_stream_opened.add(&media_stream::s_sctp_stream_open, this);
			transport->sctp_->on_stream_closed.add(&media_stream::s_sctp_stream_closed, this);
			transport->sctp_->on_stream_message.add(&media_stream::s_sctp_stream_message, this);
		}
		else
		{
			auto ps = sys::string_util::split(protos, "/");
			local_sdp_media_.protos.clear();
			for (auto p : ps)
			{
				local_sdp_media_.protos.insert(p);
			}
		}
		
		transport_rtp_ = transport;
		transport_rtcp_ = transport;
		transport->raw_packet_event_.add(s_transport_raw_packet, this);
		transport->dtls_handshake_event_.add(s_transport_dtls_handshake, this);
	}

	media_stream::~media_stream()
	{
		transport_rtp_->rtp_packet_event_.remove(s_transport_rtp_packet, this);
		transport_rtcp_->rtcp_packet_event_.remove(s_transport_rtcp_packet, this);
		transport_rtp_->stun_message_event_.remove(s_transport_stun_message, this);
		transport_rtcp_->stun_message_event_.remove(s_transport_stun_message, this);
		transport_rtp_->dtls_handshake_event_.remove(s_transport_dtls_handshake, this);
		transport_rtcp_->dtls_handshake_event_.remove(s_transport_dtls_handshake, this);

		transport_rtp_->raw_packet_event_.remove(s_transport_raw_packet, this);
		if (transport_rtp_->sctp_)
		{
			transport_rtp_->sctp_->on_connected.remove(&media_stream::s_sctp_connected, this);
			transport_rtp_->sctp_->on_closed.remove(&media_stream::s_sctp_closed, this);
			transport_rtp_->sctp_->on_sctp_heartbeat.remove(&media_stream::s_sctp_heartbeat, this);
			transport_rtp_->sctp_->on_stream_opened.remove(&media_stream::s_sctp_stream_open, this);
			transport_rtp_->sctp_->on_stream_closed.remove(&media_stream::s_sctp_stream_closed, this);
			transport_rtp_->sctp_->on_stream_message.remove(&media_stream::s_sctp_stream_message, this);
		}
		close();
	}

	const std::string& media_stream::mid()const
	{
		std::shared_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		return local_sdp_media_.mid;
	}

	void media_stream::close()
	{
		send_rtcp_bye();

		{
			std::unique_lock<std::shared_mutex>lk(senders_mutex_);
			senders_.clear();
		}
		{
			std::unique_lock<std::shared_mutex>lk(receivers_mutex_);
			receivers_.clear();
		}


	}

	
	bool media_stream::add_local_video_track(codec_type_t codec, uint8_t pt, int frequency, bool rtcp_fb)
	{
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		if (local_sdp_media_.media_type != media_type_video)
		{
			return false;
		}

		sdp_format fmt(pt, codec, frequency);
		if (rtcp_fb)
		{
			fmt.rtcp_fb.insert("goog-remb");
			//fmt.rtcp_fb.insert("transport-cc");
			fmt.rtcp_fb.insert("ccm fir");
			fmt.rtcp_fb.insert("nack");
			fmt.rtcp_fb.insert("nack pli");
		}
		local_sdp_media_.rtpmap.insert(std::make_pair((int)pt,fmt));
		return true;
	}

	bool media_stream::add_local_audio_track(codec_type_t codec, uint8_t pt, int frequency, int channels,bool rtcp_fb)
	{
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		if (local_sdp_media_.media_type != media_type_audio)
		{
			return false;
		}

		sdp_format fmt(pt, codec, frequency);
		if (rtcp_fb)
		{
			fmt.rtcp_fb.insert("goog-remb");
			//fmt.rtcp_fb.insert("transport-cc");
			fmt.rtcp_fb.insert("ccm fir");
			fmt.rtcp_fb.insert("nack");
			fmt.rtcp_fb.insert("nack pli");
		}
		local_sdp_media_.rtpmap.insert(std::make_pair((int)pt, fmt));

		
		return true;
	}

	bool media_stream::add_local_rtx_track(uint8_t apt, uint8_t rtx_pt, int frequency, bool rtcp_fb)
	{
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);

		sdp_format fmt(rtx_pt,codec_type_rtx, frequency);
		if (rtcp_fb)
		{
			fmt.rtcp_fb.insert("goog-remb");
			//fmt.rtcp_fb.insert("transport-cc");
			fmt.rtcp_fb.insert("ccm fir");
			fmt.rtcp_fb.insert("nack");
			fmt.rtcp_fb.insert("nack pli");
		}
		fmt.fmtp.set_apt(apt);
		local_sdp_media_.rtpmap.insert(std::make_pair((int)rtx_pt, fmt));
		return true;
	}

	bool media_stream::add_remote_video_track(codec_type_t codec, uint8_t pt, int frequency,bool rtcp_fb)
	{
		std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		if (media_type() != media_type_video)
		{
			return false;
		}

		sdp_format fmt(pt, codec, frequency);
		if (rtcp_fb)
		{
			fmt.rtcp_fb.insert("goog-remb");
			//fmt.rtcp_fb.insert("transport-cc");
			fmt.rtcp_fb.insert("ccm fir");
			fmt.rtcp_fb.insert("nack");
			fmt.rtcp_fb.insert("nack pli");
		}
		remote_sdp_media_.rtpmap.insert(std::make_pair((int)pt, fmt));
		return true;
	}

	bool media_stream::add_remote_audio_track(codec_type_t codec, uint8_t pt, int frequency, int channels, bool rtcp_fb)
	{
		std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		if (media_type()!= media_type_audio)
		{
			return false;
		}

		sdp_format fmt(pt, codec, frequency, channels);
		if (rtcp_fb)
		{
			fmt.rtcp_fb.insert("goog-remb");
			//fmt.rtcp_fb.insert("transport-cc");
			fmt.rtcp_fb.insert("ccm fir");
			fmt.rtcp_fb.insert("nack");
			fmt.rtcp_fb.insert("nack pli");
		}
		remote_sdp_media_.rtpmap.insert(std::make_pair((int)pt, fmt));

		return true;
	}

	bool media_stream::add_remote_rtx_track(uint8_t apt, uint8_t rtx_pt, int frequency, bool rtcp_fb)
	{
		std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);

		sdp_format fmt(rtx_pt, codec_type_rtx, frequency);
		if (rtcp_fb)
		{
			fmt.rtcp_fb.insert("goog-remb");
			//fmt.rtcp_fb.insert("transport-cc");
			fmt.rtcp_fb.insert("ccm fir");
			fmt.rtcp_fb.insert("nack");
			fmt.rtcp_fb.insert("nack pli");
		}
		fmt.fmtp.set_apt(apt);
		remote_sdp_media_.rtpmap.insert(std::make_pair((int)rtx_pt, fmt));
		return true;
	}

	void media_stream::set_remote_trans_mode(rtp_trans_mode_t trans_mode)
	{
		std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		remote_sdp_media_.trans_mode = trans_mode;
	}
	void media_stream::set_local_trans_mode(rtp_trans_mode_t trans_mode)
	{
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		local_sdp_media_.trans_mode = trans_mode;
	}

	void media_stream::set_remote_ssrc(uint32_t ssrc)
	{
		std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		ssrc_t ssrct;
		ssrct.ssrc = ssrc;
		ssrct.cname = cname_;
		ssrct.msid = remote_sdp_media_.msid;
		remote_sdp_media_.ssrcs.push_back(ssrct);
	}

	void media_stream::set_remote_setup(sdp_setup_t setup)
	{
		std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		remote_sdp_media_.setup = setup;
	}
	void media_stream::set_local_setup(sdp_setup_t setup)
	{
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		local_sdp_media_.setup = setup;
	}

	bool media_stream::add_remote_rtpmap_attribute(uint16_t pt, const char* key, const char* val)
	{
		std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		auto itr = remote_sdp_media_.rtpmap.find(pt);
		if (itr == remote_sdp_media_.rtpmap.end())
		{
			return false;
		}

		if (sys::string_util::icasecompare(key, "rtcp_fb"))
		{
			itr->second.rtcp_fb.insert(val);
		}
		else if (sys::string_util::icasecompare(key, "fmtp"))
		{
			itr->second.fmtp.parse(val);
		}
		else
		{
			itr->second.attrs.push_back(sdp_pair(key, val,":"));
		}
		return true;
	}

	bool media_stream::clear_remote_rtpmap_attributes(uint16_t pt)
	{
		std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		auto itr = remote_sdp_media_.rtpmap.find(pt);
		if (itr == remote_sdp_media_.rtpmap.end())
		{
			return false;
		}
		itr->second.attrs.clear();
		itr->second.rtcp_fb.clear();
		itr->second.fmtp.clear();
		return true;
	}

	bool media_stream::add_local_rtpmap_attribute(uint16_t pt, const char* key, const char* val)
	{
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		auto itr=local_sdp_media_.rtpmap.find(pt);
		if (itr == local_sdp_media_.rtpmap.end())
		{
			return false;
		}
		
		if (sys::string_util::icasecompare(key,"rtcp_fb"))
		{
			itr->second.rtcp_fb.insert(val);
		}
		else if (sys::string_util::icasecompare(key, "fmtp"))
		{
			itr->second.fmtp.parse(val);
		}
		else 
		{
			itr->second.attrs.push_back(sdp_pair(key, val,":"));
		}
		return true;
	}

	bool media_stream::clear_local_rtpmap_attributes(uint16_t pt)
	{
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		auto itr = local_sdp_media_.rtpmap.find(pt);
		if (itr == local_sdp_media_.rtpmap.end())
		{
			return false;
		}
		itr->second.attrs.clear();
		itr->second.rtcp_fb.clear();
		itr->second.fmtp.clear();
		return true;
	}

	void media_stream::add_remote_attribute(const char* key, const char* val)
	{
		std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		remote_sdp_media_.attrs.push_back(litertp::sdp_pair(key, val, ":"));
	}

	void media_stream::clear_remote_attributes()
	{
		std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		remote_sdp_media_.attrs.clear();
	}

	void media_stream::add_local_attribute(const char* key, const char* val)
	{
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);

		local_sdp_media_.attrs.push_back(litertp::sdp_pair(key, val, ":"));
	}

	void media_stream::clear_local_attributes()
	{
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		local_sdp_media_.attrs.clear();
	}

	bool media_stream::set_local_fmtp(int pt,const fmtp& fmtp)
	{
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		auto itr=local_sdp_media_.rtpmap.find(pt);
		if (itr == local_sdp_media_.rtpmap.end())
		{
			return false;
		}
		
		itr->second.fmtp = fmtp;
		return true;
	}

	bool media_stream::get_local_fmtp(int pt, fmtp& fmtp)
	{
		std::shared_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		auto itr = local_sdp_media_.rtpmap.find(pt);
		if (itr == local_sdp_media_.rtpmap.end())
		{
			return false;
		}
		fmtp = itr->second.fmtp;
		return true;
	}

	bool media_stream::set_remote_fmtp(int pt, const fmtp& fmtp)
	{
		std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		auto itr = remote_sdp_media_.rtpmap.find(pt);
		if (itr == remote_sdp_media_.rtpmap.end())
		{
			return false;
		}
		itr->second.fmtp = fmtp;
		return true;
	}

	bool media_stream::get_remote_fmtp(int pt, fmtp& fmtp)
	{
		std::shared_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		auto itr = remote_sdp_media_.rtpmap.find(pt);
		if (itr == remote_sdp_media_.rtpmap.end())
		{
			return false;
		}
		fmtp = itr->second.fmtp;
		return true;
	}

	void media_stream::add_local_candidate(uint32_t foundation, uint32_t component, const char* address, int port, uint32_t priority)
	{
		candidate ca;
		ca.foundation = foundation;
		ca.component = component;
		ca.address = address;
		ca.port = port;
		ca.priority = priority;
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		ca.ufrag = local_sdp_media_.ice_ufrag;
		local_sdp_media_.candidates.push_back(ca);
	}

	void media_stream::add_remote_candidate(uint32_t foundation, uint32_t component, const char* address, int port, uint32_t priority)
	{
		candidate ca;
		ca.foundation = foundation;
		ca.component = component;
		ca.address = address;
		ca.port = port;
		ca.priority = priority;
		std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		ca.ufrag = remote_sdp_media_.ice_ufrag;
		remote_sdp_media_.candidates.push_back(ca);
	}

	void media_stream::clear_local_candidates()
	{
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		local_sdp_media_.candidates.clear();
	}

	void media_stream::clear_remote_candidates()
	{
		std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		remote_sdp_media_.candidates.clear();
	}

	void media_stream::enable_dtls()
	{
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		is_dtls_ = true;
#ifdef LITERTP_SSL
		if (transport_rtp_->enable_security(true))
		{
			if (local_sdp_media_.media_type == media_type_application)
			{
				local_sdp_media_.protos.insert("DTLS");
			}
			else 
			{
				local_sdp_media_.protos.erase("AVP");
				local_sdp_media_.protos.erase("AVPF");
				local_sdp_media_.protos.insert("TLS");
				local_sdp_media_.protos.insert("SAVPF");
			}

			local_sdp_media_.fingerprint = transport_rtp_->fingerprint();
		}
		transport_rtcp_->enable_security(true);
#endif
	}

	void media_stream::disable_dtls()
	{
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		is_dtls_ = false;
#ifdef LITERTP_SSL

		if (local_sdp_media_.media_type == media_type_application)
		{
			local_sdp_media_.protos.erase("DTLS");
		}
		else
		{
			local_sdp_media_.protos.erase("SAVP");
			local_sdp_media_.protos.erase("SAVPF");
			local_sdp_media_.protos.erase("TLS");
			local_sdp_media_.protos.insert("AVPF");
		}
		local_sdp_media_.fingerprint = "";
		if (transport_rtp_)
		{
			transport_rtp_->enable_security(false);
		}
		if (transport_rtcp_)
		{
			transport_rtcp_->enable_security(false);
		}
#endif
	}

	bool media_stream::has_remote_sdp()const
	{
		std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		return has_remote_sdp_media_;
	}

	bool media_stream::set_remote_sdp(const sdp_media& sdp, sdp_type_t sdp_type)
	{
		{
			std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
			if (has_remote_sdp_media_)
				return false;
			remote_sdp_media_ = sdp;
			has_remote_sdp_media_ = true;
		}

		if (transport_rtp_)
		{
			std::string s_remote_port = sdp.get_attribute("sctp-port", "5000");
			char* endptr = nullptr;
			transport_rtp_->sctp_remote_port_ = (uint16_t)strtol(s_remote_port.c_str(), &endptr, 0);
		}

		sdp_media sdpm_local = this->get_local_sdp();

		transport_rtp_->ice_ufrag_remote_ = sdp.ice_ufrag;
		transport_rtp_->ice_pwd_remote_ = sdp.ice_pwd;
		transport_rtcp_->ice_ufrag_remote_ = sdp.ice_ufrag;
		transport_rtcp_->ice_pwd_remote_ = sdp.ice_pwd;

		if (sdp_type == sdp_type_offer)
		{
			//this is the offer from remote end, we can modify the local sdp

			if (sdp.setup == sdp_setup_actpass)
			{
				this->set_local_setup(sdp_setup_active);
			}

			if (sdp.trans_mode == rtp_trans_mode_recvonly)
			{
				this->set_local_trans_mode(rtp_trans_mode_sendonly);
			}
			else if (sdp.trans_mode == rtp_trans_mode_sendonly)
			{
				this->set_local_trans_mode(rtp_trans_mode_recvonly);
			}

			if (sdp.is_security())
			{
				enable_dtls();
			}
			else
			{
				disable_dtls();
			}

		}
		else 
		{
			//this is the answer from remote end
			if (!check_setup())
			{
				return false;
			}

			if (!this->negotiate())
			{
				return false;
			}
			if (!this->exchange())
			{
				return false;
			}
			{
				std::shared_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
				if (sdp.is_security()&&!local_sdp_media_.is_security())
				{
					return false;
				}
			}
			
		}

		this->clear_remote_rtp_endpoint();
		this->clear_remote_rtcp_endpoint();
		std::vector<candidate> candidates;
		{
			std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
			candidates = this->remote_sdp_media_.candidates;
			has_remote_sdp_media_ = true;
		}
		if (candidates.size() > 0)
		{
			uint32_t priority = 0;
			for (auto itr = candidates.begin(); itr != candidates.end(); itr++)
			{
				sockaddr_storage addr = { };
				sys::socket::ep2addr(AF_INET,itr->address.c_str(), itr->port, (sockaddr*)&addr);
				if (itr->component == 1)
				{
					this->set_remote_rtp_endpoint((const sockaddr*)&addr, sizeof(addr), itr->priority);
				}
				else if (itr->component == 2)
				{
					this->set_remote_rtcp_endpoint((const sockaddr*)&addr, sizeof(addr), itr->priority);
				}

				if (itr->priority > priority)
				{
					priority = itr->priority;
				}
			}

			sockaddr_storage addr2 = {};
			if (!this->get_remote_rtcp_endpoint(&addr2))
			{
				if (this->get_remote_rtp_endpoint(&addr2))
				{
					this->set_remote_rtcp_endpoint((const sockaddr*)&addr2, sizeof(addr2), priority);
				}
			}
		}
		else 
		{
			sockaddr_storage addr_rtp = { 0 };
			if (sys::string_util::icasecompare(sdp.rtp_network_type, "IP6"))
			{
				sys::socket::ep2addr(AF_INET6, sdp.rtp_address.c_str(), sdp.rtp_port, (sockaddr*)&addr_rtp);
			}
			else
			{
				sys::socket::ep2addr(AF_INET, sdp.rtp_address.c_str(), sdp.rtp_port, (sockaddr*)&addr_rtp);
			}
			this->set_remote_rtp_endpoint((const sockaddr*)&addr_rtp, sizeof(addr_rtp), 0);

			sockaddr_storage addr_rtcp = { 0 };
			if (sys::string_util::icasecompare(sdp.rtcp_network_type, "IP6"))
			{
				sys::socket::ep2addr(AF_INET6, sdp.rtcp_address.c_str(), sdp.rtcp_port, (sockaddr*)&addr_rtcp);
			}
			else
			{
				sys::socket::ep2addr(AF_INET, sdp.rtcp_address.c_str(), sdp.rtcp_port, (sockaddr*)&addr_rtcp);
			}
			this->set_remote_rtcp_endpoint((const sockaddr*)&addr_rtcp, sizeof(addr_rtcp), 0);
		}



		return true;
	}

	void media_stream::set_local_sdp(const sdp_media& sdp)
	{
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		this->local_sdp_media_ = sdp;
	}

	void media_stream::set_sdp_type(sdp_type_t sdp_type)
	{
		sdp_type_ = sdp_type;
		if (sdp_type_ == sdp_type_offer)
		{
			std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
			local_sdp_media_.setup = sdp_setup_actpass;
			local_sdp_media_.rtcp_rsize = true;
		}
	}

	void media_stream::set_remote_mid(const char* mid)
	{
		std::unique_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		remote_sdp_media_.mid = mid;
	}

	void media_stream::set_remote_rtp_endpoint(const sockaddr* addr, int addr_size, uint32_t priority)
	{
		//std::string ip;
		//int port = 0;
		//sys::socket::addr2ep(addr, &ip, &port);

		std::unique_lock<std::shared_mutex>lk(remote_rtp_endpoint_mutex_);
		if (priority > remote_rtp_endpoint_priority)
		{
			memcpy(&remote_rtp_endpoint_, addr, addr_size);
			remote_rtp_endpoint_priority = priority;

			if (transport_rtp_&&transport_rtp_->sctp_)
			{
				transport_rtp_->sctp_->set_remote_addr(addr, addr_size);
			}
		}
	}

	void media_stream::set_remote_rtcp_endpoint(const sockaddr* addr, int addr_size, uint32_t priority)
	{
		std::unique_lock<std::shared_mutex>lk(remote_rtcp_endpoint_mutex_);
		if (priority > remote_rtcp_endpoint_priority) 
		{
			memcpy(&remote_rtcp_endpoint_, addr, addr_size);
			remote_rtcp_endpoint_priority = priority;
		}
	}

	bool media_stream::get_remote_rtp_endpoint(sockaddr_storage* addr)
	{
		std::shared_lock<std::shared_mutex> lk(remote_rtp_endpoint_mutex_);
		if (remote_rtp_endpoint_priority <0)
		{
			return false;
		}
		memcpy(addr, &remote_rtp_endpoint_, sizeof(remote_rtp_endpoint_));
		return true;
	}

	bool media_stream::get_remote_rtcp_endpoint(sockaddr_storage* addr)
	{
		std::shared_lock<std::shared_mutex>lk(remote_rtcp_endpoint_mutex_);
		if (remote_rtcp_endpoint_priority < 0)
		{
			return false;
		}
		memcpy(addr, &remote_rtcp_endpoint_, sizeof(remote_rtcp_endpoint_));
		return true;
	}

	void media_stream::clear_remote_rtp_endpoint()
	{
		std::unique_lock<std::shared_mutex>lk(remote_rtp_endpoint_mutex_);
		remote_rtp_endpoint_priority = -1;
	}

	void media_stream::clear_remote_rtcp_endpoint()
	{
		std::unique_lock<std::shared_mutex>lk(remote_rtcp_endpoint_mutex_);
		remote_rtcp_endpoint_priority = -1;
	}

	srtp_role_t media_stream::srtp_role()
	{
		auto sdpm_remote = this->get_remote_sdp();
		auto sdpm_local = this->get_local_sdp();

		if (sdpm_local.setup == sdp_setup_active&& (sdpm_remote.setup == sdp_setup_actpass || sdpm_remote.setup == sdp_setup_passive))
		{
			return srtp_role_client;
		}
		else if (sdpm_local.setup == sdp_setup_passive && (sdpm_remote.setup == sdp_setup_actpass|| sdpm_remote.setup == sdp_setup_active))
		{
			return srtp_role_server;
		}
		else if (sdpm_local.setup == sdp_setup_actpass && (sdpm_remote.setup == sdp_setup_active))
		{
			return srtp_role_server;
		}
		else if (sdpm_local.setup == sdp_setup_actpass && (sdpm_remote.setup == sdp_setup_passive))
		{
			return srtp_role_client;
		}
		else
		{
			return srtp_role_client;
		}
	}

	stun_ice_role_t media_stream::stun_ice_role()
	{
		if (remote_ice_lite_)
			return stun_ice_controlling;

		if (sdp_type_ == sdp_type_offer)
			return stun_ice_controlling;
		else
			return stun_ice_controlled;
	}

	bool media_stream::check_setup()
	{
		if (!is_dtls_)
			return true;
		auto sdpm_remote = this->get_remote_sdp();
		auto sdpm_local = this->get_local_sdp();
		if (sdpm_local.setup == sdp_setup_active && (sdpm_remote.setup == sdp_setup_actpass || sdpm_remote.setup == sdp_setup_passive))
		{
			return true;
		}
		else if (sdpm_local.setup == sdp_setup_passive && (sdpm_remote.setup == sdp_setup_actpass || sdpm_remote.setup == sdp_setup_active))
		{
			return true;
		}
		else if (sdpm_local.setup == sdp_setup_actpass && (sdpm_remote.setup == sdp_setup_active))
		{
			return true;
		}
		else if (sdpm_local.setup == sdp_setup_actpass && (sdpm_remote.setup == sdp_setup_passive))
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	bool media_stream::exchange()
	{
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		std::unique_lock<std::shared_mutex> lk2(remote_sdp_media_mutex_);

		//I am an offer, this called by remote answer
		std::vector<sdp_format> local_fmts;
		for (auto itr = local_sdp_media_.rtpmap.begin(); itr != local_sdp_media_.rtpmap.end(); itr++)
		{
			sdp_format fmt;
			if (remote_sdp_media_.negotiate(&itr->second, &fmt))
			{
				sdp_format local_fmt = itr->second;
				local_fmt.pt = fmt.pt;
				local_fmts.push_back(local_fmt);
				remote_sdp_media_.rtpmap.emplace(itr->first, itr->second); //append local to remote
			}
		}

		//Replace local payload with remote
		local_sdp_media_.rtpmap.clear();
		for (auto itr = local_fmts.begin(); itr != local_fmts.end(); itr++)
		{
			local_sdp_media_.rtpmap.emplace(itr->pt, *itr);
		}

		return true;
	}


	bool media_stream::negotiate()
	{
		{
			std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
			std::unique_lock<std::shared_mutex> lk2(remote_sdp_media_mutex_);

			for (auto itr = remote_sdp_media_.rtpmap.begin(); itr != remote_sdp_media_.rtpmap.end();)
			{
				sdp_format fmt;
				if (!local_sdp_media_.negotiate(&itr->second, &fmt))
				{
					itr = remote_sdp_media_.rtpmap.erase(itr);
				}
				else
				{
					itr->second.rtcp_fb.erase("transport-cc");  //not suported
					itr++;
				}
			}

			
			for (auto itr = local_sdp_media_.rtpmap.begin(); itr != local_sdp_media_.rtpmap.end();)
			{
				sdp_format fmt;
				if (!remote_sdp_media_.negotiate(&itr->second, &fmt))
				{
					itr = local_sdp_media_.rtpmap.erase(itr);
				}
				else
				{
					itr++;
				}
			}
			if ((local_sdp_media_.rtpmap.size() == 0|| remote_sdp_media_.rtpmap.size() == 0)&&
				(local_sdp_media_.media_type!=media_type_application|| remote_sdp_media_.media_type != media_type_application))
			{
				return false;
			}

			

			if (sdp_type_ == sdp_type_offer)
			{
				//I am an offer, this called by remote answer
			}
			else if (sdp_type_ == sdp_type_answer)
			{
				//I am an anwser, this called by remote offer
				//If not clear this, webrtc stream will be delayed.
				remote_sdp_media_.extmap.clear();

				//local_sdp_media_.set_msid(remote_sdp_media_.msid);
			}
			else
			{
				return false;
			}
		}
		transport_rtp_->srtp_role_ = srtp_role();
		transport_rtcp_->srtp_role_ = transport_rtp_->srtp_role_;

		transport_rtp_->ice_role_ = stun_ice_role();
		transport_rtcp_->ice_role_ = transport_rtp_->ice_role_;

		transport_rtp_->sdp_type_ = sdp_type_;
		transport_rtcp_->sdp_type_ = sdp_type_;
		return true;
	}



	void media_stream::run_rtcp_stats()
	{
		std::string compound_pkt;
		compound_pkt.reserve(2048);
		auto senders=get_senders();
		if (senders.size() > 0)
		{
			for (auto sender : senders) 
			{
				if (sender->format().codec == codec_type_rtx)
				{
					continue;
				}
				rtcp_sr sr = { 0 };
				sender->prepare_sr(sr);

				auto receivers = get_receivers();
				for (auto receiver : receivers)
				{
					if (receiver->format().codec == codec_type_rtx)
					{
						continue;
					}
					rtcp_report rp;
					receiver->prepare_rr(rp);
					rtcp_sr_add_report(&sr, &rp);
				}

				size_t size = rtcp_sr_size(&sr);
				uint8_t* buf_sr = (uint8_t*)malloc(2048);
				if (buf_sr) {
					rtcp_sr_serialize(&sr, buf_sr, size);
					compound_pkt.append((const char*)buf_sr, size);
					free(buf_sr);
				}
			}
		}
		else
		{
			auto receivers = get_receivers();
			for (auto receiver : receivers)
			{
				if (receiver->format().codec == codec_type_rtx)
				{
					continue;
				}

				rtcp_rr* rr = rtcp_rr_create();
				rr->ssrc = receiver->ssrc();
				rtcp_rr_init(rr);
				rtcp_report rp;
				receiver->prepare_rr(rp);
				rtcp_rr_add_report(rr, &rp);
				size_t size = rtcp_rr_size(rr);
				uint8_t* buf_rr = (uint8_t*)malloc(2048);
				if (buf_rr) {
					rtcp_rr_serialize(rr, buf_rr, size);
					compound_pkt.append((const char*)buf_rr, size);
					free(buf_rr);
				}
				rtcp_rr_free(rr);
			}

			
		}


		rtcp_sdes* sdes = rtcp_sdes_create();
		rtcp_sdes_init(sdes);
		
		auto sdpm_local = this->get_local_sdp();
		for (auto itr : sdpm_local.ssrcs) 
		{
			rtcp_sdes_add_entry(sdes, itr.ssrc);
			rtcp_sdes_set_item(sdes, itr.ssrc, RTCP_SDES_CNAME, itr.cname.c_str());
		}
		size_t size = rtcp_sdes_size(sdes);
		uint8_t* buf_sdes = (uint8_t*)malloc(size);
		if (buf_sdes) {
			rtcp_sdes_serialize(sdes, buf_sdes, size);
			compound_pkt.append((const char*)buf_sdes, size);
			free(buf_sdes);
		}
		rtcp_sdes_free(sdes);

		send_rtcp_packet((uint8_t*)compound_pkt.data(), (int)compound_pkt.size());



	}

	void media_stream::run_stun_request()
	{
		sockaddr_storage addr_rtp = { 0 };
		sockaddr_storage addr_rtcp = { 0 };

		auto sdpm_remote = this->get_remote_sdp();

		if (get_remote_rtp_endpoint(&addr_rtp) && get_remote_rtcp_endpoint(&addr_rtcp))
		{
			this->get_remote_rtp_endpoint(&addr_rtp);
			transport_rtp_->send_stun_request((const sockaddr*)&addr_rtp, sizeof(addr_rtp), (uint32_t)remote_rtp_endpoint_priority);
			if (!sdpm_remote.rtcp_mux&& transport_rtp_!= transport_rtcp_)
			{
				transport_rtcp_->send_stun_request((const sockaddr*)&addr_rtcp, sizeof(addr_rtcp), (uint32_t)remote_rtcp_endpoint_priority);
			}
		}
		else
		{
			for (auto& ca : sdpm_remote.candidates)
			{
				if (ca.component == 1)
				{
					sockaddr_storage addr = { 0 };
					sys::socket::ep2addr(ca.address.c_str(), ca.port, (sockaddr*)&addr);

					transport_rtp_->send_stun_request((const sockaddr*)&addr, sizeof(addr), ca.priority);
				}
				else if (ca.component == 2)
				{
					sockaddr_storage addr = { 0 };
					sys::socket::ep2addr(ca.address.c_str(), ca.port, (sockaddr*)&addr);

					transport_rtcp_->send_stun_request((const sockaddr*)&addr, sizeof(addr), ca.priority);
				}
			}
		}
		
	}


	void media_stream::send_rtcp_nack(uint32_t ssrc_sender, uint32_t ssrc_media, uint16_t pid, uint16_t blp)
	{
		rtcp_fb* fb = rtcp_fb_create();
		if (!fb)
			return;
		rtcp_fb_init(fb, RTCP_RTPFB, RTCP_RTPFB_FMT_NACK);
		fb->ssrc_sender = ssrc_sender;
		fb->ssrc_media = ssrc_media;
		rtcp_rtpfb_nack_set(fb, pid, blp);

		uint8_t buffer[2048] = { 0 };// size 2048 for srtp
		int size=rtcp_fb_serialize(fb, buffer,sizeof(buffer));
		rtcp_fb_free(fb);
		if (size>0)
		{
			send_rtcp_packet(buffer, size);
		}

		auto receiver = get_receiver_by_ssrc(ssrc_media);
		if (receiver)
		{
			receiver->increase_nack();
		}
	}


	void media_stream::send_rtcp_keyframe(uint32_t ssrc_media)
	{
		auto sdpm_remote = get_remote_sdp();
		uint32_t ssrc=get_local_ssrc();

		for (auto itr_fmt = sdpm_remote.rtpmap.begin(); itr_fmt != sdpm_remote.rtpmap.end(); itr_fmt++)
		{
			if (itr_fmt->second.codec == codec_type_rtx) {
				continue;
			}

			if (itr_fmt->second.rtcp_fb.find("nack pli") != itr_fmt->second.rtcp_fb.end())
			{
				send_rtcp_keyframe_pli(ssrc, ssrc_media);
			}
			else if (itr_fmt->second.rtcp_fb.find("ccm fir") != itr_fmt->second.rtcp_fb.end())
			{
				send_rtcp_keyframe_fir(ssrc, ssrc_media);
			}
			else
			{
				send_rtcp_keyframe_pli(ssrc, ssrc_media);
			}
		}

	}

	void media_stream::send_rtcp_keyframe(uint32_t ssrc_media, uint16_t pt)
	{
		auto sdpm_remote = get_remote_sdp();
		uint32_t ssrc = get_local_ssrc();


		sdp_format fmt;
		if (!get_remote_format(pt, fmt))
			return;


		if (fmt.rtcp_fb.find("nack pli") == fmt.rtcp_fb.end())
		{
			send_rtcp_keyframe_pli(ssrc, ssrc_media);
		}
		else if (fmt.rtcp_fb.find("ccm fir") == fmt.rtcp_fb.end())
		{
			send_rtcp_keyframe_fir(ssrc, ssrc_media);
		}
		else
		{
			send_rtcp_keyframe_pli(ssrc, ssrc_media);
		}
	}

	void media_stream::send_rtcp_keyframe_pli(uint32_t ssrc_sender, uint32_t ssrc_media)
	{

		if (on_require_keyframe.count() > 0) {
			on_require_keyframe.invoke(ssrc_sender, ssrc_media);
			return;
		}

		auto receiver = get_receiver_by_ssrc(ssrc_media);
		if (receiver)
		{
			receiver->increase_pli();
		}

		rtcp_fb* fb = rtcp_fb_create();
		rtcp_fb_init(fb, RTCP_PSFB, RTCP_PSFB_FMT_PLI);
		fb->ssrc_sender = ssrc_sender;
		fb->ssrc_media = ssrc_media;
		uint8_t buffer[2048] = { 0 };// size 2048 for srtp
		int size=rtcp_fb_serialize(fb, buffer, sizeof(buffer));
		rtcp_fb_free(fb);
		if (size > 0) 
		{
			send_rtcp_packet(buffer, size);
		}
	}

	void media_stream::send_rtcp_keyframe_fir(uint32_t ssrc_sender, uint32_t ssrc_media)
	{
		if (on_require_keyframe.count() > 0) {
			on_require_keyframe.invoke(ssrc_sender, ssrc_media);
			return;
		}

		auto receiver = get_receiver_by_ssrc(ssrc_media);

		rtcp_fb* fb = rtcp_fb_create();
		rtcp_fb_init(fb, RTCP_PSFB, RTCP_PSFB_FMT_FIR);
		fb->ssrc_sender = ssrc_sender;
		fb->ssrc_media = ssrc_media;
		
		uint8_t fci[8] = { 0 };
		fci[0] = (ssrc_media & 0xFF000000) >> 24;
		fci[1] = (ssrc_media & 0x00FF0000) >> 16;
		fci[2] = (ssrc_media & 0x0000FF00) >> 8;
		fci[3] = ssrc_media & 0x000000FF;
		if (receiver) 
		{
			fci[4] = receiver->fir_count();
			receiver->increase_fir();
		}
		rtcp_fb_set_fci(fb, fci, sizeof(fci));

		uint8_t buffer[2048] = { 0 };// size 2048 for srtp
		int size = rtcp_fb_serialize(fb, buffer, sizeof(buffer));
		rtcp_fb_free(fb);
		if (size > 0)
		{
			send_rtcp_packet(buffer, size);
		}


	}

	void media_stream::send_rtcp_bye(const char* reason)
	{
		rtcp_bye* pkt=rtcp_bye_create();
		if (!pkt)
			return;
		pkt->message = (char*)reason;
		
		auto senders = this->get_senders();
		
		pkt->src_ids = new uint32_t[senders.size()]();
		for (auto sender:senders)
		{
			if (sender->format().codec != codec_type_rtx)
			{
				rtcp_bye_add_source(pkt, sender->ssrc());
			}
		}
		
		uint8_t buffer[2048] = { 0 };
		int size=rtcp_bye_serialize(pkt, buffer, sizeof(buffer));
		rtcp_bye_free(pkt);
		if (size > 0)
		{
			send_rtcp_packet(buffer, size);
		}
	}


	bool media_stream::send_rtp_packet(packet_ptr packet)
	{
		sockaddr_storage addr = { 0 };
		if (!this->get_remote_rtp_endpoint(&addr))
		{
			return false;
		}

		std::string ip;
		int port = 0;
		sys::socket::addr2ep((const sockaddr*)&addr, &ip, &port);

		return transport_rtp_->send_rtp_packet(packet, (const sockaddr*)&addr, sizeof(addr));
	}

	bool media_stream::send_rtcp_packet(const uint8_t* rtcp_data, int size)
	{
		sockaddr_storage addr = { 0 };
		if (!this->get_remote_rtcp_endpoint(&addr))
		{
			return false;
		}

		return transport_rtcp_->send_rtcp_packet(rtcp_data, size, (const sockaddr*)&addr, sizeof(addr));
	}

	bool media_stream::send_app_packet(const uint8_t* data, int size)
	{
		sockaddr_storage addr = { 0 };
		if (!this->get_remote_rtp_endpoint(&addr))
		{
			return false;
		}

		return transport_rtp_->send_raw_packet(data, size, (const sockaddr*)&addr, sizeof(addr));
	}

	bool media_stream::send_data_channel(const uint8_t* data, int size)
	{
		if (!transport_rtp_->sctp_)
			return false;

		std::vector<uint8_t> buf(data, data + size);
		return transport_rtp_->sctp_->send_user_message(sctp_stream_id_, buf);
	}

	media_type_t media_stream::media_type()
	{
		std::shared_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		return local_sdp_media_.media_type;
	}

	bool media_stream::send_frame(const uint8_t* frame, uint32_t size, uint32_t duration)
	{
		auto sender= get_default_sender();
		if (!sender)
		{
			return false;
		}

		return sender->send_frame(frame, size, duration);
	}


	sdp_media media_stream::get_local_sdp()
	{
		std::shared_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		return local_sdp_media_;

	}

	sdp_media media_stream::get_remote_sdp()
	{
		std::shared_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		return remote_sdp_media_;
	}

	bool media_stream::get_local_format(int pt, sdp_format& fmt)
	{
		std::shared_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		auto itr=local_sdp_media_.rtpmap.find(pt);
		if (itr == local_sdp_media_.rtpmap.end())
		{
			return false;
		}

		fmt = itr->second;
		return true;
	}
	bool media_stream::get_remote_format(int pt, sdp_format& fmt)
	{
		std::shared_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		auto itr = remote_sdp_media_.rtpmap.find(pt);
		if (itr == remote_sdp_media_.rtpmap.end())
		{
			return false;
		}

		fmt = itr->second;
		return true;
	}


	uint32_t media_stream::get_local_ssrc(int idx)
	{
		std::shared_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		if (idx < 0 || idx >= (int)local_sdp_media_.ssrcs.size())
		{
			return 0;
		}

		return local_sdp_media_.ssrcs[idx].ssrc;
	}
	uint32_t media_stream::get_remote_ssrc(int idx)
	{
		std::shared_lock<std::shared_mutex> lk(remote_sdp_media_mutex_);
		if (idx < 0 || idx >= (int)remote_sdp_media_.ssrcs.size())
		{
			return 0;
		}

		return remote_sdp_media_.ssrcs[idx].ssrc;
	}

	void media_stream::get_stats(rtp_stats_t& stats)
	{
		memset(&stats, 0, sizeof(stats));
		auto sender=this->get_default_sender();
		auto sdp_local = this->get_local_sdp();

		uint16_t pt = 0;
		
		if (sender) 
		{
			pt=sender->format().pt;
		}
		
		if (pt == 0)
		{
			auto itr = sdp_local.rtpmap.begin();
			if (itr != sdp_local.rtpmap.end())
			{
				pt = itr->first;
			}
		}


		stats.pt = pt;
		stats.mt = sdp_local.media_type;

		receiver_ptr receiver = this->get_receiver(pt);

		if (sender)
		{
			stats.ct = sender->format().codec;
			sender->get_stats(stats.sender_stats);
		}

		if (receiver)
		{
			if (stats.ct == codec_type_unknown)
			{
				stats.ct = receiver->format().codec;
			}
			
			receiver->get_stats(stats.receiver_stats);
		}
		
	}


	void media_stream::set_timestamp(uint32_t ms)
	{
		std::shared_lock<std::shared_mutex>lk(senders_mutex_);
		for (auto itr=senders_.begin();itr!=senders_.end();itr++)
		{
			itr->second->set_last_rtp_timestamp_ms(ms);
		}
	}

	uint32_t media_stream::timestamp()
	{
		auto sender=get_default_sender();
		if (!sender)
		{
			return 0;
		}

		return sender->last_rtp_timestamp_ms();
	}

	void media_stream::set_msid(const std::string& msid)
	{
		std::unique_lock<std::shared_mutex> lk(local_sdp_media_mutex_);
		local_sdp_media_.msid = msid;
	}



	sender_ptr media_stream::get_sender(int pt)
	{
		{
			std::shared_lock<std::shared_mutex>lk(senders_mutex_);
			auto itr = senders_.find(pt);
			if (itr != senders_.end())
			{
				return itr->second;
			}
		}


		sdp_format fmt;
		if (!get_local_format(pt, fmt))
		{
			return nullptr;
		}

		return create_sender(fmt);

	}

	sender_ptr media_stream::get_sender_by_ssrc(uint32_t ssrc)
	{
		std::shared_lock<std::shared_mutex>lk(senders_mutex_);
		for (auto itr = senders_.begin(); itr != senders_.end(); itr++)
		{
			if (itr->second->ssrc() == ssrc)
			{
				return itr->second;
			}
		}
		return nullptr;
	}

	sender_ptr media_stream::get_default_sender()
	{
		{
			std::shared_lock<std::shared_mutex>lk(senders_mutex_);
			for (auto itr=senders_.begin() ;itr!= senders_.end();itr++)
			{
				if (itr->second->format().codec != codec_type_rtx) {
					return itr->second;
				}
			}
		}



		auto sdp = this->get_local_sdp();
		//auto sdp = this->get_remote_sdp();

		auto itr_fmt = sdp.rtpmap.begin();
		if (itr_fmt == sdp.rtpmap.end())
		{
			return nullptr;
		}


		return create_sender(itr_fmt->second);
	}

	sender_ptr media_stream::create_sender(const sdp_format& fmt)
	{
		std::unique_lock<std::shared_mutex>lk(senders_mutex_);

		uint32_t ssrc = this->get_local_ssrc();

		sender_ptr sender;
		if (fmt.codec == codec_type_h264)
		{
			sender = std::make_shared<sender_video_h264>(fmt.pt,ssrc, media_type(), fmt);
		}
		else if (fmt.codec == codec_type_h265)
		{
			sender = std::make_shared<sender_video_h265>(fmt.pt, ssrc, media_type(), fmt);
		}
		else if (fmt.codec == codec_type_vp8)
		{
			sender = std::make_shared <sender_video_vp8>(fmt.pt, ssrc, media_type(), fmt);
		}
		else if (fmt.codec == codec_type_ps)
		{
			sender = std::make_shared <sender_ps>(fmt.pt, ssrc, media_type(), fmt);
		}
		else if (fmt.codec == codec_type_mpeg4_generic || fmt.codec == codec_type_mp4a_latm)
		{
			sender = std::make_shared<sender_audio_aac>(fmt.pt, ssrc, media_type(), fmt);
		}
		else if (fmt.codec == codec_type_opus || fmt.codec == codec_type_pcma || fmt.codec == codec_type_pcmu || fmt.codec == codec_type_g722)
		{
			sender = std::make_shared<sender_audio>(fmt.pt, ssrc, media_type(), fmt);
		}
		else if (fmt.codec == codec_type_rtx)
		{
			sender = std::make_shared<sender_rtx>(fmt.pt,this->get_local_ssrc(1), media_type(), fmt);
		}
		else
		{
			return nullptr;
		}

		sender->send_rtp_packet_event_.add(s_send_rtp_packet_event, this);

		senders_.insert(std::make_pair(sender->pt(), sender));

		return sender;
	}

	std::vector<sender_ptr> media_stream::get_senders()
	{
		std::shared_lock<std::shared_mutex>lk(senders_mutex_);

		std::vector<sender_ptr> vec;
		vec.reserve(senders_.size());
		for (auto sender : senders_)
		{
			vec.push_back(sender.second);
		}
		return vec;
	}

	receiver_ptr media_stream::get_receiver(int pt)
	{
		{
			std::shared_lock<std::shared_mutex>lk(receivers_mutex_);
			auto itr = receivers_.find(pt);
			if (itr != receivers_.end())
			{
				return itr->second;
			}
		}

		{
			std::unique_lock<std::shared_mutex>lk(receivers_mutex_);

			auto itr = receivers_.find(pt);
			if (itr != receivers_.end())
			{
				return itr->second;
			}


			sdp_format fmt;
			if (!get_remote_format(pt, fmt))
				return nullptr;
			uint32_t ssrc = get_remote_ssrc();

			receiver_ptr receiver = create_receiver(ssrc, fmt);
			if (!receiver)
			{
				return nullptr;
			}
			receiver->rtp_frame_event_.add(s_rtp_frame_event, this);
			receiver->rtp_nack_event_.add(s_rtp_nack_event, this);
			receiver->rtp_keyframe_event_.add(s_rtp_keyframe_event, this);
			if (!receiver->start())
				return nullptr;
			receivers_.insert(std::make_pair(pt, receiver));

			return receiver;
		}
	}

	receiver_ptr media_stream::get_receiver_by_ssrc(uint32_t ssrc)
	{
		std::shared_lock<std::shared_mutex>lk(receivers_mutex_);
		for (auto itr = receivers_.begin(); itr != receivers_.end(); itr++)
		{
			if (itr->second->ssrc() == ssrc)
			{
				return itr->second;
			}
		}
		return nullptr;
	}

	std::vector<receiver_ptr> media_stream::get_receivers()
	{
		std::shared_lock<std::shared_mutex>lk(receivers_mutex_);

		std::vector<receiver_ptr> vec;
		vec.reserve(receivers_.size());
		for (auto receiver : receivers_)
		{
			vec.push_back(receiver.second);
		}
		return vec;
	}

	receiver_ptr media_stream::create_receiver(uint32_t ssrc, const sdp_format& fmt)
	{
		if (fmt.codec == codec_type_h264)
		{
			return std::make_shared<receiver_video_h264>(ssrc, media_type_video, fmt);
		}
		else if (fmt.codec == codec_type_h265)
		{
			return std::make_shared<receiver_video_h265>(ssrc, media_type_video, fmt);
		}
		else if (fmt.codec == codec_type_vp8)
		{
			return std::make_shared <receiver_video_vp8>(ssrc, media_type_video, fmt);
		}
		//else if (fmt.codec == codec_type_vp9)
		//{
		//	return std::make_shared <receiver_video_vp9>(ssrc, media_type_video, fmt);
		//}
		else if (fmt.codec == codec_type_ps)
		{
			return std::make_shared <receiver_ps>(ssrc, media_type_video, fmt);
		}
		else if (fmt.codec == codec_type_mpeg4_generic || fmt.codec == codec_type_mp4a_latm)
		{
			return std::make_shared <receiver_audio_aac>(ssrc, media_type_audio, fmt);
		}
		else if (fmt.codec == codec_type_opus|| fmt.codec == codec_type_pcma || fmt.codec == codec_type_pcmu|| fmt.codec == codec_type_g722)
		{
			return std::make_shared <receiver_audio>(ssrc, media_type_audio, fmt);
		}
		else if (fmt.codec == codec_type_rtx)
		{
			uint8_t apt = fmt.fmtp.apt();
			receiver_ptr apt_receiver;
			auto itr = receivers_.find(apt);
			if (itr != receivers_.end())
			{
				apt_receiver = itr->second;
			}
			
			if (apt_receiver) {
				return std::make_shared <receiver_rtx>(ssrc, apt_receiver->media_type(), fmt,apt_receiver);
			}
		}

		return nullptr;
	}

	bool media_stream::has_remote_ssrc(uint32_t ssrc)
	{
		std::shared_lock<std::shared_mutex>lk(remote_sdp_media_mutex_);
		if (remote_sdp_media_.ssrcs.size() == 0)
		{
			return true;
		}
		return remote_sdp_media_.has_ssrc(ssrc);
	}

	bool media_stream::get_rtx_local(uint8_t pt, uint8_t& rtx_pt, uint32_t& rtx_ssrc)const
	{
		std::shared_lock<std::shared_mutex>lk(local_sdp_media_mutex_);
		for(auto itr=local_sdp_media_.rtpmap.begin();itr!=local_sdp_media_.rtpmap.end();itr++)
		{
			if (itr->second.codec == codec_type_rtx)
			{
				if (itr->second.fmtp.apt() == pt)
				{
					rtx_pt = itr->first;
					if (local_sdp_media_.ssrcs.size() == 1) {
						rtx_ssrc = local_sdp_media_.ssrcs[0].ssrc;
					}
					else if (local_sdp_media_.ssrcs.size() > 1)
					{
						rtx_ssrc = local_sdp_media_.ssrcs[1].ssrc;
					}
					return true;
				}
			}
		}
		return false;
	}

	void media_stream::s_transport_dtls_handshake(void* ctx, srtp_role_t role)
	{
		media_stream* p = static_cast<media_stream*>(ctx);
		p->update_.update();
	}

	void media_stream::s_transport_rtp_packet(void* ctx, std::shared_ptr<sys::socket> skt, packet_ptr packet, const sockaddr* addr, int addr_size)
	{
		media_stream* p = (media_stream*)ctx;
		p->update_.update();
		auto receiver = p->get_receiver(packet->handle_->header->pt);
		if (!receiver)
		{
			return;
		}

		if (receiver->format().codec != codec_type_rtx)
		{
			if (p->use_rtp_addr_) {
				p->set_remote_rtp_endpoint(addr, addr_size, 1);
				p->use_rtp_addr_ = false;
			}
			// Fix ssrc by rtp packet
			if (p->remote_sdp_media_.ssrcs.size() > 0)
			{
				if (p->remote_sdp_media_.ssrcs[0].ssrc != packet->handle_->header->ssrc) {
					p->remote_sdp_media_.ssrcs[0].ssrc = packet->handle_->header->ssrc;
				}
			}
			else
			{
				litertp::ssrc_t ssrc = { packet->handle_->header->ssrc };
				p->remote_sdp_media_.ssrcs.push_back(ssrc);
			}
		}

		receiver->insert_packet(packet);

		
	}

	void media_stream::s_transport_rtcp_packet(void* ctx, std::shared_ptr<sys::socket> skt, uint16_t pt, const uint8_t* buffer, size_t size, const sockaddr* addr, int addr_size)
	{
		media_stream* p = (media_stream*)ctx;
		p->update_.update();

		if (p->use_rtcp_addr_) {
			p->set_remote_rtcp_endpoint(addr, addr_size, 1);
			p->use_rtcp_addr_ = false;
		}
		if (pt == rtcp_packet_type::RTCP_APP)
		{
			auto app = rtcp_app_create();
			if (rtcp_app_parse(app, buffer, size) >= 0)
			{
				if (p->has_remote_ssrc(app->ssrc))
				{
					p->on_rtcp_app(app);
				}
			}
			rtcp_app_free(app);
		}
		else if (pt == rtcp_packet_type::RTCP_BYE)
		{
			auto bye = rtcp_bye_create();
			if (rtcp_bye_parse(bye, buffer, size) >= 0)
			{
				for (unsigned int i = 0; i < bye->header.common.count; i++)
				{
					if (p->has_remote_ssrc(bye->src_ids[i])) 
					{
						p->on_rtcp_bye(bye);
					}
				}
			}
			rtcp_bye_free(bye);
		}
		else if (pt == rtcp_packet_type::RTCP_RR)
		{
			auto rr = rtcp_rr_create();
			if (rtcp_rr_parse(rr, buffer, size) >= 0)
			{
				if (p->has_remote_ssrc(rr->ssrc))
				{
					p->on_rtcp_rr(rr);
				}
			}
			rtcp_rr_free(rr);
		}
		else if (pt == rtcp_packet_type::RTCP_SR)
		{
			auto sr = rtcp_sr_create();
			if (rtcp_sr_parse(sr, buffer, size) >= 0)
			{
				if (p->has_remote_ssrc(sr->ssrc))
				{
					p->on_rtcp_sr(sr);
				}
			}
			rtcp_sr_free(sr);
		}
		else if (pt == rtcp_packet_type::RTCP_SDES)
		{
			auto sdes = rtcp_sdes_create();
			if (rtcp_sdes_parse(sdes, buffer, size) >= 0)
			{
				p->on_rtcp_sdes(sdes);
			}
			rtcp_sdes_free(sdes);
		}
		else if (pt == rtcp_packet_type::RTCP_RTPFB)
		{
			auto fb = rtcp_fb_create();
			if (rtcp_fb_parse(fb, buffer, size) >= 0)
			{
				if(p->has_remote_ssrc(fb->ssrc_sender))
				{
					if (fb->header.app.subtype == RTCP_RTPFB_FMT_NACK)
					{
						uint16_t pid = 0, bld = 0;
						rtcp_rtpfb_nack_get(fb, &pid, &bld);

						p->on_rtcp_nack(fb->ssrc_media, pid, bld);
					}
				}
			}
			rtcp_fb_free(fb);
		}
		else if (pt == rtcp_packet_type::RTCP_PSFB)
		{
			auto fb = rtcp_fb_create();
			if (rtcp_fb_parse(fb, buffer, size) >= 0)
			{
				if (p->has_remote_ssrc(fb->ssrc_sender))
				{
					if (fb->header.app.subtype == RTCP_PSFB_FMT_PLI)
					{
						//if (fb->ssrc_media == p->get_local_ssrc(0))
						//{
							p->on_rtcp_pli(fb->ssrc_media);
						//}
						//else
						//{
						//	LOGW("received unmatched ssrc pli,sender=%u,media=%u\n", fb->ssrc_sender, fb->ssrc_media);
						//}
					}
					else if (fb->header.app.subtype == RTCP_PSFB_FMT_FIR)
					{
						//if (fb->ssrc_media == p->get_local_ssrc(0))
						//{
							rtcp_psfb_fir_item item;
							rtcp_psfb_fir_get_item(fb, 0, &item);
							p->on_rtcp_fir(item.ssrc, item.seq_nr);
						//}
						//else
						//{
						//	LOGW("received unmatched ssrc fir,sender=%u,media=%u", fb->ssrc_sender, fb->ssrc_media);
						//}
					}
				}
			}
			rtcp_fb_free(fb);
		}
	}

	void media_stream::s_transport_stun_message(void* ctx, std::shared_ptr<sys::socket> skt, const stun_message& msg, const sockaddr* addr, int addr_size)
	{
		media_stream* p = (media_stream*)ctx;
		p->update_.update();

		if (msg.type_ == stun_message_type_binding_request)
		{
			std::string spriority=msg.get_attribute(stun_attribute_type_priority);
			uint32_t priority = 0;
			if (spriority.size() >= 4)
			{
				priority |= ((uint8_t)spriority[0]) << 24;
				priority |= ((uint8_t)spriority[1]) << 16;
				priority |= ((uint8_t)spriority[2]) << 8;
				priority |= ((uint8_t)spriority[3]);
			}

			
			if (p->transport_rtp_->socket_ == skt)
			{
				p->set_remote_rtp_endpoint(addr, addr_size, priority);
			}

			if (p->transport_rtcp_->socket_ == skt)
			{
				p->set_remote_rtcp_endpoint(addr, addr_size, priority);
			}
		}
	}

	void media_stream::s_transport_raw_packet(void* ctx, std::shared_ptr<sys::socket> skt, const uint8_t* raw_data, size_t size, const sockaddr* addr, int addr_size)
	{
		media_stream* p = static_cast<media_stream*>(ctx);
		p->update_.update();
		//std::string mid = p->mid();
		//p->on_app_data_received.invoke(mid.c_str(), raw_data, size);
	}

	void media_stream::s_sctp_connected(void* ctx)
	{
		media_stream* p = static_cast<media_stream*>(ctx);
		p->update_.update();
		if(p->sdp_type_ == sdp_type_offer)
		{
			if (p->transport_rtp_->sctp_)
			{
				p->transport_rtp_->sctp_->create_stream("signal",p->sctp_stream_id_);
			}
		}
	}

	void media_stream::s_sctp_closed(void* ctx)
	{
		media_stream* p = static_cast<media_stream*>(ctx);
		p->on_data_channel_close.invoke(p->mid().c_str());
	}

	void media_stream::s_sctp_heartbeat(void* ctx)
	{
		media_stream* p = static_cast<media_stream*>(ctx);
		p->update_.update();
	}

	void media_stream::s_sctp_stream_open(void* ctx, uint16_t stream_id)
	{
		media_stream* p = static_cast<media_stream*>(ctx);
		p->update_.update();
		p->sctp_stream_id_ = stream_id;
		p->on_data_channel_open.invoke(p->mid().c_str());
	}

	void media_stream::s_sctp_stream_closed(void* ctx, uint16_t stream_id)
	{
		media_stream* p = static_cast<media_stream*>(ctx);
		p->on_data_channel_close.invoke(p->mid().c_str());
	}

	void media_stream::s_sctp_stream_message(void* ctx, uint16_t stream_id, const std::vector<uint8_t>& message)
	{
		media_stream* p = static_cast<media_stream*>(ctx);
		p->update_.update();
		if (p->sctp_stream_id_ == stream_id) {
			p->on_data_channel_message.invoke(p->mid().c_str(), message.data(), message.size());
		}
	}


	void media_stream::on_rtcp_app(const rtcp_app* app)
	{
	}

	void media_stream::on_rtcp_bye(const rtcp_bye* bye)
	{
		
	}

	void media_stream::on_rtcp_sr(const rtcp_sr* sr)
	{
		auto senders = get_senders();
		for (auto sender : senders)
		{
			if(sender->format().codec == codec_type_rtx)
			{
				continue;
			}
			rtcp_report* report=rtcp_sr_find_report(sr,sender->ssrc());
			if (report)
			{
				sender->update_remote_report(*report);
			}
		}

		auto receivers = get_receivers();
		for (auto receiver : receivers)
		{
			if (receiver->format().codec == codec_type_rtx)
			{
				continue;
			}
			if (receiver->ssrc() == sr->ssrc)
			{
				receiver->update_remote_sr(*sr);
			}
		}

		LOGT("ssrc %d send report", sr->ssrc);
	}

	void media_stream::on_rtcp_rr(const rtcp_rr* rr)
	{

		auto senders = get_senders();
		for (auto sender : senders)
		{
			if (sender->format().codec == codec_type_rtx)
			{
				continue;
			}

			rtcp_report* report = rtcp_rr_find_report(rr, sender->ssrc());
			if (report)
			{
				sender->update_remote_report(*report);
			}
		}

		LOGT("ssrc %d receive report", rr->ssrc);
	}

	void media_stream::on_rtcp_sdes(const rtcp_sdes* sdes)
	{

	}

	void media_stream::on_rtcp_nack(uint32_t ssrc, uint16_t pid, uint16_t bld)
	{
		LOGD("recevied rtcp nack ssrc=%u seq=%u\n", ssrc,pid);
		auto sender=this->get_default_sender();
		if (sender)
		{
			uint8_t rtx_pt = 0;
			uint32_t rtx_ssrc = 0;
			sender_ptr rtx_sender;
			if (this->get_rtx_local(sender->pt(), rtx_pt, rtx_ssrc)) {
				rtx_sender = this->get_sender(rtx_pt);
			}


			auto pkt = sender->get_history(pid);
			if (pkt)
			{
				if (rtx_sender) {
					rtx_sender->send_packet(pkt);
				}
				else {
					this->send_rtp_packet(pkt);
				}
			}

			for (int i = 0; i < 16; i++)
			{
				bld = bld >> i;
				if (bld & 0x0001)
				{
					
					pkt = sender->get_history(pid + i + 1);
					if (pkt)
					{
						if (rtx_sender) {
							rtx_sender->send_packet(pkt);
						}
						else {
							this->send_rtp_packet(pkt);
						}
					}
				}
			}

			sender->increase_nack();
		}

	}

	void media_stream::on_rtcp_pli(uint32_t ssrc)
	{
		LOGD("ssrc %u required keyframe by pli\n",ssrc);

		auto sender = this->get_default_sender();
		if (sender)
		{
			sender->increase_pli();
		}

		on_received_require_keyframe.invoke(ssrc, 0);
	}

	void media_stream::on_rtcp_fir(uint32_t ssrc, uint8_t nr)
	{
		LOGD("ssrc %u required keyframe by fir\n",ssrc);

		auto sender = this->get_default_sender();
		if (sender)
		{
			sender->increase_fir();
		}

		on_received_require_keyframe.invoke(ssrc, 1);
	}



	void media_stream::s_rtp_frame_event(void* ctx, uint32_t ssrc, const sdp_format& fmt, const av_frame_t& frame)
	{
		media_stream* p = (media_stream*)ctx;
		p->on_rtp_frame_event(ssrc, fmt, frame);
	}
	void media_stream::on_rtp_frame_event(uint32_t ssrc, const sdp_format& fmt, const av_frame_t& frame)
	{
		std::string media_id=mid();
		on_frame_received.invoke(media_id.c_str(), ssrc, fmt.pt, fmt.frequency, fmt.channels, &frame);
	}


	void media_stream::s_rtp_nack_event(void* ctx, uint32_t ssrc, const sdp_format& fmt, uint16_t pid, uint16_t bld)
	{
		media_stream* p = (media_stream*)ctx;
		p->on_rtp_nack_event(ssrc, fmt, pid, bld);
	}
	void media_stream::on_rtp_nack_event(uint32_t ssrc, const sdp_format& fmt, uint16_t pid, uint16_t bld)
	{
		LOGD("send nack pid=%u bld=%x\n",pid,bld);
		this->send_rtcp_nack(get_local_ssrc(), ssrc, pid, bld);
	}


	void media_stream::s_rtp_keyframe_event(void* ctx, uint32_t ssrc, const sdp_format& fmt)
	{
		media_stream* p = (media_stream*)ctx;
		p->on_rtp_keyframe_event(ssrc, fmt);
	}
	void media_stream::on_rtp_keyframe_event(uint32_t ssrc, const sdp_format& fmt)
	{
		this->send_rtcp_keyframe(ssrc,fmt.pt);
	}

	void media_stream::s_send_rtp_packet_event(void* ctx, packet_ptr packet)
	{
		media_stream* p = (media_stream*)ctx;
		p->send_rtp_packet(packet);
	}



	

}
