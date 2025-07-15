/**
 * @file transport.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */


#pragma once
#include <shared_mutex>
#include <asio.hpp>
#include <sys2/update_time.h>
#include <spdlog/spdlogger.hpp>
#include "../rtpx_def.h"
#include "../packet.h"

#include "../rtp/rtcp_header.h"
#include "../rtp/rtcp_app.h"
#include "../rtp/rtcp_bye.h"
#include "../rtp/rtcp_fb.h"
#include "../rtp/rtcp_report.h"
#include "../rtp/rtcp_rr.h"
#include "../rtp/rtcp_sr.h"
#include "../rtp/rtcp_sdes.h"

#include "../stun/stun_message.h"
#include "../sctp/sctp.h"
#include "../dtls/dtls.h"

namespace rtpx
{
	class transport;
	typedef std::shared_ptr<transport> transport_ptr;

	class transport :public std::enable_shared_from_this<transport>
	{
	public:

		typedef std::function<void(packet_ptr, const asio::ip::udp::endpoint&)> rtp_packet_handler;
		typedef std::function<void(dtls_role_t dtls_role)> dtls_completed_handler;
		typedef std::function<void(const asio::ip::udp::endpoint&)> stun_completed_handler;
		typedef std::function<void(const rtcp_app*, const asio::ip::udp::endpoint&)> rtcp_app_handler;
		typedef std::function<void(uint32_t, const char*, const asio::ip::udp::endpoint&)> rtcp_bye_handler;
		typedef std::function<void(const rtcp_sr*, const asio::ip::udp::endpoint&)> rtcp_sr_handler;
		typedef std::function<void(const rtcp_rr*, const asio::ip::udp::endpoint&)> rtcp_rr_handler;
		typedef std::function<void(const rtcp_sdes_entry*, const asio::ip::udp::endpoint&)> rtcp_sdes_handler;
		typedef std::function<void(const rtcp_fb*, const asio::ip::udp::endpoint&)> rtcp_fb_handler;

		transport(asio::io_context& ioc, uint16_t local_port,spdlogger_ptr log);
		~transport();

		virtual bool open() = 0;
		virtual void close();
		virtual bool send(const uint8_t* data, size_t size, const asio::ip::udp::endpoint& endpoint) = 0;

		bool dtls_send(const uint8_t* data, size_t size, const asio::ip::udp::endpoint& endpoint);
		bool send_rtp_packet(packet_ptr packet, const asio::ip::udp::endpoint& endpoint);
		bool send_rtcp_packet(const uint8_t* rtcp_data, size_t size, const asio::ip::udp::endpoint& endpoint);
		bool send_stun_request(uint32_t local_priority,const asio::ip::udp::endpoint& endpoint);
		bool send_dtls_request(const asio::ip::udp::endpoint& endpoint);
		bool sctp_send(uint16_t sid,const uint8_t* data, size_t size);

		uint16_t local_port()const { return local_port_; }
		bool is_timeout()const { return update_.is_timeout(30000); }

		bool enable_dtls(bool enabled);
		std::string fingerprint() const;

		void set_local_ice_options(const std::string& ufrag, const std::string& pwd) {
			ice_ufrag_local_ = ufrag;
			ice_pwd_local_ = pwd;
		}
		void set_remote_ice_options(const std::string& ufrag, const std::string& pwd) {
			ice_ufrag_remote_ = ufrag;
			ice_pwd_remote_ = pwd;
		}

		void set_dtls_role(dtls_role_t role) { dtls_role_ = role; }
		void set_ice_role(stun_ice_role_t role) { ice_role_ = role; }

		void received_message(const uint8_t* buffer, size_t size,const asio::ip::udp::endpoint& endpoint);

		void sctp_init(uint16_t local_port);
		bool sctp_open_stream(uint16_t& sid);
		void sctp_close_stream(uint16_t sid);
		void sctp_set_remote_port(uint16_t port) { sctp_remote_port_ = port; }

		static std::string make_key(const asio::ip::udp::endpoint& endpoint);
		static std::string make_key(const std::string& ip, uint16_t port);

	protected:
		proto_type_t detect_message(uint8_t b);
		bool detect_rtcp_packet(const uint8_t* buffer, int size, rtcp_header& hdr);

		void handle_rtp_message(const uint8_t* buffer, int size, const asio::ip::udp::endpoint& endpoint);
		void handle_rtp_packet(packet_ptr packet, const asio::ip::udp::endpoint& endpoint);
		void handle_rtcp_message(const uint8_t* buffer, int size,const rtcp_header& hdr, const asio::ip::udp::endpoint& endpoint);
		void handle_dtls_message(const uint8_t* buffer, int size, const asio::ip::udp::endpoint& endpoint);
		void handle_dtls_completed(const asio::ip::udp::endpoint& ep);
		void handle_app_message(const uint8_t* buffer, int size, const asio::ip::udp::endpoint& endpoint);
		void handle_stun_message(const uint8_t* buffer, int size, const asio::ip::udp::endpoint& endpoint);
		void handle_stun_completed(const asio::ip::udp::endpoint& endpoint);

		void handle_rtcp_app(const rtcp_app* app, const asio::ip::udp::endpoint& endpoint);
		void handle_rtcp_bye(const rtcp_bye* bye, const asio::ip::udp::endpoint& endpoint);
		void handle_rtcp_sr(const rtcp_sr* sr, const asio::ip::udp::endpoint& endpoint);
		void handle_rtcp_rr(const rtcp_rr* rr, const asio::ip::udp::endpoint& endpoint);
		void handle_rtcp_sdes(const rtcp_sdes* sdes, const asio::ip::udp::endpoint& endpoint);
		void handle_rtcp_feedback(const rtcp_fb* fb, const asio::ip::udp::endpoint& endpoint);
		
	protected:
		void handle_sctp_send_data(const std::vector<uint8_t>& data, const asio::ip::udp::endpoint& endpoint) ;
		void handle_sctp_connected() ;
		void handle_sctp_disconnected() ;
		void handle_sctp_heartbeat() ;
		void handle_sctp_stream_opened(uint16_t stream_id) ;
		void handle_sctp_stream_closed(uint16_t stream_id) ;
		void handle_sctp_stream_message(uint16_t stream_id, const std::vector<uint8_t>& message) ;


	public:
		rtp_packet_handler on_rtp_packet;
		dtls_completed_handler on_dtls_completed;
		stun_completed_handler on_stun_completed;
		rtcp_app_handler on_rtcp_app;
		rtcp_bye_handler on_rtcp_bye;
		rtcp_sr_handler on_rtcp_sr;
		rtcp_rr_handler on_rtcp_rr;
		rtcp_sdes_handler on_rtcp_sdes;
		rtcp_fb_handler on_rtcp_fb;

		sctp::connected_handler on_sctp_connected;
		sctp::disconnected_handler on_sctp_disconnected;
		sctp::heartbeat_handler on_sctp_heartbeat;
		sctp::stream_opened_handler on_sctp_stream_opened;
		sctp::stream_closed_handler on_sctp_stream_closed;
		sctp::stream_message_handler on_sctp_stream_message;

	protected:
		asio::io_context& ioc_;
		spdlogger_ptr log_;
		sys::update_time update_;
		uint16_t local_port_=0;

		std::string ice_ufrag_remote_;
		std::string ice_pwd_remote_;
		std::string ice_ufrag_local_;
		std::string ice_pwd_local_;
		uint16_t local_ice_network_id_ = 1;
		uint16_t local_ice_network_cost_ = 10;

		dtls_role_t dtls_role_ = dtls_role_server;
		stun_ice_role_t ice_role_ = stun_ice_controlling;
		uint64_t tie_breaker_ = 0;

		std::recursive_mutex sctp_mutex_;
		sctp_ptr sctp_;
		uint16_t sctp_remote_port_ = 0;

		dtls_ptr dtls_;

		bool handshake_ = false;
#ifdef RTPX_SSL
		std::mutex srtp_mutex_;
		srtp_t srtp_in_ = nullptr;
		srtp_policy_t srtp_in_policy_ = { };
		srtp_t srtp_out_ = nullptr;
		srtp_policy_t srtp_out_policy_ = { };
#endif
	};


}

