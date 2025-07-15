/**
 * @file rtp_session.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#pragma once
#include <memory>
#include <map>
#include "media_stream.h"
#include "sdp/sdp.h"
#include "transports/udp_transport.h"
#include "transports/custom_transport.h"

namespace rtpx {

	class custom_transport;
	class rtp_session;
	typedef std::shared_ptr<rtp_session> rtp_session_ptr;

	class rtp_session:public std::enable_shared_from_this<rtp_session>
	{
	public:
		typedef std::function<void()> connected_handler;
		typedef std::function<void()> disconnected_handler;
		typedef std::function<void(const std::string&, const sdp_format&, const av_frame_t&)> frame_handler;
		typedef std::function<void(const std::string&)> send_require_keyframe_handler;
		typedef std::function<void(const std::string&)> receive_require_keyframe_handler;
		typedef std::function<void(const std::string&)> data_channel_opened_handler;
		typedef std::function<void(const std::string&)> data_channel_closed_handler;
		typedef std::function<void(const std::string&, const std::vector<uint8_t>&)> data_channel_message_handler;

		rtp_session(asio::io_context& ioc, spdlogger_ptr log,bool stun=true,bool is_port_mux=false);
		~rtp_session();

		//The start whill be called by create_answer or set_remote_sdp
		void start();
		void stop();

		bool is_timeout()const { return update_.is_timeout(20000); }

		bool create_offer(sdp& offer);
		bool create_answer(sdp& answer);
		bool set_remote_sdp(const sdp& remote_sdp, sdp_type_t type);

		media_stream_ptr create_media_stream(media_type_t mt, const std::string& mid,bool security, const std::string& ms_stream_id = "", const std::string& ms_track_id = "",rtp_trans_mode_t trans_mode=rtp_trans_mode_sendrecv, bool rtcp_mux = true);
		media_stream_ptr create_media_stream(media_type_t mt, const std::string& mid, bool security, custom_transport::send_hander sender, const std::string& ms_stream_id = "", const std::string& ms_track_id = "", rtp_trans_mode_t trans_mode = rtp_trans_mode_sendrecv,bool rtcp_mux = true);
		bool remove_media_stream_by_mid(const std::string& mid);
		std::vector<media_stream_ptr> media_streams() const;
		media_stream_ptr get_media_stream_by_mid(const std::string& mid);
		media_stream_ptr get_media_stream_by_msid(const std::string& sid, const std::string& tid);
		media_stream_ptr get_media_stream_by_remote_ssrc(uint32_t ssrc);
		media_stream_ptr get_media_stream_by_local_ssrc(uint32_t ssrc);
		void clear_media_streams();

		sdp_type_t sdp_role()const { return sdp_role_; }

		void require_keyframe();
		bool require_keyframe(const std::string& mid);
	protected:
		media_stream_ptr create_media_stream(media_type_t mt, const std::string& mid,bool security, transport_ptr rtp,transport_ptr rtcp,
			const std::string& ms_stream_id = "", const std::string& ms_track_id = "", rtp_trans_mode_t trans_mode=rtp_trans_mode_sendrecv);

		transport_ptr create_transport(uint16_t port, bool security);
		transport_ptr create_transport(uint16_t port, bool security, custom_transport::send_hander sender);
		void setup_transport(transport_ptr tp);
		transport_ptr get_transport(uint16_t port) const;
		bool try_remove_transport(uint16_t port);
		std::vector<transport_ptr> transports() const;

		bool create_transport_pair(bool security,bool rtcp_mux,transport_ptr& rtp, transport_ptr& rtcp);
		bool create_transport_pair(bool security, bool rtcp_mux, custom_transport::send_hander sender,transport_ptr& rtp, transport_ptr& rtcp);

	protected:
		void handle_rtp_packet(packet_ptr pkt, const asio::ip::udp::endpoint& ep);
		void handle_stun_completed(uint16_t local_port, const asio::ip::udp::endpoint& ep);
		void handle_dtls_completed(uint16_t local_port, dtls_role_t dtls_role);
		void handle_rtcp_app(const rtcp_app* app, const asio::ip::udp::endpoint& endpoint);
		void handle_rtcp_bye(uint32_t ssrc, const char* reason, const asio::ip::udp::endpoint& endpoint);
		void handle_rtcp_sr(const rtcp_sr* sr, const asio::ip::udp::endpoint& endpoint);
		void handle_rtcp_rr(const rtcp_rr* rr, const asio::ip::udp::endpoint& endpoint);
		void handle_rtcp_sdes(const rtcp_sdes_entry* entry, const asio::ip::udp::endpoint& endpoint);
		void handle_rtcp_fb(const rtcp_fb* fb, const asio::ip::udp::endpoint& endpoint);
	
		void handle_timer(const std::error_code& ec);

	protected:
		void handle_ms_frame(const std::string& mid, const sdp_format& fmt, const av_frame_t& frame);
		void handle_ms_request_keyframe(const std::string& mid);

		void handle_sctp_connected(uint16_t local_port);
		void handle_sctp_disconnected(uint16_t local_port);
		void handle_sctp_heartbeat(uint16_t local_port);
		void handle_sctp_stream_opened(uint16_t local_port,uint16_t stream_id);
		void handle_sctp_stream_closed(uint16_t local_port,uint16_t stream_id);
		void handle_sctp_stream_message(uint16_t local_port,uint16_t stream_id, const std::vector<uint8_t>& message);

	protected:
		dtls_role_t dtls_role(sdp_setup_t remote_setup);
		stun_ice_role_t stun_ice_role();

	protected:
		std::atomic_bool active_;
		asio::io_context& ioc_;
		asio::steady_timer timer_;
		sys::update_time update_;
		uint16_t timer_count_ = 0;
		uint16_t timer_interval_ = 1;
		spdlogger_ptr log_;
		std::string cname_;

		bool remote_ice_lite_ = false;
		sdp_type_t sdp_role_= sdp_type_unknown;
		sdp_setup_t setup_ = sdp_setup_none;
		dtls_role_t dtls_role_ = dtls_role_server;
		stun_ice_role_t ice_role_ = stun_ice_controlling;

		bool stun_ = true;
		bool ice_lite_ = false;

		std::string ice_ufrag_;
		std::string ice_pwd_;
		std::string ice_options_;

		bool is_port_mux_ = true;
		uint16_t mux_port_ = 0;

		mutable std::shared_mutex media_streams_mutex_;
		std::vector<media_stream_ptr> media_streams_;

		mutable std::shared_mutex transports_mutex_;
		std::vector<transport_ptr> transports_;

	public:
		connected_handler on_connected;
		disconnected_handler on_disconnected;
		frame_handler on_frame;
		send_require_keyframe_handler on_send_require_keyframe;
		receive_require_keyframe_handler on_receive_require_keyframe;
		data_channel_opened_handler on_data_channel_opened;
		data_channel_closed_handler on_data_channel_closed;
		data_channel_message_handler on_data_channel_message;
	};
	
}

