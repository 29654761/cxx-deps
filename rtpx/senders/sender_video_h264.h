/**
 * @file sender_video_h264.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */



#pragma once

#include "sender.h"

namespace rtpx
{
	class sender_video_h264:public sender
	{
	public:
		// aggregate: 多个小 NAL 是否合并成 STAP-A 发送，默认开（WebRTC 链路依赖）。
		// 关闭后每个 NAL 都按单 NAL 包或 FU-A 发送，供不支持 STAP-A 的 SIP/H.323 终端使用
		sender_video_h264(uint8_t pt, uint32_t ssrc, media_type_t mt, const sdp_format& fmt, spdlogger_ptr log, bool aggregate = true);
		virtual ~sender_video_h264();
		
		bool send_frame(const uint8_t* frame, uint32_t size, uint32_t duration);

	private:
		// end_of_frame: 这组 NAL 是否为一帧的结尾。SPS/PPS 传 false：不打 Mark、时间戳不前进，与随后的 IDR 同属一帧
		void send_nals(const std::vector<std::string>& nals, uint32_t duration, bool end_of_frame = true);
		void send_nal(uint32_t duration,const uint8_t* nal, uint32_t nal_size, bool islast);
	private:
		std::string sps_;
		std::string pps_;
		bool aggregate_ = true; // 是否允许 STAP-A 聚合
	};


}

