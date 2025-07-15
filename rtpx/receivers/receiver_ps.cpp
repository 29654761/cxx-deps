/**
 * @file receiver_ps.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#include "receiver_ps.h"

#include "../rtpx_def.h"
#include "../util/sn.hpp"
#include "../mpeg/ps_frame.h"

#include <string.h>

namespace rtpx
{
	receiver_ps::receiver_ps(asio::io_context& ioc, int32_t ssrc, media_type_t mt, const sdp_format& fmt, spdlogger_ptr log)
		:receiver(ioc,ssrc,mt,fmt,log)
	{
		nack_max_count_ = 0;
	}

	receiver_ps::~receiver_ps()
	{
		stop();
	}

	bool receiver_ps::insert_packet(packet_ptr pkt)
	{
		std::unique_lock<std::shared_mutex>lk(mutex_);
		if (!receiver::insert_packet(pkt))
		{
			return false;
		}

		std::vector<packet_ptr> frame;
		while (find_a_frame(frame))
		{
			if (frame.size() > 0) {
				// A completed nal
				combin_frame(frame);
			}
		}

		check_for_drop();


		return true;
	}


	bool receiver_ps::find_a_frame(std::vector<packet_ptr>& pkts)
	{
		pkts.clear();
		if (begin_seq_ < 0 || end_seq_ < 0 || sn::ahead_of<uint16_t>(begin_seq_, end_seq_))
		{
			return false;
		}

		std::vector<int> idx_lst;
		uint16_t i = begin_seq_;
		bool detected = false;
		uint32_t ts = 0;
		while (!sn::ahead_of<uint16_t>(i, end_seq_))
		{
			int pos = i % PACKET_BUFFER_SIZE;
			auto pkt = recv_packs_[pos];
			if (!pkt)
			{
				return false;
			}
			if (ts == 0)
			{
				ts = pkt->header()->ts;
			}

			if (ts != pkt->header()->ts)
			{
				detected = true;
				break;
			}

			idx_lst.push_back(pos);
			i++;

			if (pkt->header()->m == 1)
			{
				detected = true;
				break;
			}
			
		}

		if (!detected || idx_lst.size() == 0)
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

	void receiver_ps::check_for_drop()
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
		uint32_t ts = 0;
		while (!sn::ahead_of<uint16_t>(i, end_seq_))
		{
			int idx = i % PACKET_BUFFER_SIZE;
			auto pkt = recv_packs_[idx];
			if (!pkt)
			{
				i++;
				continue;
			}

			if (ts == 0)
			{
				ts = pkt->header()->ts;
			}

			if (ts != pkt->header()->ts)
			{
				//new start
				frame_begin_ts_ = std::chrono::high_resolution_clock::now();
				break;
			}
			
			recv_packs_[idx].reset();
			i++;

			if (pkt->header()->m == 1)
			{
				//new start
				frame_begin_ts_ = std::chrono::high_resolution_clock::now();
				break;
			}
		}

		begin_seq_ = i;
	}

	void receiver_ps::combin_frame(const std::vector<packet_ptr>& pkts)
	{
		std::string data;
		data.reserve(pkts.size() * MAX_RTP_PAYLOAD_SIZE);

		for (auto itr = pkts.begin(); itr != pkts.end(); itr++)
		{
			data.append((const char*)(*itr)->payload(), (*itr)->payload_size());
		}

		mpeg::ps_frame psframe;
		mpeg::sys_header sys;
		mpeg::psm2 psm;
		std::vector<mpeg::pes2> pes_v;
		std::vector<mpeg::pes2> pes_a;
		bool iskeyframe = false;
		if (psframe.deserialize((const uint8_t*)data.data(), data.size()))
		{
			for (auto itr = psframe.blocks.begin(); itr != psframe.blocks.end(); itr++)
			{
				if (itr->start_code == PS_STREAM_ID_SYSTEM_HEADER)
				{
					iskeyframe = true;
					sys.deserialize((const uint8_t*)itr->block.data(), itr->block.size());
				}
				else if (itr->start_code == PS_STREAM_ID_MAP)
				{
					psm.deserialize((const uint8_t*)itr->block.data(), itr->block.size());
				}
				else if (itr->start_code == PS_STREAM_ID_VIDEO_START)
				{
					mpeg::pes2 pes;
					if (pes.deserialize(itr->start_code, (const uint8_t*)itr->block.data(), itr->block.size()))
					{
						pes_v.push_back(pes);
					}
				}
				else if (itr->start_code == PS_STREAM_ID_AUDIO_START)
				{
					mpeg::pes2 pes;
					if (pes.deserialize(itr->start_code, (const uint8_t*)itr->block.data(), itr->block.size()))
					{
						pes_a.push_back(pes);
					}
				}
			}
		}

		if (pes_v.size() > 0)
		{
			if (iskeyframe)
			{
				waiting_for_keyframe_ = false;
			}

			if (!waiting_for_keyframe_)
			{
				std::string vframe_data;
				for (auto itr = pes_v.begin(); itr != pes_v.end(); itr++)
				{
					vframe_data.append(itr->pes_packet_data_byte);
				}
				av_frame_t frame;
				memset(&frame, 0, sizeof(frame));
				frame.ct = codec_type_h264;
				frame.mt = media_type_video;
				frame.pts = pes_v[0].pts;
				if (pes_v[0].pts_dts_flags == 3) {
					frame.dts = pes_v[0].dts;
				}
				else
				{
					frame.dts = frame.pts;
				}
				frame.data = (uint8_t*)vframe_data.data();
				frame.data_size = (uint32_t)vframe_data.size();

				invoke_rtp_frame(frame);
				stats_.frames_received++;
				/*
				std::string file = "e:\\ps\\h264";
				FILE* f = fopen(file.c_str(), "ab+");
				if (f)
				{
					fwrite(frame.data, 1, frame.data_size, f);
					fclose(f);
				}
				*/
			}
			else
			{
				stats_.frames_droped++;
			}
		}
		
		if (pes_a.size())
		{
			std::string aframe_data;
			for (auto itr = pes_a.begin(); itr != pes_a.end(); itr++)
			{
				aframe_data.append(itr->pes_packet_data_byte);
			}
			av_frame_t frame;
			memset(&frame, 0, sizeof(frame));
			frame.ct = codec_type_pcma;
			frame.mt = media_type_audio;
			frame.pts = pes_a[0].pts;
			if (pes_a[0].pts_dts_flags == 3) {
				frame.pts = pes_a[0].dts;
			}
			else
			{
				frame.dts = frame.pts;
			}
			frame.data = (uint8_t*)aframe_data.data();
			frame.data_size = (uint32_t)aframe_data.size();

			invoke_rtp_frame(frame);
			stats_.frames_received++;
		}
		
		
		
		/*
		av_frame_t frame;
		memset(&frame, 0, sizeof(frame));
		frame.ct = codec_type_h264;
		frame.mt = media_type_video;
		frame.pts = 0;
		frame.dts = frame.pts;
		frame.data = (uint8_t*)data.data();
		frame.data_size = data.size();

		invoke_rtp_frame(frame);
		*/
	}

}
