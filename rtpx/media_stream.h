/**
 * @file media_stream.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#pragma once
#include <memory>
#include <vector>
#include <shared_mutex>
#include <asio.hpp>

#include "transports/transport.h"
#include "sdp/sdp_media.h"

#include "senders/sender.h"
#include "receivers/receiver.h"

namespace rtpx {


	class media_stream;
	typedef std::shared_ptr<media_stream> media_stream_ptr;

	class media_stream:public std::enable_shared_from_this<media_stream>
	{
	public:

		typedef std::function<void(const std::string&, const sdp_format&, const av_frame_t&)> frame_handler;
		typedef std::function<void(const std::string&)> send_require_keyframe_handler;
		typedef std::function<void(const std::string&)> receive_require_keyframe_handler;

		media_stream(asio::io_context& ioc,media_type_t mt, rtp_trans_mode_t trans_mode,transport_ptr rtp,transport_ptr rtcp,spdlogger_ptr log);
		~media_stream();

		void close();

		sdp_type_t sdp_role()const { return sdp_role_; }
		sdp_setup_t local_setup()const { return local_setup_; }
		sdp_setup_t remote_setup()const { return remote_setup_; }

		bool add_local_video_track(codec_type_t codec, uint8_t pt, int frequency = 90000, bool rtcp_fb = true);
		bool add_local_audio_track(codec_type_t codec, uint8_t pt, int frequency, int channels = 1, bool rtcp_fb = true);
		bool add_local_rtx_track(uint8_t pt, uint8_t apt, int frequency, bool rtcp_fb = true);

		bool add_remote_video_track(codec_type_t codec, uint8_t pt, int frequency = 90000, bool rtcp_fb = true);
		bool add_remote_audio_track(codec_type_t codec, uint8_t pt, int frequency, int channels = 1, bool rtcp_fb = true);
		bool add_remote_rtx_track(uint8_t pt, uint8_t apt, int frequency, bool rtcp_fb = true);

		void add_local_data_channel_track(uint16_t sctp_port);

		bool set_local_rtpmap_fmtp(uint8_t pt, const fmtp& fmtp);
		bool set_local_rtpmap_attribute(uint8_t pt, const std::string& key, const std::string& val);
		bool clear_local_rtpmap_attributes(uint8_t pt);

		bool set_remote_rtpmap_attribute(uint8_t pt, const std::string& key, const std::string& val);
		bool clear_remote_rtpmap_attributes(uint8_t pt);

		bool create_offer(sdp_media& sdp);
		bool create_answer(sdp_media& sdp);
		bool set_remote_sdp(const sdp_media& sdp, sdp_type_t type);

		bool send_frame(uint8_t pt,const uint8_t* frame, uint32_t size, uint32_t duration);
		bool send_rtp_packet(packet_ptr packet);
		bool send_rtcp_packet(const uint8_t* rtcp_data, int size);
		bool send_data_channel(const uint8_t* data, int size);

		void send_rtcp_keyframe(uint32_t ssrc_media);
		void send_rtcp_keyframe(uint32_t ssrc_media, uint8_t pt);
		void send_rtcp_keyframe_pli(uint32_t ssrc_sender, uint32_t ssrc_media);
		void send_rtcp_keyframe_fir(uint32_t ssrc_sender, uint32_t ssrc_media);
		void send_rtcp_bye(const char* reason = "closed");
		void send_rtcp_nack(uint32_t ssrc_sender, uint32_t ssrc_media, uint16_t pid, uint16_t blp);

		void use_rtp_endpoint(bool v) { use_rtp_endpoint_ = v; }
		void use_rtcp_endpoint(bool v) { use_rtp_endpoint_ = v; }
		void set_remote_rtp_endpoint(const asio::ip::udp::endpoint& endpoint, int64_t priority);
		void set_remote_rtcp_endpoint(const asio::ip::udp::endpoint& endpoint, int64_t priority);
		bool remote_rtp_endpoint(asio::ip::udp::endpoint& endpoint, int64_t* priority=nullptr)const;
		bool remote_rtcp_endpoint(asio::ip::udp::endpoint& endpoint, int64_t* priority=nullptr)const;


		void set_security(bool v) { security_ = v; }
		bool is_security()const { return security_; }

		const uint16_t local_rtp_port()const { return rtp_transport_->local_port(); }
		const uint16_t local_rtcp_port()const { return rtcp_transport_->local_port(); }
		transport_ptr rtp_transport() { return rtcp_transport_; }
		transport_ptr rtcp_transport() { return rtcp_transport_; }

		uint32_t local_rtp_ssrc()const { return local_rtp_ssrc_; }
		uint32_t local_rtx_ssrc()const { return local_rtx_ssrc_; }
		uint32_t remote_rtp_ssrc()const;
		uint32_t remote_rtx_ssrc()const;
		void set_remote_rtp_ssrc(uint32_t ssrc);
		void set_remote_rtx_ssrc(uint32_t ssrc);

		void set_local_ice_options(const std::string& ufrag, const std::string& pwd, const std::string& options);
		void set_remote_ice_options(const std::string& ufrag, const std::string& pwd, const std::string& options);

		media_type_t media_type()const { return media_type_; }
		void set_cname(const std::string& v) { cname_ = v; }
		const std::string& cname()const { return cname_; }

		void set_mid(const std::string& v) { mid_ = v; }
		const std::string& mid()const { return mid_; }

		void set_msid_sid(const std::string& v) { msid_sid_ = v; }
		const std::string& msid_sid()const { return msid_sid_; }

		void set_msid_tid(const std::string& v) { msid_tid_ = v; }
		const std::string& msid_tid()const { return msid_tid_; }

		void set_rtcp_mux(bool v) { is_rtcp_mux_ = v; }
		bool is_rtcp_mux()const { return is_rtcp_mux_; }

		uint16_t sctp_stream_id()const { return sctp_stream_id_; }
		void set_sctp_stream_id(uint16_t sid) { if (!has_sctp_stream_id_) { sctp_stream_id_ = sid; has_sctp_stream_id_ = true; } }

		rtp_trans_mode_t trans_mode()const {
			std::shared_lock<std::shared_mutex> lk(local_mutex_);
			return trans_mode_; 
		}
		void set_trans_mode(rtp_trans_mode_t v) { 
			std::unique_lock<std::shared_mutex> lk(local_mutex_);
			trans_mode_ = v; 
		}

		std::vector<sdp_format> local_formats()const;
		std::vector<sdp_format> remote_formats()const;

		bool receive_rtp_packet(const packet_ptr& packet,const asio::ip::udp::endpoint& endpoint);
		bool receive_rtcp_app(const rtcp_app* app, const asio::ip::udp::endpoint& endpoint);
		bool receive_rtcp_bye(uint32_t ssrc,const char* reason, const asio::ip::udp::endpoint& endpoint);
		bool receive_rtcp_sr(const rtcp_sr* sr, const asio::ip::udp::endpoint& endpoint);
		bool receive_rtcp_rr(const rtcp_report* report, const asio::ip::udp::endpoint& endpoint);
		bool receive_rtcp_sdes(const rtcp_sdes_entry* sdes, const asio::ip::udp::endpoint& endpoint);
		bool receive_rtcp_feedback(const rtcp_fb* fb, const asio::ip::udp::endpoint& endpoint);

		bool receive_stun_completed(const asio::ip::udp::endpoint& endpoint);
		bool receive_dtls_completed(dtls_role_t dtls_role);


		bool send_stun_request();
		bool send_stun_request(const candidate& cand);
		void detect_candidates();

		bool send_dtls_request();
		bool send_rtcp_report();

		bool remote_format(uint8_t pt, sdp_format& fmt);
		bool local_format(uint8_t pt, sdp_format& fmt);

		void sctp_connected();
		void sctp_disconnected();
		void sctp_heartbeat();
		bool sctp_open_stream();
		void sctp_close_stream();

		void add_local_candidate(const candidate& cand);
		void clear_local_candidates();
		std::vector<candidate> local_candidates()const;

		void add_remote_candidate(const candidate& cand);
		void clear_remote_candidates();
		std::vector<candidate> remote_candidates()const;

		void get_stats(rtp_stats_t& stats);
	private:
		sender_ptr create_sender(const sdp_format& fmt);
		std::vector<sender_ptr> senders()const;
		sender_ptr get_default_sender();
		sender_ptr get_sender(uint8_t pt);

		receiver_ptr create_receiver(const sdp_format& fmt);
		std::vector<receiver_ptr> receivers()const;
		receiver_ptr get_receiver_by_ssrc(uint32_t ssrc);
		receiver_ptr get_receiver(uint8_t pt);
		receiver_ptr get_default_receiver();

		bool local_rtx_format(uint8_t pt, uint8_t& rtx_pt, uint32_t& rtx_ssrc)const;
		bool negotiate(const sdp_media& remote_sdp);
		bool to_local_sdp(sdp_media& local_sdp);
	private:
		void on_rtcp_fb_nack(uint32_t ssrc, uint16_t pid, uint16_t bld);
		void on_rtcp_fb_pli(uint32_t ssrc);
		void on_rtcp_fb_fir(uint32_t ssrc, uint8_t nr);

	private:
		asio::io_context& ioc_;
		spdlogger_ptr log_;
		

		sdp_type_t sdp_role_ = sdp_type_unknown;
		rtp_trans_mode_t trans_mode_ = rtp_trans_mode_t::rtp_trans_mode_inactive;
		sdp_setup_t local_setup_ = sdp_setup_t::sdp_setup_none;
		sdp_setup_t remote_setup_ = sdp_setup_t::sdp_setup_none;

		bool security_ = false;

		bool use_rtp_endpoint_ = false;
		bool use_rtcp_endpoint_ = false;

		mutable std::shared_mutex remote_mutex_;
		mutable std::shared_mutex local_mutex_;
		media_type_t media_type_ = media_type_unknown;
		bool is_rtcp_mux_ = false;

		std::string cname_;
		std::string mid_;
		std::string msid_sid_;
		std::string msid_tid_;

		std::string local_ice_ufrag_;
		std::string local_ice_pwd_;
		std::string local_ice_options_;
		

		std::string remote_ice_ufrag_;
		std::string remote_ice_pwd_;
		std::string remote_ice_options_;

		transport_ptr rtp_transport_;
		transport_ptr rtcp_transport_;

		uint32_t local_rtp_ssrc_ = 0;
		uint32_t local_rtx_ssrc_ = 0;
		uint32_t remote_rtp_ssrc_ = 0;
		uint32_t remote_rtx_ssrc_ = 0;


		int64_t remote_rtp_endpoint_priority_ = -1;
		asio::ip::udp::endpoint remote_rtp_endpoint_;
		int64_t remote_rtcp_endpoint_priority_ = -1;
		asio::ip::udp::endpoint remote_rtcp_endpoint_;

		std::vector<candidate> local_candidates_;
		std::vector<candidate> remote_candidates_;

		std::map<uint8_t, sdp_format> local_formats_;
		std::map<uint8_t, sdp_format> remote_formats_;

		std::map<uint8_t, sender_ptr> senders_;
		std::map<uint8_t, receiver_ptr> receivers_;

		std::string app_proto_;
		uint16_t sctp_port_ = 0;
		bool has_sctp_stream_id_ = false;
		uint16_t sctp_stream_id_ = 0;

	public:
		frame_handler on_frame;
		send_require_keyframe_handler on_send_require_keyframe;
		receive_require_keyframe_handler on_receive_require_keyframe;
	};
	
}

