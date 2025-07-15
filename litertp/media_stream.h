/**
 * @file media_stream.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#pragma once

#include "transports/transport_udp.h"
#include "transports/transport_custom.h"

#include "senders/sender.h"
#include "receivers/receiver.h"

#include "proto/rtcp_app.h"
#include "proto/rtcp_bye.h"
#include "proto/rtcp_fb.h"
#include "proto/rtcp_report.h"
#include "proto/rtcp_rr.h"
#include "proto/rtcp_sr.h"
#include "proto/rtcp_sdes.h"

#include "sdp/sdp.h"
#include "sctp/sctp.h"

#include <sys2/signal.h>
#include <sys2/update_time.h>

namespace litertp
{
	class media_stream
	{
	public:
		media_stream(media_type_t media_type, uint32_t ssrc,const std::string& mid,const std::string& cname,const std::string& ice_options, const std::string& ice_ufrag, const std::string& ice_pwd,
			const std::string& local_address,transport_ptr transport_rtp, transport_ptr transport_rtcp,bool is_tcp=false);
		
		//application
		media_stream(const std::string& mid, const std::string& protos, const std::string& ice_options, const std::string& ice_ufrag, const std::string& ice_pwd,transport_ptr transport);

		~media_stream();

		const std::string& mid()const;

		void close();


		bool add_local_video_track(codec_type_t codec, uint8_t pt, int frequency = 90000,bool rtcp_fb=true);
		bool add_local_audio_track(codec_type_t codec, uint8_t pt, int frequency, int channels = 1, bool rtcp_fb = false);
		bool add_local_rtx_track(uint8_t apt, uint8_t rtx_pt, int frequency, bool rtcp_fb=true);

		bool add_remote_video_track(codec_type_t codec, uint8_t pt, int frequency = 90000, bool rtcp_fb = true);
		bool add_remote_audio_track(codec_type_t codec, uint8_t pt, int frequency, int channels = 1, bool rtcp_fb = false);
		bool add_remote_rtx_track(uint8_t apt, uint8_t rtx_pt, int frequency, bool rtcp_fb = true);
		
		void set_remote_trans_mode(rtp_trans_mode_t trans_mode);
		void set_local_trans_mode(rtp_trans_mode_t trans_mode);
		void set_remote_ssrc(uint32_t ssrc);
		void set_remote_setup(sdp_setup_t setup);
		void set_local_setup(sdp_setup_t setup);

		bool add_remote_rtpmap_attribute(uint16_t pt,const char* key, const char* val);
		bool clear_remote_rtpmap_attributes(uint16_t pt);

		bool add_local_rtpmap_attribute(uint16_t pt,const char* key, const char* val);
		bool clear_local_rtpmap_attributes(uint16_t pt);

		void add_remote_attribute(const char* key, const char* val);
		void clear_remote_attributes();

		void add_local_attribute(const char* key, const char* val);
		void clear_local_attributes();

		bool set_local_fmtp(int pt,const fmtp& fmtp);
		bool get_local_fmtp(int pt, fmtp& fmtp);
		bool set_remote_fmtp(int pt, const fmtp& fmtp);
		bool get_remote_fmtp(int pt, fmtp& fmtp);


		void add_local_candidate(uint32_t foundation, uint32_t component, const char* address, int port, uint32_t priority);
		void add_remote_candidate(uint32_t foundation, uint32_t component, const char* address, int port, uint32_t priority);
		void clear_local_candidates();
		void clear_remote_candidates();

		void enable_dtls();
		void disable_dtls();

		bool set_remote_sdp(const sdp_media& sdp, sdp_type_t sdp_type);
		bool has_remote_sdp()const;
		void set_local_sdp(const sdp_media& sdp);

		sdp_type_t sdp_type() const { return sdp_type_; }
		void set_sdp_type(sdp_type_t sdp_type);

		void set_remote_mid(const char* mid);

		void set_remote_rtp_endpoint(const sockaddr* addr, int addr_size,uint32_t priority);
		void set_remote_rtcp_endpoint(const sockaddr* addr, int addr_size, uint32_t priority);
		bool get_remote_rtp_endpoint(sockaddr_storage* addr);
		bool get_remote_rtcp_endpoint(sockaddr_storage* addr);
		void clear_remote_rtp_endpoint();
		void clear_remote_rtcp_endpoint();

		srtp_role_t srtp_role();
		stun_ice_role_t stun_ice_role();
		bool check_setup();

		bool negotiate();
		bool exchange();

		void run_rtcp_stats();
		void run_stun_request();

		void send_rtcp_nack(uint32_t ssrc_sender, uint32_t ssrc_media, uint16_t pid, uint16_t blp);

		
		void send_rtcp_keyframe(uint32_t ssrc_media);
		void send_rtcp_keyframe(uint32_t ssrc_media,uint16_t pt);
		void send_rtcp_keyframe_pli(uint32_t ssrc_sender, uint32_t ssrc_media);
		void send_rtcp_keyframe_fir(uint32_t ssrc_sender, uint32_t ssrc_media);
		void send_rtcp_bye(const char* reason="closed");

		bool send_rtp_packet(packet_ptr packet);
		bool send_rtcp_packet(const uint8_t* rtcp_data,int size);
		bool send_app_packet(const uint8_t* data, int size);
		bool send_data_channel(const uint8_t* data, int size);

		media_type_t media_type();

		bool send_frame(const uint8_t* frame, uint32_t size, uint32_t duration);

		sdp_media get_local_sdp();
		sdp_media get_remote_sdp();
		
		bool get_local_format(int pt, sdp_format& fmt);
		bool get_remote_format(int pt, sdp_format& fmt);

		uint32_t get_local_ssrc(int idx=0);
		uint32_t get_remote_ssrc(int idx=0);

		void get_stats(rtp_stats_t& stats);

		void set_timestamp(uint32_t ms);
		uint32_t timestamp();

		void set_msid(const std::string& msid);

		void use_rtp_address(bool use) { 
			use_rtp_addr_ = use;
			use_rtcp_addr_ = use;
		}

		void set_remote_ice_lite(bool ice_lite) {
			remote_ice_lite_ = ice_lite;
		}
		bool is_remote_ice_lite() const {
			return remote_ice_lite_;
		}

		bool is_timeout()const { return update_.is_timeout(20000); }
	private:
		static void s_transport_dtls_handshake(void* ctx, srtp_role_t role);
		static void s_transport_rtp_packet(void* ctx, std::shared_ptr<sys::socket> skt, packet_ptr packet, const sockaddr* addr, int addr_size);
		static void s_transport_rtcp_packet(void* ctx, std::shared_ptr<sys::socket> skt, uint16_t pt, const uint8_t* buffer, size_t size, const sockaddr* addr, int addr_size);
		static void s_transport_stun_message(void* ctx, std::shared_ptr<sys::socket> skt, const stun_message& msg, const sockaddr* addr, int addr_size);
		static void s_transport_raw_packet(void* ctx, std::shared_ptr<sys::socket> skt, const uint8_t* raw_data, size_t size, const sockaddr* addr, int addr_size);
		
		static void s_rtp_frame_event(void* ctx, uint32_t ssrc, const sdp_format& fmt, const av_frame_t& frame);
		void on_rtp_frame_event(uint32_t ssrc, const sdp_format& fmt, const av_frame_t& frame);

		static void s_rtp_nack_event(void* ctx, uint32_t ssrc, const sdp_format& fmt, uint16_t pid, uint16_t bld);

		void on_rtp_nack_event(uint32_t ssrc, const sdp_format& fmt, uint16_t pid, uint16_t bld);

		static void s_rtp_keyframe_event(void* ctx, uint32_t ssrc, const sdp_format& fmt);
		void on_rtp_keyframe_event(uint32_t ssrc, const sdp_format& fmt);


		static void s_send_rtp_packet_event(void* ctx,packet_ptr packet);

		

		sender_ptr get_default_sender();
		sender_ptr get_sender(int pt);
		sender_ptr get_sender_by_ssrc(uint32_t ssrc);
		std::vector<sender_ptr> get_senders();
		sender_ptr create_sender(const sdp_format& fmt);

		receiver_ptr get_receiver(int pt);
		receiver_ptr get_receiver_by_ssrc(uint32_t ssrc);
		std::vector<receiver_ptr> get_receivers();
		receiver_ptr create_receiver(uint32_t ssrc,const sdp_format& fmt);

		bool has_remote_ssrc(uint32_t ssrc);

		bool get_rtx_local(uint8_t pt,uint8_t& rtx_pt, uint32_t& rtx_ssrc)const;
	private:
		static void s_sctp_connected(void* ctx);
		static void s_sctp_closed(void* ctx);
		static void s_sctp_heartbeat(void* ctx);
		static void s_sctp_stream_open(void* ctx, uint16_t stream_id);
		static void s_sctp_stream_closed(void* ctx, uint16_t stream_id);
		static void s_sctp_stream_message(void* ctx, uint16_t stream_id, const std::vector<uint8_t>& message);
	private:

		void on_rtcp_app(const rtcp_app* app);
		void on_rtcp_bye(const rtcp_bye* bye);
		void on_rtcp_sr(const rtcp_sr* sr);
		void on_rtcp_rr(const rtcp_rr* rr);
		void on_rtcp_sdes(const rtcp_sdes* sdes);
		void on_rtcp_nack(uint32_t ssrc,uint16_t pid, uint16_t bld);
		void on_rtcp_pli(uint32_t ssrc);
		void on_rtcp_fir(uint32_t ssrc, uint8_t nr);

	public:

		sys::callback<litertp_on_frame_t> on_frame_received;
		sys::callback<litertp_on_received_require_keyframe_t> on_received_require_keyframe;
		sys::callback<litertp_on_require_keyframe_t> on_require_keyframe;

		sys::callback<litertp_on_data_channel_open_t> on_data_channel_open;
		sys::callback<litertp_on_data_channel_close_t> on_data_channel_close;
		sys::callback<litertp_on_data_channel_message_t> on_data_channel_message;


		transport_ptr transport_rtp_;
		transport_ptr transport_rtcp_;
	private:
		std::string cname_;

		sys::update_time update_;

		mutable std::shared_mutex local_sdp_media_mutex_;
		sdp_media local_sdp_media_;

		mutable std::shared_mutex remote_sdp_media_mutex_;
		bool has_remote_sdp_media_ = false;
		sdp_media remote_sdp_media_;
		


		mutable std::shared_mutex remote_rtp_endpoint_mutex_;
		sockaddr_storage remote_rtp_endpoint_ = { 0 };
		int64_t remote_rtp_endpoint_priority = -1;

		mutable std::shared_mutex remote_rtcp_endpoint_mutex_;
		sockaddr_storage remote_rtcp_endpoint_ = { 0 };
		int64_t remote_rtcp_endpoint_priority = -1;
		

		mutable std::shared_mutex senders_mutex_;
		std::map<int, sender_ptr> senders_;

		mutable std::shared_mutex receivers_mutex_;
		std::map<int, receiver_ptr> receivers_;


		sdp_type_t sdp_type_= sdp_type_offer;
		bool is_dtls_ = false;

		bool use_rtp_addr_ = false;
		bool use_rtcp_addr_ = false;

		bool remote_ice_lite_ = false;

		uint16_t sctp_stream_id_ = 0;
	};


	typedef std::shared_ptr<media_stream> media_stream_ptr;
}
