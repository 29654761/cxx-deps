/**
 * @file transport.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#pragma once

#include "../packet.h"
#include "../litertp_def.h"
#include "../stun/stun_message.h"
#include "../sctp/sctp.h"

#include <memory>
#include <thread>
#include <sys2/socket.h>
#include <sys2/mutex_callback.hpp>




namespace litertp {

	typedef enum proto_type_t
	{
		proto_unknown = 0,
		proto_rtp = 1,
		proto_dtls = 2,
		proto_stun = 3,
	}proto_type_t;

	typedef enum stun_ice_role_t
	{
		stun_ice_controlling,
		stun_ice_controlled,
	} stun_ice_role_t;

	class transport
	{
	public:
		typedef void (*transport_rtp_packet)(void* ctx, std::shared_ptr<sys::socket> skt, packet_ptr packet, const sockaddr* addr, int addr_size);
		typedef void (*transport_rtcp_packet)(void* ctx, std::shared_ptr<sys::socket> skt, uint16_t pt, const uint8_t* buffer, size_t size, const sockaddr* addr, int addr_size);
		typedef void (*transport_stun_message)(void* ctx, std::shared_ptr<sys::socket> skt, const stun_message& msg, const sockaddr* addr, int addr_size);
		typedef void (*transport_connected)(void* ctx, int port);
		typedef void (*transport_disconnected)(void* ctx,int port);

		typedef void (*transport_raw_packet)(void* ctx, std::shared_ptr<sys::socket> skt, 
			const uint8_t* data,size_t data_size,
			const sockaddr* addr, int addr_size);

		typedef void (*transport_dtls_handshake)(void* ctx, srtp_role_t role);
		transport();
		virtual ~transport();


		virtual bool start();
		virtual void stop();

		virtual bool send_rtp_packet(packet_ptr packet, const sockaddr* addr, int addr_size) = 0;
		virtual bool send_rtcp_packet(const uint8_t* rtcp_data, int size, const sockaddr* addr, int addr_size) = 0;
		virtual void send_stun_request(const sockaddr* addr, int addr_size, uint32_t priority)=0;
		virtual bool send_raw_packet(const uint8_t* raw_data, int size, const sockaddr* addr, int addr_size) = 0;

		virtual bool enable_security(bool enabled)=0;
		virtual std::string fingerprint() const= 0;

		bool test_rtcp_packet(const uint8_t* data, int size, int* pt);

		bool receive_rtp_packet(const uint8_t* rtp_packet, int size);
		bool receive_rtp_packet(const packet_ptr pkt);
		bool receive_rtcp_packet(const uint8_t* rtcp_data, int size);


		void set_raw_socket(bool raw_socket) { raw_socket_ = raw_socket; }

		void init_sctp(int local_port);

	protected:
		static void s_send_sctp_packet(void* ctx, const std::vector<uint8_t>& data, const sockaddr* addr, socklen_t addr_size);
		virtual void on_dtls_handshake(srtp_role_t role);
		virtual void on_app_data(const uint8_t* data, int size, const sockaddr* addr, int addr_size);
	public:

		std::recursive_mutex mutex_;
		bool active_ = false;
		std::shared_ptr<sys::socket> socket_;

		sys::mutex_callback<transport_rtp_packet> rtp_packet_event_;
		sys::mutex_callback<transport_rtcp_packet> rtcp_packet_event_;
		sys::mutex_callback<transport_stun_message> stun_message_event_;
		sys::mutex_callback<transport_connected> connected_event_;
		sys::mutex_callback<transport_disconnected> disconnected_event_;
		sys::mutex_callback<transport_raw_packet> raw_packet_event_;
		sys::mutex_callback<transport_dtls_handshake> dtls_handshake_event_;

		srtp_role_t srtp_role_ = srtp_role_server;
		stun_ice_role_t ice_role_ = stun_ice_controlling;
		sdp_type_t sdp_type_ = sdp_type_t::sdp_type_answer;
		uint64_t tie_breaker_ = 0;

		int port_ = 0;

		std::string ice_ufrag_remote_;
		std::string ice_pwd_remote_;
		std::string ice_ufrag_local_;
		std::string ice_pwd_local_;
		uint16_t local_ice_network_id_ = 1;
		uint16_t local_ice_network_cost_ = 10;

		bool raw_socket_ = false;

		sctp_ptr sctp_;
		uint16_t sctp_remote_port_ = 0;
	};


	typedef std::shared_ptr<transport> transport_ptr;
}
