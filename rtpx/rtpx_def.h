/**
 * @file litertp.cpp
 * @brief Struct defined.
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

	//#ifdef MSVC

#include "avtypes.h"

#define MAX_RTP_PAYLOAD_SIZE 1200
#define PACKET_BUFFER_SIZE 512

	typedef enum sdp_type_t
	{
		sdp_type_unknown,
		sdp_type_offer,
		sdp_type_answer,
	}sdp_type_t;

	typedef enum sdp_setup_t
	{
		sdp_setup_none,
		sdp_setup_actpass,
		sdp_setup_active,
		sdp_setup_passive,
	}sdp_setup_t;

	typedef enum dtls_role_t
	{
		dtls_role_server,
		dtls_role_client,
	}dtls_role_t;

	typedef enum rtp_trans_mode_t
	{
		rtp_trans_mode_inactive = 0,
		rtp_trans_mode_recvonly = 0x01,
		rtp_trans_mode_sendonly = 0x02,
		rtp_trans_mode_sendrecv = 0x03,
	}rtp_trans_mode_t;

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


	typedef struct _rtp_sender_stats_t
	{
		codec_type_t ct;
		uint8_t pt;
		uint32_t ssrc;
		uint32_t last_timestamp;
		uint16_t last_seq;

		double jitter;

		uint64_t packets_sent;
		uint64_t bytes_sent;
		uint32_t packets_sent_period;
		uint32_t bytes_sent_period;

		uint32_t lost;
		uint16_t lost_period;

		double lsr_unix;
		uint32_t nack;
		uint32_t pli;
		uint32_t fir;

	}rtp_sender_stats_t;

	typedef struct _rtp_receiver_stats_t
	{
		codec_type_t ct;
		uint8_t pt;
		uint32_t ssrc;
		uint32_t last_timestamp;
		uint16_t last_seq;

		double jitter;

		uint64_t packets_received;
		uint64_t bytes_received;
		uint64_t frames_received;
		uint64_t frames_droped;
		uint32_t packets_received_period;
		uint32_t bytes_received_period;

		uint64_t packets_sent;
		uint64_t bytes_sent;
		uint32_t packets_sent_period;
		uint32_t bytes_sent_period;

		uint32_t lost;
		uint16_t lost_period;

		double lsr_unix;

		uint32_t nack;
		uint32_t pli;
		uint32_t fir;
	}rtp_receiver_stats_t;


	typedef struct _rtp_stats_t
	{
		media_type_t mt;
		rtp_sender_stats_t sender_stats;
		rtp_receiver_stats_t receiver_stats;
	}rtp_stats_t;


#ifdef __cplusplus
}
#endif //__cplusplus