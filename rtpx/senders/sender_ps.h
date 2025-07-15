/**
 * @file sender_ps.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#pragma once

#include "sender.h"

namespace rtpx
{
	class sender_ps :public sender
	{
	public:
		sender_ps(uint8_t pt, uint32_t ssrc, media_type_t mt, const sdp_format& fmt, spdlogger_ptr log);
		virtual ~sender_ps();

		bool send_frame(const uint8_t* frame, uint32_t size, uint32_t duration);

	private:
		bool send_nal(uint32_t duration, const std::vector<std::string>& nals);
		bool send_rtp_frame(const uint8_t* frame, uint32_t size, uint32_t duration);
	};


}

