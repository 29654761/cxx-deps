/**
 * @file sender_video_h265.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */



#pragma once

#include "sender.h"

namespace rtpx
{
	class sender_video_h265 :public sender
	{
	public:
		sender_video_h265(uint8_t pt, uint32_t ssrc, media_type_t mt, const sdp_format& fmt, spdlogger_ptr log);
		virtual ~sender_video_h265();
		
		bool send_frame(const uint8_t* frame, uint32_t size, uint32_t duration);

	private:
		void send_nal(uint32_t duration,const uint8_t* nal, uint32_t nal_size, bool islast);
	};


}

