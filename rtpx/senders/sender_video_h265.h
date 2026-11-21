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
		// aggregate: 多个小 NAL 是否合并成 AP（类型 48）发送，默认开。
		// 关闭后每个 NAL 都按单 NAL 包或 FU 发送，供不支持聚合包的 SIP/H.323 终端使用
		sender_video_h265(uint8_t pt, uint32_t ssrc, media_type_t mt, const sdp_format& fmt, spdlogger_ptr log, bool aggregate = true);
		virtual ~sender_video_h265();
		
		bool send_frame(const uint8_t* frame, uint32_t size, uint32_t duration);

	private:
		void send_nal(uint32_t duration,const uint8_t* nal, uint32_t nal_size, bool islast);
	private:
		bool aggregate_ = true; // 是否允许 AP 聚合
	};


}

