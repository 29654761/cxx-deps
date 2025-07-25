/**
 * @file receiver_audio.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#include "receiver_audio.h"

#include "../litertp_def.h"
#include "../util/sn.hpp"

#include <string.h>

namespace litertp
{
	receiver_audio::receiver_audio(int32_t ssrc, media_type_t mt, const sdp_format& fmt)
		:receiver(ssrc,mt,fmt)
	{
		nack_max_count_ = 3;
		useinbandfec_ = fmt.fmtp.useinbandfec();
	}

	receiver_audio::~receiver_audio()
	{
		stop();
	}

	bool receiver_audio::insert_packet(packet_ptr pkt)
	{
		std::unique_lock<std::shared_mutex>lk(mutex_);
		if (!receiver::insert_packet(pkt))
		{
			return false;
		}

		find_a_frame();
		return true;
	}


	void receiver_audio::find_a_frame()
	{
		if (begin_seq_ < 0 || end_seq_ < 0|| sn::ahead_of<uint16_t>(begin_seq_, end_seq_))
		{
			return;
		}

		uint16_t i = begin_seq_;
		while (!sn::ahead_of<uint16_t>(i, end_seq_))
		{
			int pos = i % PACKET_BUFFER_SIZE;

			auto pkt = recv_packs_[pos];
			if (pkt == nullptr)
			{
				if (is_timeout())
				{
					i++; //drop this packet;
					stats_.frames_droped++;
					continue;
				}
				else 
				{
					break;
				}
			}

			av_frame_t frame;
			memset(&frame, 0, sizeof(frame));
			frame.ct = format_.codec;
			frame.mt = media_type_audio;
			frame.pts = pkt->handle_->header->ts;
			frame.dts = frame.pts;
			frame.data = (uint8_t*)pkt->payload();
			frame.data_size = (uint32_t)pkt->payload_size();

			if (useinbandfec_)
			{
				if (has_first_&&last_sent_seq_ + 1 != i)
				{
					frame.fec = 1;
					invoke_rtp_frame(frame);
				}
			}
			frame.fec = 0;
			invoke_rtp_frame(frame);
			last_sent_seq_ = i;
			has_first_ = true;
			stats_.frames_received++;
			recv_packs_[pos].reset();

			i++;
		}

		begin_seq_ = i;
		frame_begin_ts_ = std::chrono::high_resolution_clock::now();
	}


}
