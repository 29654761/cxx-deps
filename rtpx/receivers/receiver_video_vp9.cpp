/**
 * @file receiver_video_vp9.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#include "receiver_video_vp9.h"

#include "../rtpx_def.h"
#include "../util/sn.hpp"
#include "../vpx/vp9_header.h"

#include <string.h>

namespace rtpx
{
	receiver_video_vp9::receiver_video_vp9(asio::io_context& ioc, int32_t ssrc, media_type_t mt, const sdp_format& fmt, spdlogger_ptr log)
		:receiver(ioc,ssrc,mt, fmt,log)
	{
	}

	receiver_video_vp9::~receiver_video_vp9()
	{
		stop();
	}

	bool receiver_video_vp9::insert_packet(packet_ptr pkt)
	{
		std::unique_lock<std::shared_mutex>lk(mutex_);
		if (!receiver::insert_packet(pkt))
		{
			return false;
		}

		std::vector<packet_ptr> pkts;
		while (find_a_frame(pkts))
		{
			// A completed nal
			frame_begin_ts_ = std::chrono::high_resolution_clock::now();
			combin_frame(pkts);
		}
		check_for_drop();

		

		return true;
	}


	bool receiver_video_vp9::find_a_frame(std::vector<packet_ptr>& pkts)
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
			if (pkt == nullptr)
			{
				return false;
			}

			idx_lst.push_back(pos);

			if (pkt->header()->m == 1)
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

	void receiver_video_vp9::check_for_drop()
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
			if (log_)
			{
				log_->warn("VP8 Drop rtp packet seq={}", i)->flush();
			}
			recv_packs_[idx].reset();

			//vp9_header h;
			//int pos=h.deserialize(pkt->payload(),0,(int)pkt->payload_size());
			//if (h.non_reference == 0)  // nal ref idc
			//{
			//	waiting_for_keyframe_ = true;
			//	break;
			//}

			if (pkt->header()->m == 1)
			{
				//new start
				frame_begin_ts_ = std::chrono::high_resolution_clock::now();
				break;
			}
		}

		begin_seq_ = i;
		frame_begin_ts_ = std::chrono::high_resolution_clock::now();
	}

	bool receiver_video_vp9::combin_frame(const std::vector<packet_ptr>& pkts)
	{
		if (pkts.size() <= 0) {
			return false;
		}

		packet_ptr first_pkt;
		vp9_header first_pkt_vp9_header;

		std::string frame_data;
		frame_data.reserve(pkts.size()*MAX_RTP_PAYLOAD_SIZE);
		for (auto pkt : pkts)
		{
			const uint8_t* payload = pkt->payload();
			int payload_size = (int)pkt->payload_size();

			vp9_header h;
			int pos=h.deserialize(payload,0, payload_size);
			if (pos < 0)
			{
				continue;
			}
			if (h.b)
			{
				first_pkt = pkt;
				first_pkt_vp9_header = h;
			}
			if (!first_pkt)
			{
				continue;
			}

			int frame_size = payload_size - pos;
			if (frame_size > 0)
			{
				frame_data.append((const char*)payload + pos, frame_size);
			}
			
			if (pkt->header()->m == 1)
			{
				break;
			}
		}


		if (!first_pkt|| frame_data.empty())
		{
			return false;
		}

		if (vp9_header::is_keyframe((const uint8_t*)frame_data.data(),frame_data.size()))
		{
			//is key frame.
			waiting_for_keyframe_ = false;
		}

		av_frame_t frame;
		memset(&frame, 0, sizeof(frame));
		frame.ct = codec_type_vp9;
		frame.mt = media_type_video;
		frame.pts = first_pkt->header()->ts;
		frame.dts = frame.pts;
		//frame.ft = first_pkt_vp8_header.show_frame==1?frame_type_iframe:frame_type_pframe;
		frame.data = (uint8_t*)frame_data.data();
		frame.data_size = (uint32_t)frame_data.size();


		if (!waiting_for_keyframe_)
		{
			invoke_rtp_frame(frame);
			stats_.frames_received++;
		}
		else
		{
			stats_.frames_droped++;
		}

		return true;
	}



}
