/**
 * @file sender_video_av1.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */


#pragma once

#include "sender.h"

namespace rtpx
{
	class sender_video_av1 : public sender
	{
	public:
		sender_video_av1(uint8_t pt, uint32_t ssrc, media_type_t mt, const sdp_format& fmt, spdlogger_ptr log);
		virtual ~sender_video_av1();

		bool send_frame(const uint8_t* frame, uint32_t size, uint32_t duration);
	};


}

