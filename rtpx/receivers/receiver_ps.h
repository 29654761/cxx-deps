/**
 * @file receiver_ps.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#pragma once

#include "receiver.h"


namespace rtpx
{

	class receiver_ps :public receiver
	{
	public:
		receiver_ps(asio::io_context& ioc, int32_t ssrc, media_type_t mt, const sdp_format& fmt, spdlogger_ptr log);
		virtual ~receiver_ps();

		virtual bool insert_packet(packet_ptr pkt);
	private:
		bool find_a_frame(std::vector<packet_ptr>& pkts);
		void check_for_drop();
		void combin_frame(const std::vector<packet_ptr>& pkts);


	};


}