/**
 * @file receiver_video_h264.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#pragma once

#include "receiver.h"
#include "../h264/nal_splicer.h"


namespace litertp
{

	class receiver_video_h264:public receiver
	{
	public:
		receiver_video_h264(int32_t ssrc, media_type_t mt, const sdp_format& fmt);
		virtual ~receiver_video_h264();

		virtual bool insert_packet(packet_ptr pkt);
	private:
		bool find_a_frame(std::vector<packet_ptr>& pkts);

		//check and drop the broken frame;
		void check_for_drop();



		bool combin_frame(const std::vector<packet_ptr>& pkts);

		void commit_fu_frame(std::string& fu_frame_data,packet_ptr& first_pkt);
	
		void invoke_nal_frame(av_frame_t& frame);
	private:
		nal_splicer splicer_;
	};


}