/**
 * @file receiver_video_h265.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#include "receiver_video_h265.h"

#include "../litertp_def.h"
#include "../util/sn.hpp"
#include "../h265/h265.h"
#include "../log.h"


#include <sys2/util.h>
#include <string.h>

namespace litertp
{
	receiver_video_h265::receiver_video_h265(int32_t ssrc, media_type_t mt, const sdp_format& fmt)
		:receiver(ssrc,mt,fmt)
	{
	}

	receiver_video_h265::~receiver_video_h265()
	{
		stop();
	}



	bool receiver_video_h265::insert_packet(packet_ptr pkt)
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


	bool receiver_video_h265::find_a_frame(std::vector<packet_ptr>& pkts)
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

			if (pkt->handle_->header->m==1)
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

	void receiver_video_h265::check_for_drop()
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

			h265::nal_header_t fui = { 0 };
			h265::nal_header_set(&fui, pkt->payload(),pkt->payload_size());


			LOGD("drop packet %d\n",idx);
			recv_packs_[idx].reset();

			if (pkt->handle_->header->m==1)
			{
				//new start
				frame_begin_ts_ = std::chrono::high_resolution_clock::now();
				break;
			}
		}

		begin_seq_ = i;
	}

	bool receiver_video_h265::combin_frame(const std::vector<packet_ptr>& pkts)
	{
		if (pkts.size() <= 0) {
			return false;
		}

		packet_ptr first_pkt;
		std::string fu_frame_data;
		
		for (auto pkt : pkts)
		{
			const uint8_t* payload = pkt->payload();
			int payload_size = (int)pkt->payload_size();

			if (payload_size < 2)
			{
				continue;
			}

			h265::nal_header_t fui = { 0 };
			h265::nal_header_set(&fui, payload, payload_size);
			if (fui.f != 0)
			{
				continue;
			}
			av_frame_t frame;
			memset(&frame, 0, sizeof(frame));
			frame.ct = codec_type_h264;
			frame.mt = media_type_video;
			if (fui.type < 48)
			{
				if (h265::is_key_frame((h265::nal_type_t)fui.type))
				{
					waiting_for_keyframe_ = false;
				}
				std::string frame_data;
				frame_data.reserve(payload_size + 4);

				frame_data.append(3, 0);
				frame_data.append(1, 1);
				frame_data.append((const char*)payload, payload_size);
				
				frame.pts = pkt->handle_->header->ts;
				frame.dts = frame.pts;
				frame.data = (uint8_t*)frame_data.data();
				frame.data_size = (int32_t)frame_data.size();



				if (!waiting_for_keyframe_)
				{
					invoke_rtp_frame(frame);
					stats_.frames_received++;
				}
				else
				{
					stats_.frames_droped++;
				}
			}
			else if (fui.type == 48) //AP
			{
				frame.pts = pkt->handle_->header->ts;
				frame.dts = frame.pts;
				const uint8_t* buf = payload + 2; //skip fui
				int size = payload_size - 2;

				
				while (size >= 2)
				{
					uint16_t nal_size = 0;
					nal_size = (buf[0] << 8) | buf[1];
					const uint8_t* nal_data = buf + 2;
					if (nal_size > size - 2)
						break;

					h265::nal_header_t nalh = { 0 };
					h265::nal_header_set(&nalh, nal_data,nal_size);

					if (h265::is_key_frame((h265::nal_type_t)nalh.type))
					{
						waiting_for_keyframe_ = false;
					}
					

					std::string frame_data;
					frame_data.reserve(nal_size + 4);
					frame_data.append(3, 0);
					frame_data.append(1, 1);
					frame_data.append((const char*)nal_data, nal_size);

					if (!waiting_for_keyframe_)
					{
						frame.data = (uint8_t*)frame_data.data();
						frame.data_size = (uint32_t)frame_data.size();
						invoke_rtp_frame(frame);
						stats_.frames_received++;
					}
					else
					{
						stats_.frames_droped++;
					}

					buf += (nal_size + 2);
					size -= (nal_size + 2);
				}
				
			}
			else if (fui.type ==49)  //FU
			{
				frame.pts = pkt->handle_->header->ts;
				frame.dts = frame.pts;


				h265::fu_header_t fuh = { 0 };
				h265::fu_header_set(&fuh, payload[2]);

				if (fuh.s == 1)
				{
					first_pkt = pkt;

					fu_frame_data = "";
					fu_frame_data.clear();
					fu_frame_data.reserve(MAX_RTP_PAYLOAD_SIZE* pkts.size() + 4);
					fu_frame_data.append(3, 0);
					fu_frame_data.append(1, 1);

					h265::nal_header_t nalh = { 0 };
					nalh.f = fui.f;
					nalh.layer = fui.layer;
					nalh.tid = fui.tid;
					nalh.type = fuh.t;
					uint8_t nalhv[2] = {};
					h265::nal_header_get(&nalh, nalhv,2);
					fu_frame_data.append((const char*)nalhv, 2);
				}
				if (!first_pkt)
				{
					continue;
				}

				


				int skip_nal_data = 3;
				const uint8_t* buf = payload + skip_nal_data; //skip fui and fu header (and don for FU-B)
				size_t size = payload_size - skip_nal_data;
				fu_frame_data.append((const char*)buf, size);

			}
			else
			{
				return false;
			}
		}


		if (fu_frame_data.size() > 0)
		{
			av_frame_t frame;
			memset(&frame, 0, sizeof(frame));
			frame.ct = codec_type_h264;
			frame.mt = media_type_video;
			frame.pts = first_pkt->handle_->header->ts;
			frame.dts = frame.pts;
			frame.data = (uint8_t*)fu_frame_data.data();
			frame.data_size = (uint32_t)fu_frame_data.size();

			h265::nal_header_t nalh = { 0 };
			h265::nal_header_set(&nalh, frame.data + 4, frame.data_size - 4);
			if (h265::is_key_frame((h265::nal_type_t)nalh.type))
			{
				waiting_for_keyframe_ = false;
			}

			if (!waiting_for_keyframe_)
			{
				invoke_rtp_frame(frame);
				stats_.frames_received++;
			}
			else
			{
				stats_.frames_droped++;
			}
		}

		return true;
	}


}
