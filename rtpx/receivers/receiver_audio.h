/**
 * @file receiver_audio.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#pragma once

#include "receiver.h"


namespace rtpx
{

	class receiver_audio:public receiver
	{
	public:
		receiver_audio(asio::io_context& ioc, int32_t ssrc, media_type_t mt, const sdp_format& fmt,spdlogger_ptr log);
		virtual ~receiver_audio();

		virtual bool insert_packet(packet_ptr pkt);
	private:
		void find_a_frame();
		bool useinbandfec_ = false;
		bool has_first_ = false;
		uint32_t last_sent_seq_ = 0;
	};


}