/**
 * @file receiver_video_av1.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#include "receiver_video_av1.h"

#include "../rtpx_def.h"
#include "../util/sn.hpp"
#include "../av1/obu.h"


#include <sys2/util.h>
#include <string.h>

namespace rtpx
{
	receiver_video_av1::receiver_video_av1(asio::io_context& ioc, int32_t ssrc, media_type_t mt, const sdp_format& fmt, spdlogger_ptr log)
		:receiver(ioc,ssrc,mt,fmt,log)
	{
	}

	receiver_video_av1::~receiver_video_av1()
	{
		stop();
	}



	bool receiver_video_av1::insert_packet(packet_ptr pkt)
	{
		std::unique_lock<std::shared_mutex>lk(mutex_);
		if (!receiver::insert_packet(pkt))
		{
			return false;
		}



		std::vector<packet_ptr> frame;
		while (find_a_frame(frame))
		{
			// A completed nal
			combin_frame(frame);
		}
		
		check_for_drop();

		

		return true;
	}


	bool receiver_video_av1::find_a_frame(std::vector<packet_ptr>& pkts)
	{
		pkts.clear();
		if (begin_seq_ < 0 || end_seq_ < 0|| sn::ahead_of<uint16_t>(begin_seq_, end_seq_))
		{
			return false;
		}

		std::vector<int> idx_lst;
		uint16_t i = begin_seq_;
		bool detected = false;
		while (!sn::ahead_of<uint16_t>(i, end_seq_))
		{
			int pos = i % PACKET_BUFFER_SIZE;
			i++;

			auto pkt = recv_packs_[pos];
			if (!pkt)
			{
				return false;
			}

			idx_lst.push_back(pos);

			if (pkt->header()->m==1)
			{
				detected = true;
				break;
			}
		}

		if (!detected||idx_lst.size() == 0)
		{
			return false;
		}

		begin_seq_ = i;
		frame_begin_ts_ = std::chrono::high_resolution_clock::now();

		pkts.reserve(idx_lst.size());
		for (int idx : idx_lst)
		{
			pkts.push_back(recv_packs_[idx]);
			recv_packs_[idx].reset();
		}

		return true;
	}

	void receiver_video_av1::check_for_drop()
	{
		if (begin_seq_ < 0 || end_seq_ < 0 || sn::ahead_of<uint16_t>(begin_seq_, end_seq_))
		{
			return;
		}
		if (!is_timeout())
		{
			return;
		}

		uint16_t i = begin_seq_;
		while (!sn::ahead_of<uint16_t>(i, end_seq_))
		{
			int idx = i % PACKET_BUFFER_SIZE;
			i++;

			auto pkt = recv_packs_[idx];
			if (!pkt)
			{
				continue;
			}

			//h264::nal_header_t fui = { 0 };
			//h264::nal_header_set(&fui, pkt->payload()[0]);

			//if (fui.nri > 0)  // nal ref idc
			//{
			//	waiting_for_keyframe_ = true;
			//}
			if (log_)
			{
				log_->warn("H264 Drop rtp packet seq={}",i)->flush();
			}
			recv_packs_[idx].reset();

			if (pkt->header()->m==1)
			{
				//new start
				frame_begin_ts_ = std::chrono::high_resolution_clock::now();
				break;
			}
		}

		begin_seq_ = i;
	}

	bool receiver_video_av1::combin_frame(const std::vector<packet_ptr>& pkts)
	{
		if (pkts.size() <= 0) {
			return false;
		}

		std::string obu_data;
		for (auto pkt : pkts)
		{
			const uint8_t* payload = pkt->payload();
			int payload_size = (int)pkt->payload_size();

			if (payload_size < 1)
			{
				continue;
			}

			obu::aggr_header_t aggr = {};
			obu::decode_aggr_header(payload[0], &aggr);

			std::vector<obu::element_t> obus;
			obu::find_elements(payload + 1, payload_size - 1, aggr.w, obus);

			for (auto itr = obus.begin(); itr != obus.end(); itr++)
			{
				if (itr == obus.begin()) //First obu is a segment
				{
					if (aggr.z == 0 || aggr.n == 1)
					{
						invoke_obu(obu_data, pkt->header()->ts);
					}
				}


				if (aggr.z == 0 || aggr.n == 1||obu_data.size() > 0) {
					obu_data.append((const char*)itr->data, itr->size);
				}
				if (itr == obus.end()-1) //Last obu is end of unit
				{
					if (aggr.y == 0)
					{
						invoke_obu(obu_data, pkt->header()->ts);
					}
				}
				else //Not last obu
				{
					invoke_obu(obu_data, pkt->header()->ts);
				}
			}
			
		}

		return true;
	}

	void receiver_video_av1::invoke_obu(std::string& obu, int64_t ts)
	{
		if (obu.empty())
			return;

		av_frame_t frame;
		memset(&frame, 0, sizeof(frame));
		frame.ct = codec_type_av1;
		frame.mt = media_type_video;
		frame.pts = ts;
		frame.dts = frame.pts;
		frame.data = (uint8_t*)obu.data();
		frame.data_size = obu.size();

		obu::header_t hdr = {};
		obu::decode_header(frame.data, 0, frame.data_size, &hdr);

		invoke_rtp_frame(frame);
		stats_.frames_received++;
		obu.clear();
	}
}
