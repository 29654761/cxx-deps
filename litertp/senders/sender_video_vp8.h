/**
 * @file sender_video_vp8.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#pragma once

#include "sender.h"

namespace litertp
{
	class sender_video_vp8:public sender
	{
	public:
		sender_video_vp8(uint8_t pt, uint32_t ssrc, media_type_t mt, const sdp_format& fmt);
		virtual ~sender_video_vp8();

		bool send_frame(const uint8_t* frame, uint32_t size, uint32_t duration);

	private:
		bool send_nal(uint32_t duration, int pt, const uint8_t* nal, uint32_t nal_size, bool islast);

		static bool find_next_nal(const uint8_t* buffer, int size, int& offset, int& nal_start, int& nal_size,bool& islast);
	};


}

