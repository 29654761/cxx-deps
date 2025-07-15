/**
 * @file sender_rtx.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */


#pragma once

#include "sender.h"

namespace rtpx
{
	class sender_rtx:public sender
	{
	public:
		sender_rtx(uint8_t rtx_pt,uint32_t rtx_ssrc, media_type_t mt, const sdp_format& fmt, spdlogger_ptr log);
		virtual ~sender_rtx();

		bool send_frame(const uint8_t* frame, uint32_t size, uint32_t duration);
		virtual bool send_packet(packet_ptr pkt);

	};


}

