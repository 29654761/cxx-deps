/**
 * @file media_stream.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#include "media_stream.h"
#include <sys2/util.h>

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


namespace rtpx {

	media_stream::media_stream(asio::io_context& ioc,media_type_t mt, rtp_trans_mode_t trans_mode, transport_ptr rtp, transport_ptr rtcp, spdlogger_ptr log)
		:ioc_(ioc)
	{
		log_ = log;
		media_type_ = mt;
		trans_mode_ = trans_mode;
		local_rtp_ssrc_= sys::util::random_number<uint32_t>(0x1, 0xFFFFFFFF);
		if (mt == media_type_video||mt==media_type_audio) {
			local_rtx_ssrc_ = sys::util::random_number<uint32_t>(0x1, 0xFFFFFFFF);
		}
		

		rtp_transport_ = rtp;
		rtcp_transport_ = rtcp;

		is_rtcp_mux_ = rtp_transport_ == rtcp_transport_;

	}
	media_stream::~media_stream()
	{
	}

	void media_stream::close()
	{
		{
			std::unique_lock<std::shared_mutex> lk(local_mutex_);
			senders_.clear();
		}

		{
			std::unique_lock<std::shared_mutex> lk(remote_mutex_);
			for (auto& r : receivers_)
			{
				r.second->stop();
			}
			receivers_.clear();
		}
	}

	bool media_stream::add_local_video_track(codec_type_t codec, uint8_t pt, int frequency, bool rtcp_fb)
	{
		std::unique_lock<std::shared_mutex> lk(local_mutex_);
		if (media_type_ != media_type_video)
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
		auto r=local_formats_.insert(std::make_pair((int)pt, fmt));
		return r.second;
	}

	bool media_stream::add_local_audio_track(codec_type_t codec, uint8_t pt, int frequency, int channels, bool rtcp_fb)
	{
		std::unique_lock<std::shared_mutex> lk(local_mutex_);
		if (media_type_ != media_type_audio)
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
		auto r = local_formats_.insert(std::make_pair(pt, fmt));
		return r.second;
	}

	bool media_stream::add_local_rtx_track(uint8_t pt, uint8_t apt, int frequency, bool rtcp_fb)
	{
		std::unique_lock<std::shared_mutex> lk(local_mutex_);

		if (local_formats_.find(apt) == local_formats_.end()) {
			return false;
		}

		sdp_format fmt(pt, codec_type_rtx, frequency);
		if (rtcp_fb)
		{
			fmt.rtcp_fb.insert("goog-remb");
			//fmt.rtcp_fb.insert("transport-cc");
			fmt.rtcp_fb.insert("ccm fir");
			fmt.rtcp_fb.insert("nack");
			fmt.rtcp_fb.insert("nack pli");
		}
		fmt.fmtp.set_apt(apt);
		auto r=local_formats_.insert(std::make_pair(pt, fmt));
		return r.second;
	}

	bool media_stream::add_remote_video_track(codec_type_t codec, uint8_t pt, int frequency, bool rtcp_fb)
	{
		std::unique_lock<std::shared_mutex> lk(remote_mutex_);
		if (media_type_ != media_type_video)
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
		auto r = remote_formats_.insert(std::make_pair((int)pt, fmt));
		return r.second;
	}

	bool media_stream::add_remote_audio_track(codec_type_t codec, uint8_t pt, int frequency, int channels, bool rtcp_fb)
	{
		std::unique_lock<std::shared_mutex> lk(remote_mutex_);
		if (media_type_ != media_type_audio)
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
		auto r = remote_formats_.insert(std::make_pair((int)pt, fmt));
		return r.second;
	}

	bool media_stream::add_remote_rtx_track(uint8_t pt, uint8_t apt, int frequency, bool rtcp_fb)
	{
		std::unique_lock<std::shared_mutex> lk(remote_mutex_);

		if (remote_formats_.find(apt) == remote_formats_.end()) {
			return false;
		}

		sdp_format fmt(pt, codec_type_rtx, frequency);
		if (rtcp_fb)
		{
			fmt.rtcp_fb.insert("goog-remb");
			//fmt.rtcp_fb.insert("transport-cc");
			fmt.rtcp_fb.insert("ccm fir");
			fmt.rtcp_fb.insert("nack");
			fmt.rtcp_fb.insert("nack pli");
		}
		fmt.fmtp.set_apt(apt);
		auto r = remote_formats_.insert(std::make_pair((int)pt, fmt));
		return r.second;
	}


	void media_stream::add_local_data_channel_track(uint16_t sctp_port)
	{
		std::unique_lock<std::shared_mutex> lk(local_mutex_);
		app_proto_ = "webrtc-datachannel";
		sctp_port_ = sctp_port;

		rtp_transport_->sctp_init(sctp_port_);
	}

	bool media_stream::set_local_rtpmap_fmtp(uint8_t pt, const fmtp& fmtp)
	{
		std::unique_lock<std::shared_mutex> lk(local_mutex_);
		auto itr = local_formats_.find(pt);
		if (itr == local_formats_.end())
			return false;

		itr->second.fmtp = fmtp;
		return true;
	}

	bool media_stream::set_local_rtpmap_attribute(uint8_t pt, const std::string& key, const std::string& val)
	{
		std::unique_lock<std::shared_mutex> lk(local_mutex_);
		auto itr=local_formats_.find(pt);
		if (itr == local_formats_.end())
			return false;

		itr->second.fmtp.set(key, val);
		return true;
	}

	bool media_stream::clear_local_rtpmap_attributes(uint8_t pt)
	{
		std::unique_lock<std::shared_mutex> lk(local_mutex_);
		auto itr = local_formats_.find(pt);
		if (itr == local_formats_.end())
			return false;
		itr->second.fmtp.clear();
		return true;
	}

	bool media_stream::set_remote_rtpmap_attribute(uint8_t pt, const std::string& key, const std::string& val)
	{
		std::unique_lock<std::shared_mutex> lk(remote_mutex_);
		auto itr = remote_formats_.find(pt);
		if (itr == remote_formats_.end())
			return false;

		itr->second.fmtp.set(key, val);
		return true;
	}

	bool media_stream::clear_remote_rtpmap_attributes(uint8_t pt)
	{
		std::unique_lock<std::shared_mutex> lk(remote_mutex_);
		auto itr = remote_formats_.find(pt);
		if (itr == remote_formats_.end())
			return false;

		itr->second.fmtp.clear();
		return true;
	}


	bool media_stream::create_offer(sdp_media& sdp)
	{
		std::shared_lock<std::shared_mutex> lk(local_mutex_);
		sdp_role_ = sdp_type_offer;
		local_setup_ =sdp_setup_actpass;

		return to_local_sdp(sdp);
	}

	bool media_stream::create_answer(sdp_media& sdp)
	{
		std::shared_lock<std::shared_mutex> lk(local_mutex_);
		std::shared_lock<std::shared_mutex> lk2(remote_mutex_);

		if (sdp_role_ != sdp_type_answer)
		{
			if (log_)
			{
				log_->warn("Create asnwer failed: The SDP role must be answer,but now is {}. mid={},mt={}",
					sdp_role_,mid_,media_type_)->flush();
			}
			return false;
		}
		return to_local_sdp(sdp);
	}

	bool media_stream::set_remote_sdp(const sdp_media& sdp, sdp_type_t type)
	{
		std::unique_lock<std::shared_mutex> lk(remote_mutex_);
		std::unique_lock<std::shared_mutex> lk2(local_mutex_);

		set_remote_ice_options(sdp.ice_ufrag, sdp.ice_pwd, sdp.ice_options);

		if (sdp.ssrcs.size() > 0) {
			remote_rtp_ssrc_ = sdp.ssrcs[0].ssrc;
		}
		if (sdp.ssrcs.size() > 1) {
			remote_rtx_ssrc_ = sdp.ssrcs[1].ssrc;
		}
		remote_setup_ = sdp.setup;
		if (remote_setup_ == sdp_setup_none)
		{
			if (log_)
			{
				log_->warn("Set remote SDP failed: The remote parties missing setup.mid={},mt={}",
					mid_, media_type_)->flush();
			}
			return false;
		}

		if (type == sdp_type_offer)
		{
			sdp_role_ = sdp_type_answer;
			mid_ = sdp.mid;
			msid_sid_ = sdp.msid_sid;
			msid_tid_ = sdp.msid_tid;
			app_proto_ = sdp.app_data_type;

			std::string sctp_port_str=sdp.get_attribute("sctp-port", "0");
			char* endptr = nullptr;
			sctp_port_ = (uint16_t)strtol(sctp_port_str.c_str(), &endptr, 10);

			if (sdp.setup == sdp_setup_actpass)
			{
				local_setup_ = sdp_setup_active;
			}
			else if (sdp.setup == sdp_setup_active)
			{
				local_setup_ = sdp_setup_passive;
			}
			else if (sdp.setup == sdp_setup_passive)
			{
				local_setup_ = sdp_setup_active;
			}

			if (sdp.trans_mode == rtp_trans_mode_sendrecv) {
				trans_mode_ = rtp_trans_mode_sendrecv;
			}
			else if (sdp.trans_mode == rtp_trans_mode_sendonly)
			{
				trans_mode_ = rtp_trans_mode_recvonly;
			}
			else if (sdp.trans_mode == rtp_trans_mode_recvonly)
			{
				trans_mode_ = rtp_trans_mode_sendonly;
			}
			else
			{
				if (log_)
				{
					log_->warn("Set remote SDP failed: The trans_made of both parties is incorrect. local={},remote={},mid={},mt={}",
						trans_mode_, sdp.trans_mode,mid_,media_type_)->flush();
				}
				return false;
			}

			remote_formats_ = sdp.rtpmap;
			local_formats_ = sdp.rtpmap;

			if (sdp.ssrcs.size() > 0) {
				remote_rtp_ssrc_ = sdp.ssrcs[0].ssrc;
				if (sdp.ssrcs.size() > 1) {
					remote_rtx_ssrc_ = sdp.ssrcs[1].ssrc;
				}
			}

			if (sdp.media_type == media_type_application)
			{
				if (sdp.app_data_type == "webrtc-datachannel")
				{
					rtp_transport_->sctp_set_remote_port(sctp_port_);
					rtp_transport_->sctp_init(sctp_port_);
				}
			}

			security_ = sdp.is_security();
			rtp_transport_->enable_dtls(security_);
			rtcp_transport_->enable_dtls(security_);
			
		}
		else if (type == sdp_type_answer)
		{
			if (!negotiate(sdp)) {
				return false;
			}
		}
		else
		{
			if (log_)
			{
				log_->warn("Set remote SDP failed: Unknown SDP type {},mid={},mt={}", sdp_role_,mid_,media_type_)->flush();
			}
			return false;
		}

		if (sdp.candidates.size() > 0)
		{
			remote_candidates_=sdp.candidates;
		}
		else
		{
			std::error_code ec;
			asio::ip::udp::endpoint ep(asio::ip::address::from_string(sdp.rtp_address, ec), sdp.rtp_port);
			if (!ec)
			{
				remote_rtp_endpoint_priority_ = 0;
				remote_rtp_endpoint_ = ep;
			}
			
			if (sdp.rtcp_mux)
			{
				remote_rtcp_endpoint_ = remote_rtp_endpoint_;
				remote_rtcp_endpoint_priority_ = remote_rtp_endpoint_priority_;
			}
			else
			{
				asio::ip::udp::endpoint ep2(asio::ip::address::from_string(sdp.rtcp_address, ec), sdp.rtcp_port);
				if (!ec)
				{
					remote_rtcp_endpoint_priority_ = 0;
					remote_rtcp_endpoint_ = ep;
				}
			}
		}

		return true;
	}

	bool media_stream::negotiate(const sdp_media& remote_sdp)
	{
		senders_.clear();
		for (auto& r : receivers_)
		{
			r.second->stop();
		}
		receivers_.clear();

		if (media_type_ != remote_sdp.media_type)
		{
			if (log_)
			{
				log_->warn("Negotiate failed: Unmatched miedia type. local={},remote={},mid={}",
					media_type_, remote_sdp.media_type,mid_)->flush();
			}
			return false;
		}

		if (media_type_ == media_type_application)
		{
			if (app_proto_ != remote_sdp.app_data_type)
			{
				if (log_)
				{
					log_->warn("Negotiate failed: Unmatched app proto. local={},remote={},mid={},mt={}",
						app_proto_, remote_sdp.app_data_type,mid_,media_type_)->flush();
				}
				return false;
			}
		}

		if (remote_sdp.setup == local_setup_)
		{
			if (log_)
			{
				log_->warn("Negotiate failed: The setup of both parties is incorrect, both is {}. mid={},mt={}",
					local_setup_,mid_,media_type_)->flush();
			}
			return false;
		}
		if (remote_sdp.trans_mode == trans_mode_ && trans_mode_ != rtp_trans_mode_sendrecv)
		{
			if (log_)
			{
				log_->warn("Negotiate failed: The trans_made of both parties is incorrect. local={},remote={},role={},mid={},mt={}",
					trans_mode_, remote_sdp.trans_mode,sdp_role_,mid_,media_type_)->flush();
			}
			return false;
		}
		if (remote_sdp.is_security())
		{
			if (!security_)
			{
				if (log_)
				{
					log_->warn("Negotiate failed: The security of both parties is incorrect. local=false,remote=true,mid={},mt={}",mid_,media_type_)->flush();
				}
				return false;
			}
			if (remote_sdp.fingerprint.size() == 0)
			{
				if (log_)
				{
					log_->warn("Negotiate failed: No fingerprint.mid={},mt={}", mid_, media_type_)->flush();
				}
				return false;
			}
		}
		else
		{
			if (security_)
			{
				if (log_)
				{
					log_->warn("Negotiate failed: The security of both parties is incorrect. local=true,remote=false,mid={},mt={}",mid_,media_type_)->flush();
				}
				return false;
			}
		}

		std::vector<sdp_format> rfmts;
		std::set<uint8_t> not_support_rtx;
		rfmts.reserve(remote_sdp.rtpmap.size());
		for (auto itr = local_formats_.begin(); itr != local_formats_.end();)
		{
			if (itr->second.codec == codec_type_rtx) {
				itr++;
				continue;
			}

			std::vector<sdp_format> ofmts;
			if (remote_sdp.negotiate(itr->second, ofmts))
			{
				rfmts.insert(rfmts.end(), ofmts.begin(),ofmts.end());

				if (ofmts.size() < 2) {
					not_support_rtx.insert(itr->first);
				}


				itr++;
			}
			else
			{
				itr = local_formats_.erase(itr);
			}
		}


		for (const auto& rfmt : rfmts)
		{
			remote_formats_.insert(std::make_pair(rfmt.pt, rfmt));
		}

		for (auto itr = local_formats_.begin();itr!=local_formats_.end();)
		{
			itr->second.rtcp_fb.erase("transport-cc"); // Not suport yet.

			if (itr->second.codec == codec_type_rtx)
			{
				//Remove rtx which not support
				if (not_support_rtx.find(itr->second.fmtp.apt()) != not_support_rtx.end())
				{
					itr = local_formats_.erase(itr);
					continue;
				}

				//Remove rtx which no apt pt
				if (local_formats_.find(itr->second.fmtp.apt()) == local_formats_.end())
				{
					itr = local_formats_.erase(itr);
					continue;
				}
			}

			itr++;
		}

		for (auto itr = remote_formats_.begin(); itr != remote_formats_.end();)
		{
			itr->second.rtcp_fb.erase("transport-cc"); // Not suport yet.

			//Remove rtx which no apt pt
			if (itr->second.codec == codec_type_rtx
				&& remote_formats_.find(itr->second.fmtp.apt()) == remote_formats_.end())
			{
				itr = remote_formats_.erase(itr);
			}
			else
			{
				itr++;
			}
		}

		if (media_type_ != media_type_application)
		{
			if (local_formats_.size() == 0 || remote_formats_.size() == 0)
			{
				if (log_)
				{
					log_->warn("Negotiate failed: Not matched any formats . local={},remote={},mid={},mt={}",
						local_formats_.size(), remote_formats_.size(), mid_, media_type_)->flush();
				}
				return false;
			}
		}
		return true;
	}

	bool media_stream::to_local_sdp(sdp_media& local_sdp)
	{
		local_sdp.mid = mid_;
		local_sdp.protos.clear();
		local_sdp.protos.insert("UDP");
		local_sdp.media_type = media_type_;
		local_sdp.app_data_type = app_proto_;

		local_sdp.has_rtp_address = true;
		local_sdp.has_rtcp = true;
		local_sdp.rtcp_mux = is_rtcp_mux_;
		
		if (media_type_ == media_type_application)
		{
			if (security_)
			{
				local_sdp.protos.insert("DTLS");
			}
			if (app_proto_ == "webrtc-datachannel")
			{
				local_sdp.protos.insert("SCTP");
				local_sdp.app_data_type = "webrtc-datachannel";
				local_sdp.attrs.push_back(sdp_pair("sctp-port", std::to_string(sctp_port_), ":"));
				local_sdp.attrs.push_back(sdp_pair("max-message-size", "1073741823", ":"));
			}
			else
			{
				if (log_)
				{
					log_->error("Generate SDP failed: unknown app proto. mid={},mt={}",mid_,media_type_)->flush();
				}
				return false;
			}

		}
		else
		{
			local_sdp.protos.insert("RTP");
			if (security_)
			{
				local_sdp.protos.insert("TLS");
				local_sdp.protos.insert("SAVPF");
			}
			else
			{
				local_sdp.protos.insert("AVP");
				local_sdp.protos.insert("AVPF");
			}
			local_sdp.msid_sid = msid_sid_;
			local_sdp.msid_tid = msid_tid_;
			local_sdp.rtcp_rsize = true;
		}
		
		local_sdp.fingerprint = this->rtp_transport_->fingerprint();
		local_sdp.ice_ufrag = local_ice_ufrag_;
		local_sdp.ice_pwd = local_ice_pwd_;
		local_sdp.ice_options = local_ice_options_;

		local_sdp.trans_mode = trans_mode_;
		local_sdp.setup = local_setup_;

		local_sdp.rtpmap = local_formats_;



		if (media_type_ != media_type_application
			&&(trans_mode_==rtp_trans_mode_sendonly||trans_mode_==rtp_trans_mode_sendrecv))
		{
			ssrc_t st = {};
			st.ssrc = local_rtp_ssrc_;
			st.cname = cname_;
			st.msid_stream_id = msid_sid_;
			st.msid_track_id = msid_tid_;
			local_sdp.ssrcs.push_back(st);
			if (local_rtx_ssrc_ > 0)
			{
				st.ssrc = local_rtx_ssrc_;
				st.cname = cname_;
				st.msid_stream_id = msid_sid_;
				st.msid_track_id = msid_tid_;
				local_sdp.ssrcs.push_back(st);
				local_sdp.ssrc_group = "FID";
			}
		}

		local_sdp.candidates = local_candidates_;

		return true;
	}

	std::vector<sdp_format> media_stream::local_formats()const
	{
		std::vector<sdp_format> vec;
		std::shared_lock<std::shared_mutex> lock(local_mutex_);
		vec.reserve(local_formats_.size());
		for (auto& fmt : local_formats_)
		{
			vec.push_back(fmt.second);
		}
		return vec;
	}

	std::vector<sdp_format> media_stream::remote_formats()const
	{
		std::vector<sdp_format> vec;
		std::shared_lock<std::shared_mutex> lock(remote_mutex_);
		vec.reserve(remote_formats_.size());
		for (auto& fmt : remote_formats_)
		{
			vec.push_back(fmt.second);
		}
		return vec;
	}

	bool media_stream::send_frame(uint8_t pt, const uint8_t* frame, uint32_t size, uint32_t duration)
	{
		auto tm = trans_mode();
		if (tm != rtp_trans_mode_sendonly && tm != rtp_trans_mode_sendrecv)
			return false;

		auto sender = get_sender(pt);
		if (!sender)
		{
			return false;
		}

		return sender->send_frame(frame, size, duration);
	}

	bool media_stream::send_rtp_packet(packet_ptr packet)
	{
		asio::ip::udp::endpoint ep;
		if (!remote_rtp_endpoint(ep))
			return false;


		return rtp_transport_->send_rtp_packet(packet,ep);
	}

	bool media_stream::send_rtcp_packet(const uint8_t* rtcp_data, int size)
	{
		asio::ip::udp::endpoint ep;
		if (!remote_rtcp_endpoint(ep))
			return false;

		return rtcp_transport_->send_rtcp_packet(rtcp_data, size, ep);
	}

	bool media_stream::send_data_channel(const uint8_t* data, int size)
	{
		auto tm = trans_mode();
		if (tm != rtp_trans_mode_sendonly && tm != rtp_trans_mode_sendrecv)
			return false;

		return rtp_transport_->sctp_send(sctp_stream_id_,data,size);
	}

	void media_stream::send_rtcp_keyframe(uint32_t ssrc_media)
	{
		uint32_t ssrc = local_rtp_ssrc();

		std::shared_lock<std::shared_mutex> lock(remote_mutex_);
		for (auto itr_fmt = remote_formats_.begin(); itr_fmt != remote_formats_.end(); itr_fmt++)
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

	void media_stream::send_rtcp_keyframe(uint32_t ssrc_media, uint8_t pt)
	{
		uint32_t ssrc = local_rtp_ssrc();


		sdp_format fmt;
		if (!remote_format(pt, fmt))
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
		if (on_send_require_keyframe) {
			on_send_require_keyframe(mid_);
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
		int size = rtcp_fb_serialize(fb, buffer, sizeof(buffer));
		rtcp_fb_free(fb);
		if (size > 0)
		{
			send_rtcp_packet(buffer, size);
		}
	}

	void media_stream::send_rtcp_keyframe_fir(uint32_t ssrc_sender, uint32_t ssrc_media)
	{
		if (on_send_require_keyframe) {
			on_send_require_keyframe(mid_);
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
		rtcp_bye* pkt = rtcp_bye_create();
		if (!pkt)
			return;

		pkt->message = (char*)reason;

		auto ss = this->senders();

		pkt->src_ids = (uint32_t*)malloc(ss.size()*sizeof(uint32_t));
		if (!pkt->src_ids)
			return;

		for (auto s : ss)
		{
			rtcp_bye_add_source(pkt, s->ssrc());
		}

		uint8_t buffer[2048] = { 0 };
		int size = rtcp_bye_serialize(pkt, buffer, sizeof(buffer));
		rtcp_bye_free(pkt);
		if (size > 0)
		{
			send_rtcp_packet(buffer, size);
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
		int size = rtcp_fb_serialize(fb, buffer, sizeof(buffer));
		rtcp_fb_free(fb);
		if (size > 0)
		{
			send_rtcp_packet(buffer, size);
		}

		if (log_)
		{
			log_->debug("Send RTCP Nack ssrc={},pid={}", ssrc_media,pid)->flush();
		}
		
		auto receiver = get_receiver_by_ssrc(ssrc_media);
		if (receiver)
		{
			receiver->increase_nack();
		}
		
	}

	uint32_t media_stream::remote_rtp_ssrc()const
	{
		std::shared_lock<std::shared_mutex> lock(remote_mutex_);
		return remote_rtp_ssrc_;
	}
	uint32_t media_stream::remote_rtx_ssrc()const
	{
		std::shared_lock<std::shared_mutex> lock(remote_mutex_);
		return remote_rtx_ssrc_;
	}

	void media_stream::set_remote_rtp_ssrc(uint32_t ssrc)
	{
		std::unique_lock<std::shared_mutex> lock(remote_mutex_);
		remote_rtp_ssrc_ = ssrc;
	}

	void media_stream::set_remote_rtx_ssrc(uint32_t ssrc)
	{
		std::unique_lock<std::shared_mutex> lock(remote_mutex_);
		remote_rtx_ssrc_ = ssrc;
	}

	void media_stream::set_local_ice_options(const std::string& ufrag, const std::string& pwd, const std::string& options)
	{
		local_ice_ufrag_ = ufrag;
		local_ice_pwd_ = pwd;
		local_ice_options_ = options;

		rtp_transport_->set_local_ice_options(ufrag, pwd);
		if (rtcp_transport_ != rtp_transport_)
		{
			rtcp_transport_->set_local_ice_options(ufrag, pwd);
		}
	}

	void media_stream::set_remote_ice_options(const std::string& ufrag, const std::string& pwd, const std::string& options)
	{
		remote_ice_ufrag_ = ufrag;
		remote_ice_pwd_ = pwd;
		remote_ice_options_ = options;

		rtp_transport_->set_remote_ice_options(ufrag, pwd);
		if (rtcp_transport_ != rtp_transport_)
		{
			rtcp_transport_->set_remote_ice_options(ufrag, pwd);
		}
	}

	void media_stream::set_remote_rtp_endpoint(const asio::ip::udp::endpoint& endpoint, int64_t priority)
	{
		{
			std::shared_lock<std::shared_mutex> lock(remote_mutex_);
			if (priority <= remote_rtp_endpoint_priority_)
				return;
		}

		{
			std::unique_lock<std::shared_mutex> lock(remote_mutex_);
			if (priority <= remote_rtp_endpoint_priority_)
				return;

			remote_rtp_endpoint_priority_ = priority;
			remote_rtp_endpoint_ = endpoint;
		}

	}

	void media_stream::set_remote_rtcp_endpoint(const asio::ip::udp::endpoint& endpoint, int64_t priority)
	{
		{
			std::shared_lock<std::shared_mutex> lock(remote_mutex_);
			if (priority <= remote_rtcp_endpoint_priority_)
				return;
		}

		{
			std::unique_lock<std::shared_mutex> lock(remote_mutex_);
			if (priority <= remote_rtcp_endpoint_priority_)
				return;

			remote_rtcp_endpoint_priority_ = priority;
			remote_rtcp_endpoint_ = endpoint;
		}
	}

	bool media_stream::remote_rtp_endpoint(asio::ip::udp::endpoint& endpoint, int64_t* priority)const
	{
		std::shared_lock<std::shared_mutex> lock(remote_mutex_);
		if (remote_rtp_endpoint_priority_ < 0)
			return false;
		endpoint = remote_rtp_endpoint_;
		if (priority) {
			*priority = remote_rtp_endpoint_priority_;
		}
		return true;
	}

	bool media_stream::remote_rtcp_endpoint(asio::ip::udp::endpoint& endpoint, int64_t* priority)const
	{
		std::shared_lock<std::shared_mutex> lock(remote_mutex_);
		if (remote_rtcp_endpoint_priority_ < 0)
			return false;
		endpoint = remote_rtcp_endpoint_;
		if (priority) {
			*priority = remote_rtcp_endpoint_priority_;
		}
		return true;
	}

	bool media_stream::send_stun_request()
	{
		asio::ip::udp::endpoint ep1, ep2;
		int64_t priority1 = -1, priority2 = -1;

		if (!remote_rtp_endpoint(ep1, &priority1))
		{
			return false;
		}

		rtp_transport_->send_stun_request((uint32_t)priority1, ep1);

		if (!is_rtcp_mux_)
		{
			if (!remote_rtcp_endpoint(ep2, &priority2))
			{
				return false;
			}
			rtcp_transport_->send_stun_request((uint32_t)priority2, ep2);
		}

		return true;
	}

	bool media_stream::send_stun_request(const candidate& cand)
	{
		std::error_code ec;
		asio::ip::udp::endpoint ep(asio::ip::address::from_string(cand.address, ec), cand.port);
		if (ec)
			return false;

		if (cand.component == 1)
		{
			return rtp_transport_->send_stun_request(candidate::make_priority(CANDIDATE_TYPE_HOST,0xFFFF,1), ep);
		}
		else if (cand.component == 2)
		{
			return rtcp_transport_->send_stun_request(candidate::make_priority(CANDIDATE_TYPE_HOST, 0xFFFF, 2), ep);
		}
		else
		{
			return false;
		}
	}

	void media_stream::detect_candidates()
	{
		auto cands = remote_candidates();
		for (auto& cand : cands)
		{
			send_stun_request(cand);
		}
	}

	bool media_stream::send_dtls_request()
	{
		asio::ip::udp::endpoint ep1, ep2;
		int64_t priority1 = -1, priority2 = -1;
		if (!remote_rtp_endpoint(ep1, &priority1) || !remote_rtcp_endpoint(ep2, &priority2))
		{
			return false;
		}
		if (ep1 == ep2)
		{
			return rtp_transport_->send_dtls_request(ep1);
		}
		else
		{
			rtp_transport_->send_dtls_request(ep1);
			rtcp_transport_->send_dtls_request(ep2);
			return true;
		}
	}


	bool media_stream::send_rtcp_report()
	{
		std::string compound_pkt;
		compound_pkt.reserve(2048);
		auto ss = senders();
		if (ss.size() > 0)
		{
			for (auto s : ss)
			{
				rtcp_sr sr = { 0 };
				s->prepare_sr(sr);

				auto rs = receivers();
				for (auto r : rs)
				{
					rtcp_report rp;
					r->prepare_rr(rp);
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
			auto rs = receivers();
			for (auto r : rs)
			{
				rtcp_rr* rr = rtcp_rr_create();
				rr->ssrc = r->ssrc();
				rtcp_rr_init(rr);
				rtcp_report rp;
				r->prepare_rr(rp);
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

		
		rtcp_sdes_add_entry(sdes,local_rtp_ssrc_);
		rtcp_sdes_set_item(sdes, local_rtp_ssrc_, RTCP_SDES_CNAME, cname_.c_str());
		size_t size = rtcp_sdes_size(sdes);
		uint8_t* buf_sdes = (uint8_t*)malloc(size);
		if (buf_sdes) {
			rtcp_sdes_serialize(sdes, buf_sdes, size);
			compound_pkt.append((const char*)buf_sdes, size);
			free(buf_sdes);
		}
		rtcp_sdes_free(sdes);

		asio::ip::udp::endpoint ep;
		if (!remote_rtcp_endpoint(ep)) {
			return false;
		}
		return rtcp_transport_->send_rtcp_packet((uint8_t*)compound_pkt.data(), (int)compound_pkt.size(),ep);
	}


	bool media_stream::remote_format(uint8_t pt, sdp_format& fmt)
	{
		std::shared_lock<std::shared_mutex> lk(remote_mutex_);
		auto itr = remote_formats_.find(pt);
		if (itr == remote_formats_.end())
			return false;

		fmt = itr->second;
		return true;
	}

	bool media_stream::local_format(uint8_t pt, sdp_format& fmt)
	{
		std::shared_lock<std::shared_mutex> lk(local_mutex_);
		auto itr = local_formats_.find(pt);
		if (itr == local_formats_.end())
			return false;

		fmt = itr->second;
		return true;
	}

	void media_stream::sctp_connected()
	{
	}

	void media_stream::sctp_disconnected()
	{

	}

	void media_stream::sctp_heartbeat()
	{
	}

	bool media_stream::sctp_open_stream()
	{
		return rtp_transport_->sctp_open_stream(sctp_stream_id_);
	}

	void media_stream::sctp_close_stream()
	{
		rtp_transport_->sctp_close_stream(sctp_stream_id_);
	}

	void media_stream::add_local_candidate(const candidate& cand)
	{
		std::unique_lock<std::shared_mutex> lk(local_mutex_);
		local_candidates_.push_back(cand);
	}

	void media_stream::clear_local_candidates()
	{
		std::unique_lock<std::shared_mutex> lk(local_mutex_);
		local_candidates_.clear();
	}

	std::vector<candidate> media_stream::local_candidates()const
	{
		std::unique_lock<std::shared_mutex> lk(local_mutex_);
		return local_candidates_;
	}


	void media_stream::add_remote_candidate(const candidate& cand)
	{
		std::unique_lock<std::shared_mutex> lk(remote_mutex_);
		remote_candidates_.push_back(cand);
	}

	void media_stream::clear_remote_candidates()
	{
		std::unique_lock<std::shared_mutex> lk(remote_mutex_);
		remote_candidates_.clear();
	}

	std::vector<candidate> media_stream::remote_candidates()const
	{
		std::unique_lock<std::shared_mutex> lk(remote_mutex_);
		return remote_candidates_;
	}


	void media_stream::get_stats(rtp_stats_t& stats)
	{
		memset(&stats, 0, sizeof(stats));
		stats.mt = media_type();

		auto sender = this->get_default_sender();
		if (sender)
		{
			sender->get_stats(stats.sender_stats);
		}

		receiver_ptr receiver = this->get_default_receiver();
		if (receiver)
		{
			receiver->get_stats(stats.receiver_stats);
		}

	}


	sender_ptr media_stream::create_sender(const sdp_format& fmt)
	{
		uint32_t ssrc = 0;
		if (fmt.codec == codec_type_rtx) {
			ssrc = local_rtx_ssrc_;
		}
		else {
			ssrc = local_rtp_ssrc_;
		}


		sender_ptr sender;
		if (fmt.codec == codec_type_h264)
		{
			sender = std::make_shared<sender_video_h264>(fmt.pt, ssrc, media_type(), fmt,log_);
		}
		else if (fmt.codec == codec_type_h265)
		{
			sender = std::make_shared<sender_video_h265>(fmt.pt, ssrc, media_type(), fmt, log_);
		}
		else if (fmt.codec == codec_type_vp8)
		{
			sender = std::make_shared <sender_video_vp8>(fmt.pt, ssrc, media_type(), fmt, log_);
		}
		else if (fmt.codec == codec_type_ps)
		{
			sender = std::make_shared <sender_ps>(fmt.pt, ssrc, media_type(), fmt, log_);
		}
		else if (fmt.codec == codec_type_mpeg4_generic || fmt.codec == codec_type_mp4a_latm)
		{
			sender = std::make_shared<sender_audio_aac>(fmt.pt, ssrc, media_type(), fmt, log_);
		}
		else if (fmt.codec == codec_type_opus || fmt.codec == codec_type_pcma || fmt.codec == codec_type_pcmu || fmt.codec == codec_type_g722)
		{
			sender = std::make_shared<sender_audio>(fmt.pt, ssrc, media_type(), fmt, log_);
		}
		else if (fmt.codec == codec_type_rtx)
		{
			sender = std::make_shared<sender_rtx>(fmt.pt, ssrc, media_type(), fmt, log_);
		}
		else
		{
			return nullptr;
		}

		auto self = shared_from_this();
		sender->bind_send_rtp_packet_handler([self](packet_ptr pkt){
			self->send_rtp_packet(pkt);
		});

		auto r=senders_.insert(std::make_pair(sender->pt(), sender));
		if (!r.second)
			return nullptr;

		return sender;
	}

	std::vector<sender_ptr> media_stream::senders()const
	{
		std::shared_lock<std::shared_mutex>lk(local_mutex_);

		std::vector<sender_ptr> vec;
		vec.reserve(senders_.size());
		for (auto sender : senders_)
		{
			vec.push_back(sender.second);
		}
		return vec;
	}

	sender_ptr media_stream::get_default_sender()
	{
		{
			std::shared_lock<std::shared_mutex>lk(local_mutex_);
			for (auto itr = senders_.begin(); itr != senders_.end(); itr++)
			{
				if (itr->second->format().codec != codec_type_rtx) {
					return itr->second;
				}
			}
		}

		std::unique_lock<std::shared_mutex>lk(local_mutex_);

		for (const auto& fmt : local_formats_)
		{
			if (fmt.second.codec != codec_type_rtx)
			{
				return create_sender(fmt.second);
			}
		}

		return nullptr;
	}

	sender_ptr media_stream::get_sender(uint8_t pt)
	{
		{
			std::shared_lock<std::shared_mutex>lk(local_mutex_);
			auto itr = senders_.find(pt);
			if (itr != senders_.end())
			{
				return itr->second;
			}
		}

		std::unique_lock<std::shared_mutex>lk(local_mutex_);
		auto itr_fmt = local_formats_.find(pt);
		if (itr_fmt == local_formats_.end())
		{
			return nullptr;
		}

		return create_sender(itr_fmt->second);
	}

	receiver_ptr media_stream::create_receiver(const sdp_format& fmt)
	{
		uint32_t ssrc = 0;
		if (fmt.codec == codec_type_rtx) {
			ssrc = remote_rtx_ssrc_;
		}
		else {
			ssrc = remote_rtp_ssrc_;
		}

		receiver_ptr receiver;
		if (fmt.codec == codec_type_h264)
		{
			receiver = std::make_shared<receiver_video_h264>(ioc_,ssrc, media_type_video, fmt,log_);
		}
		else if (fmt.codec == codec_type_h265)
		{
			receiver = std::make_shared<receiver_video_h265>(ioc_, ssrc, media_type_video, fmt, log_);
		}
		else if (fmt.codec == codec_type_vp8)
		{
			receiver = std::make_shared <receiver_video_vp8>(ioc_, ssrc, media_type_video, fmt, log_);
		}
		//else if (fmt.codec == codec_type_vp9)
		//{
		//	receiver = std::make_shared <receiver_video_vp9>(ioc_,ssrc, media_type_video, fmt,log_);
		//}
		else if (fmt.codec == codec_type_ps)
		{
			receiver = std::make_shared <receiver_ps>(ioc_, ssrc, media_type_video, fmt, log_);
		}
		else if (fmt.codec == codec_type_mpeg4_generic || fmt.codec == codec_type_mp4a_latm)
		{
			receiver = std::make_shared <receiver_audio_aac>(ioc_, ssrc, media_type_audio, fmt, log_);
		}
		else if (fmt.codec == codec_type_opus || fmt.codec == codec_type_pcma || fmt.codec == codec_type_pcmu || fmt.codec == codec_type_g722)
		{
			receiver = std::make_shared <receiver_audio>(ioc_, ssrc, media_type_audio, fmt, log_);
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
				receiver = std::make_shared <receiver_rtx>(ioc_, ssrc, apt_receiver->media_type(), fmt, apt_receiver,log_);
			}
			else {
				return nullptr;
			}
		}
		else
		{
			return nullptr;
		}

		auto self = shared_from_this();
		receiver->on_frame.set([self](uint32_t ssrc, const sdp_format& fmt, const av_frame_t& frame) {
			self->on_frame(self->mid(), fmt, frame);
		});

		receiver->on_keyframe.set([self](uint32_t ssrc, const sdp_format& fmt) {
			self->send_rtcp_keyframe(ssrc,fmt.pt);
		});

		receiver->on_nack.set([self](uint32_t ssrc, const sdp_format& fmt, uint16_t pid, uint16_t bld) {
			self->send_rtcp_nack(self->local_rtp_ssrc_, ssrc, pid, bld);
		});
		if (!receiver->start()) {
			return nullptr;
		}

		auto r=receivers_.insert(std::make_pair(fmt.pt, receiver));
		if (!r.second)
			return nullptr;


		return receiver;
	}

	std::vector<receiver_ptr> media_stream::receivers()const
	{
		std::shared_lock<std::shared_mutex>lk(remote_mutex_);

		std::vector<receiver_ptr> vec;
		vec.reserve(receivers_.size());
		for (auto receiver : receivers_)
		{
			vec.push_back(receiver.second);
		}
		return vec;
	}

	receiver_ptr media_stream::get_receiver_by_ssrc(uint32_t ssrc)
	{
		std::shared_lock<std::shared_mutex>lk(remote_mutex_);
		for (auto itr = receivers_.begin(); itr != receivers_.end(); itr++)
		{
			if (itr->second->ssrc() == ssrc)
			{
				return itr->second;
			}
		}
		return nullptr;
	}

	receiver_ptr media_stream::get_receiver(uint8_t pt)
	{
		{
			std::shared_lock<std::shared_mutex>lk(remote_mutex_);
			auto itr = receivers_.find(pt);
			if (itr != receivers_.end())
			{
				return itr->second;
			}
		}

		{
			std::unique_lock<std::shared_mutex>lk(remote_mutex_);
			auto itr = receivers_.find(pt);
			if (itr != receivers_.end())
			{
				return itr->second;
			}


			sdp_format fmt;
			auto itr2 = remote_formats_.find(pt);
			if (itr2 == remote_formats_.end())
			{
				return nullptr;
			}

			receiver_ptr receiver = create_receiver(itr2->second);
			if (!receiver)
			{
				return nullptr;
			}

			return receiver;
		}
	}

	receiver_ptr media_stream::get_default_receiver()
	{
		{
			std::shared_lock<std::shared_mutex>lk(remote_mutex_);
			for (auto itr = receivers_.begin(); itr != receivers_.end(); itr++)
			{
				if (itr->second->format().codec != codec_type_rtx) {
					return itr->second;
				}
			}
		}

		std::unique_lock<std::shared_mutex>lk(remote_mutex_);

		for (const auto& fmt : remote_formats_)
		{
			if (fmt.second.codec != codec_type_rtx)
			{
				return create_receiver(fmt.second);
			}
		}

		return nullptr;
	}

	bool media_stream::local_rtx_format(uint8_t pt, uint8_t& rtx_pt, uint32_t& rtx_ssrc)const
	{
		std::shared_lock<std::shared_mutex>lk(local_mutex_);
		for (auto itr = local_formats_.begin(); itr != local_formats_.end(); itr++)
		{
			if (itr->second.codec == codec_type_rtx)
			{
				if (itr->second.fmtp.apt() == pt)
				{
					rtx_pt = itr->first;

					if (local_rtx_ssrc_>0) {
						rtx_ssrc = local_rtx_ssrc_;
					}
					else
					{
						rtx_ssrc = local_rtp_ssrc_;
					}
					return true;
				}
			}
		}
		return false;
	}


	bool media_stream::receive_rtp_packet(const packet_ptr& packet, const asio::ip::udp::endpoint& endpoint)
	{
		auto receiver = get_receiver(packet->header()->pt);
		if (!receiver)
		{
			return false;
		}

		if (receiver->format().codec != codec_type_rtx)
		{
			if (use_rtp_endpoint_) {
				set_remote_rtp_endpoint(endpoint, candidate::make_priority(CANDIDATE_TYPE_HOST, 0xFFFF, 1));
				use_rtp_endpoint_ = false;
			}
			// Fix ssrc by rtp packet
			if (remote_rtp_ssrc() == 0)
			{
				set_remote_rtp_ssrc(packet->header()->ssrc);
			}
		}

		return receiver->insert_packet(packet);
	}

	bool media_stream::receive_rtcp_app(const rtcp_app* app, const asio::ip::udp::endpoint& endpoint)
	{
		return true;
	}

	bool media_stream::receive_rtcp_bye(uint32_t ssrc, const char* reason, const asio::ip::udp::endpoint& endpoint)
	{
		return true;
	}

	bool media_stream::receive_rtcp_sr(const rtcp_sr* sr, const asio::ip::udp::endpoint& endpoint)
	{
		if (use_rtcp_endpoint_) {
			set_remote_rtcp_endpoint(endpoint, candidate::make_priority(CANDIDATE_TYPE_HOST, 0xFFFF, 2));
			use_rtcp_endpoint_ = false;
		}


		auto ss = senders();
		for (auto s : ss)
		{
			if (s->format().codec == codec_type_rtx)
			{
				continue;
			}
			const rtcp_report* report = rtcp_sr_find_report(sr, s->ssrc());
			if (report)
			{
				s->update_remote_report(*report);

				if (log_)
				{
					log_->debug("Received RR ssrc={} last_seq={} lost={} fraction={} jitter={},lsr={},dlst={}",
						report->ssrc, (uint16_t)report->last_seq, report->lost, report->fraction, report->jitter, report->lsr, report->dlsr)->flush();
				}
			}
		}

		auto rs = receivers();
		for (auto r : rs)
		{
			if (r->ssrc() == sr->ssrc)
			{
				r->update_remote_sr(*sr);
			}
		}

		if (log_)
		{
			log_->debug("Received SR ssrc={} pkts={} bytes={}",
				sr->ssrc, sr->pkt_count, sr->byte_count)->flush();
		}

		return true;
	}

	bool media_stream::receive_rtcp_rr(const rtcp_report* report, const asio::ip::udp::endpoint& endpoint)
	{
		if (use_rtcp_endpoint_) {
			set_remote_rtcp_endpoint(endpoint, candidate::make_priority(CANDIDATE_TYPE_HOST, 0xFFFF, 2));
			use_rtcp_endpoint_ = false;
		}

		auto ss = senders();
		for (auto s : ss)
		{
			if (s->format().codec == codec_type_rtx)
			{
				continue;
			}

			s->update_remote_report(*report);
		}

		if (log_)
		{
			log_->debug("Received RR ssrc={} last_seq={} lost={} fraction={} jitter={},lsr={},dlst={}", 
				report->ssrc, (uint16_t)report->last_seq,report->lost,report->fraction, report->jitter, report->lsr, report->dlsr)->flush();
		}

		return true;
	}

	bool media_stream::receive_rtcp_sdes(const rtcp_sdes_entry* sdes, const asio::ip::udp::endpoint& endpoint)
	{
		if (use_rtcp_endpoint_) {
			set_remote_rtcp_endpoint(endpoint, candidate::make_priority(CANDIDATE_TYPE_HOST, 0xFFFF, 2));
			use_rtcp_endpoint_ = false;
		}
		return true;
	}

	bool media_stream::receive_rtcp_feedback(const rtcp_fb* fb, const asio::ip::udp::endpoint& endpoint)
	{
		if (fb->header.common.pt == rtcp_packet_type::RTCP_RTPFB)
		{
			if (fb->header.app.subtype == RTCP_RTPFB_FMT_NACK)
			{
				uint16_t pid = 0, bld = 0;
				rtcp_rtpfb_nack_get(fb, &pid, &bld);
				on_rtcp_fb_nack(fb->ssrc_media, pid, bld);
			}
		}
		else if (fb->header.common.pt == rtcp_packet_type::RTCP_PSFB)
		{
			if (fb->header.app.subtype == RTCP_PSFB_FMT_PLI)
			{
				on_rtcp_fb_pli(fb->ssrc_media);
			}
			else if (fb->header.app.subtype == RTCP_PSFB_FMT_FIR)
			{
				int c = rtcp_psfb_fir_item_count(fb);
				for (int i = 0; i < c; i++)
				{
					rtcp_psfb_fir_item item;
					rtcp_psfb_fir_get_item(fb, i, &item);
					on_rtcp_fb_fir(item.ssrc, item.seq_nr);
				}
			}
		}

		return true;
	}

	bool media_stream::receive_stun_completed(const asio::ip::udp::endpoint& endpoint)
	{
		set_remote_rtp_endpoint(endpoint, candidate::make_priority(CANDIDATE_TYPE_HOST, 0xFFFF, 1));
		if (is_rtcp_mux_)
		{
			set_remote_rtcp_endpoint(endpoint, candidate::make_priority(CANDIDATE_TYPE_HOST, 0xFFFF, 2));
		}
		return true;
	}

	bool media_stream::receive_dtls_completed(dtls_role_t dtls_role)
	{
		return true;
	}

	void media_stream::on_rtcp_fb_nack(uint32_t ssrc, uint16_t pid, uint16_t bld)
	{
		if (log_)
		{
			log_->trace("Recevied rtcp nack ssrc={} seq={}", ssrc, pid)->flush();
		}

		auto sender = this->get_default_sender();
		if (sender)
		{
			uint8_t rtx_pt = 0;
			uint32_t rtx_ssrc = 0;
			sender_ptr rtx_sender;
			if (this->local_rtx_format(sender->pt(), rtx_pt, rtx_ssrc)) {
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
							if (log_)
							{
								log_->trace("Resend rtp packet. pt={} ssrc={} seq={}",pkt->header()->pt,pkt->header()->ssrc,pkt->header()->seq)->flush();
							}
						}
					}
				}
			}

			sender->increase_nack();
		}
	}

	void media_stream::on_rtcp_fb_pli(uint32_t ssrc)
	{
		if (log_)
		{
			log_->trace("Required keyframe by pli ssrc={}",ssrc)->flush();
		}

		auto sender = this->get_default_sender();
		if (sender)
		{
			sender->increase_pli();
		}

		if (on_receive_require_keyframe) {
			on_receive_require_keyframe(mid_);
		}
	}

	void media_stream::on_rtcp_fb_fir(uint32_t ssrc, uint8_t nr)
	{
		if (log_)
		{
			log_->trace("Required keyframe by fir ssrc={}",ssrc)->flush();
		}
		auto sender = this->get_default_sender();
		if (sender)
		{
			sender->increase_fir();
		}

		if (on_receive_require_keyframe)
		{
			on_receive_require_keyframe(mid_);
		}
	}


}

