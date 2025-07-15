/**
 * @file rtp_session.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#pragma once
#include "media_stream.h"
#include <sys2/signal.h>
#include <litertp/port_range.h>

namespace litertp
{
	class rtp_session
	{
	public:
		/// <summary>
		/// 
		/// </summary>
		/// <param name="webrtc">Whether webrtc rtp session</param>
		/// <param name="stun">Use stun detection</param>
		rtp_session(bool webrtc = false, bool stun = false,bool ice_lite=false);
		~rtp_session();

		bool start();
		void stop();
		void clear();

		media_stream_ptr create_media_stream(const std::string& mid, media_type_t mt, uint32_t ssrc, const char* local_address, int local_rtp_port, int local_rtcp_port,bool reuse_port);
		media_stream_ptr create_media_stream(const std::string& mid, media_type_t mt, uint32_t ssrc, const char* local_address, port_range_ptr ports,bool rtcp_mux);
		media_stream_ptr create_media_stream(const std::string& mid, media_type_t mt, uint32_t ssrc,int port,litertp_on_send_packet on_send_packet,void* ctx, bool reuse_port);
		media_stream_ptr create_application_stream(const std::string& mid, const std::string& protos, int port);
		media_stream_ptr create_application_stream(const std::string& mid, const std::string& protos, port_range_ptr ports);

		media_stream_ptr get_media_stream(media_type_t mt);
		media_stream_ptr get_media_stream(const std::string& mid);
		std::vector<media_stream_ptr> get_media_streams();
		bool remove_media_stream(const std::string& mid);
		void clear_media_streams();
		void clear_transports();
		void clear_local_candidates();
		void clear_remote_candidates();

		bool receive_rtp_packet(int port, const uint8_t* rtp_packet, int size);
		bool receive_rtcp_packet(int port, const uint8_t* rtcp_packet, int size);

		litertp::sdp create_offer();
		litertp::sdp create_answer();

		bool set_remote_sdp(const litertp::sdp& sdp, sdp_type_t sdp_type);
		bool set_local_sdp(const litertp::sdp& sdp);

		void require_keyframe();
		bool require_keyframe(const std::string& mid);

		int get_rtp_port(media_type_t mt);
		int get_rtcp_port(media_type_t mt);
	private:
		transport_ptr create_udp_transport(int port,bool reuse_port);
		transport_ptr create_custom_transport(int port, litertp_on_send_packet on_send_packet, void* ctx,bool reuse_port);
		transport_ptr get_transport(int port);
		void remote_transport(int port);
		void clear_no_ref_transports();



		static void s_litertp_on_frame(void* ctx,const char* mid, uint32_t ssrc, uint16_t pt, int frequency, int channels, const av_frame_t* frame);
		static void s_litertp_on_received_require_keyframe(void* ctx, uint32_t ssrc, int mode);
		static void s_transport_connected(void* ctx, int port);
		static void s_transport_disconnected(void* ctx, int port);
		static void s_litertp_on_app_data(void* ctx, const char* mid, const uint8_t* data, size_t size);

		static void s_litertp_on_data_channel_open(void* ctx, const char* mid);
		static void s_litertp_on_data_channel_close(void* ctx, const char* mid);
		static void s_litertp_on_data_channel_message(void* ctx, const char* mid, const uint8_t* message, size_t size);

		void run();

		bool local_group_bundle();
	public:
		sys::callback<litertp_on_frame_t> on_frame;
		sys::callback<litertp_on_received_require_keyframe_t> on_received_require_keyframe;
		sys::callback<litertp_on_connected_t> on_connected;
		sys::callback<litertp_on_disconnected_t> on_disconnected;

		sys::callback<litertp_on_data_channel_open_t> on_data_channel_open;
		sys::callback<litertp_on_data_channel_close_t> on_data_channel_close;
		sys::callback<litertp_on_data_channel_message_t> on_data_channel_message;
	private:
		std::mutex mutex_;
		std::atomic<bool> active_;
		
		sys::signal signal_;
		std::shared_ptr<std::thread> timer_;
		std::atomic<int> signal_times_;

		bool webrtc_ = false;
		bool stun_ = false;
		bool ice_lite_ = false;
		std::string cname_;
		std::string ice_ufrag_;
		std::string ice_pwd_;
		std::string ice_options_;
		
		std::shared_mutex streams_mutex_;
		std::vector<media_stream_ptr> streams_;
		

		std::shared_mutex transports_mutex_;
		std::map<int, transport_ptr> transports_;


	};

	typedef std::shared_ptr<rtp_session> rtp_session_ptr;
}