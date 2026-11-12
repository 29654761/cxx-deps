/**
 * @file receiver_video_h264.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#include "receiver_video_h264.h"

#include "../rtpx_def.h"
#include "../util/sn.hpp"
#include "../h264/h264.h"


#include <sys2/util.h>
#include <string.h>

namespace rtpx
{
	receiver_video_h264::receiver_video_h264(asio::io_context& ioc, int32_t ssrc, media_type_t mt, const sdp_format& fmt, spdlogger_ptr log)
		:receiver(ioc,ssrc,mt,fmt,log)
	{
	}

	receiver_video_h264::~receiver_video_h264()
	{
		stop();
	}



	bool receiver_video_h264::insert_packet(packet_ptr pkt)
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


	bool receiver_video_h264::find_a_frame(std::vector<packet_ptr>& pkts)
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

	void receiver_video_h264::check_for_drop()
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
		uint16_t drop_begin = i;
		int dropped = 0;
		int holes = 0;
		bool has_nri = false;
		while (!sn::ahead_of<uint16_t>(i, end_seq_))
		{
			int idx = i % PACKET_BUFFER_SIZE;
			i++;

			auto pkt = recv_packs_[idx];
			if (!pkt)
			{
				holes++;
				continue;
			}

			h264::nal_header_t fui = { 0 };
			h264::nal_header_set(&fui, pkt->payload()[0]);

			if (fui.nri > 0)  // nal ref idc
			{
				waiting_for_keyframe_ = true;
				has_nri = true;
			}
			recv_packs_[idx].reset();
			dropped++;

			if (pkt->header()->m==1)
			{
				//new start
				frame_begin_ts_ = std::chrono::high_resolution_clock::now();
				break;
			}
		}

		begin_seq_ = i;

		// 一帧一条汇总（原先逐包打 warning，丢一个大 I 帧就刷几十行）：
		// holes>0＝seq 范围内有从未到达的包（真丢包，NACK 也没补回来）；
		// holes=0＝包都在但组帧条件不满足（如尾部 marker 未到，对端分包/发送异常）
		if (log_ && dropped > 0)
		{
			log_->warn("H264 dropped incomplete frame: seq={}~{} dropped={} holes={} nri={} waiting_keyframe={}",
				drop_begin, (uint16_t)(i - 1), dropped, holes, has_nri ? 1 : 0, waiting_for_keyframe_ ? 1 : 0)->flush();
		}
	}

	bool receiver_video_h264::combin_frame(const std::vector<packet_ptr>& pkts)
	{
		if (pkts.size() <= 0) {
			return false;
		}

		packet_ptr first_pkt;
		std::string fu_frame_data;

		// 同一帧（find_a_frame 按 marker 收齐的一组包）里的所有 NAL 先按 Annex-B 拼成一个访问单元，最后只回调一次。
		// 原先逐 NAL 回调，多 slice 帧（如小鱼 H.239 辅流每帧上下两个 slice）会被下游当成多帧：
		// 同一 pts 写两次、每次各带 marker，接收端把上半帧当整帧，下半部分花屏
		std::string au_data;   // 当前访问单元的 Annex-B 数据
		int64_t au_pts = 0;    // 当前访问单元的 pts
		bool au_has = false;   // au_data 里是否有待回调的 NAL
		auto flush_au = [&]() {
			if (!au_has)
			{
				return;
			}
			av_frame_t au;
			memset(&au, 0, sizeof(au));
			au.ct = codec_type_h264;
			au.mt = media_type_video;
			au.pts = au_pts;
			au.dts = au_pts;
			au.data = (uint8_t*)au_data.data();
			au.data_size = (uint32_t)au_data.size();
			invoke_rtp_frame(au);
			stats_.frames_received++;
			au_data.clear();
			au_has = false;
		};
		// 时间戳不同（MTAP 带 ts 偏移）就先把上一个访问单元刷出去，不会把两帧拼在一起
		auto push_nal = [&](const av_frame_t& f) {
			if (au_has && f.pts != au_pts)
			{
				flush_au();
			}
			if (!au_has)
			{
				au_pts = f.pts;
				au_has = true;
			}
			au_data.append((const char*)f.data, f.data_size);
		};
		
		for (auto pkt : pkts)
		{
			const uint8_t* payload = pkt->payload();
			int payload_size = (int)pkt->payload_size();

			if (payload_size < 2)
			{
				continue;
			}

			h264::nal_header_t fui = { 0 };
			h264::nal_header_set(&fui, payload[0]);
			if (fui.f != 0)
			{
				continue;
			}
			av_frame_t frame;
			memset(&frame, 0, sizeof(frame));
			frame.ct = codec_type_h264;
			frame.mt = media_type_video;
			if (fui.t <= 23)
			{
				if (fui.t == 7 || fui.t == 8 || fui.t == 5)
				{
					waiting_for_keyframe_ = false;
					waiting_logged_ = false;
				}
				std::string frame_data;
				frame_data.reserve(payload_size + 4);
				frame_data.append(3, 0);
				frame_data.append(1, 1);
				frame_data.append((const char*)payload, payload_size);
				

				frame.pts = pkt->header()->ts;
				frame.dts = frame.pts;
				frame.data = (uint8_t*)frame_data.data();
				frame.data_size = (int32_t)frame_data.size();



				if (!waiting_for_keyframe_)
				{
					push_nal(frame);
				}
				else
				{
					stats_.frames_droped++;
					// 等关键帧期间的静默丢弃只记一次，避免排障时误判为"没有任何帧到达"
					if (!waiting_logged_)
					{
						waiting_logged_ = true;
						if (log_)
						{
							log_->warn("H264 frame suppressed while waiting for keyframe")->flush();
						}
					}
				}
			}
			else if (fui.t == 24)  //STAP-A
			{
				frame.pts = pkt->header()->ts;
				frame.dts = frame.pts;
				const uint8_t* buf = payload + 1; //skip fui
				int size = payload_size - 1;


				while (size >= 2)
				{
					uint16_t nal_size = 0;
					nal_size = (buf[0] << 8) | buf[1];
					const uint8_t* nal_data = buf + 2;
					if (nal_size > size - 2)
						break;

					h264::nal_header_t nalh = { 0 };
					h264::nal_header_set(&nalh,*nal_data);
					if (nalh.t == 7 || nalh.t == 8 || nalh.t == 5)
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
						push_nal(frame);
					}
					else
					{
						stats_.frames_droped++;
						// 等关键帧期间的静默丢弃只记一次，避免排障时误判为"没有任何帧到达"
						if (!waiting_logged_)
						{
							waiting_logged_ = true;
							if (log_)
							{
								log_->warn("H264 frame suppressed while waiting for keyframe")->flush();
							}
						}
					}

					buf += (nal_size + 2);
					size -= (nal_size + 2);


				}
				
			}
			else if (fui.t == 25)  //STAP-B
			{
				frame.pts = pkt->header()->ts;
				frame.dts = frame.pts;
				uint16_t don = (payload[1] << 8) | payload[2];
				const uint8_t* buf = payload + 3; // skip fui bite and DON
				int size = payload_size - 3;


				while (size >= 2)
				{
					uint16_t nal_size = 0;
					nal_size = (buf[0] << 8) | buf[1];
					const uint8_t* nal_data = buf + 2;
					if (nal_size > size - 2)
						break;

					h264::nal_header_t nalh = { 0 };
					h264::nal_header_set(&nalh, *nal_data);
					if (nalh.t == 7 || nalh.t == 8 || nalh.t == 5)
					{
						waiting_for_keyframe_ = false;
						waiting_logged_ = false;
					}

					std::string frame_data;
					frame_data.reserve(nal_size+4);
					frame_data.append(3, 0);
					frame_data.append(1, 1);
					frame_data.append((const char*)nal_data, nal_size);
					if (!waiting_for_keyframe_)
					{
						frame.data = (uint8_t*)frame_data.data();
						frame.data_size = (uint32_t)frame_data.size();
						push_nal(frame);
					}
					else
					{
						stats_.frames_droped++;
						// 等关键帧期间的静默丢弃只记一次，避免排障时误判为"没有任何帧到达"
						if (!waiting_logged_)
						{
							waiting_logged_ = true;
							if (log_)
							{
								log_->warn("H264 frame suppressed while waiting for keyframe")->flush();
							}
						}
					}
					buf += (nal_size + 2);
					size -= (nal_size + 2);
				}
				
			}
			else if (fui.t == 26)  //MTAP-A
			{
				int64_t ts = pkt->header()->ts;
				uint16_t donb = (payload[1] << 8) | payload[2];
				const uint8_t* buf = payload + 3; // skip fui bite and DON
				int size = payload_size - 3;
				while (size >= 5)
				{
					uint16_t nal_size = 0;
					nal_size = (buf[0] << 8) | buf[1];
					uint8_t dond = buf[2];
					int32_t ts_offset = (buf[3] << 8) | buf[4];
					const uint8_t* nal_data = buf + 5;
					if (nal_size > size - 5)
						break;

					h264::nal_header_t nalh = { 0 };
					h264::nal_header_set(&nalh, *nal_data);
					if (nalh.t == 7 || nalh.t == 8 || nalh.t == 5)
					{
						waiting_for_keyframe_ = false;
						waiting_logged_ = false;
					}

					std::string frame_data;
					frame_data.reserve(nal_size + 4);
					frame_data.append(3, 0);
					frame_data.append(1, 1);
					frame_data.append((const char*)nal_data, nal_size);

					frame.data = (uint8_t*)frame_data.data();
					frame.data_size = (uint32_t)frame_data.size();

					frame.pts = ts + ts_offset;
					frame.dts = frame.pts;

					

					if (!waiting_for_keyframe_)
					{
						push_nal(frame);
					}
					else
					{
						stats_.frames_droped++;
					}
					buf += (nal_size + 5);
					size -= (nal_size + 5);
				}
			}
			else if (fui.t == 27) //MTAP-B
			{
				int64_t ts = pkt->header()->ts;
				uint16_t donb = (payload[1] << 8) | payload[2];
				const uint8_t* buf = payload + 3; // skip fui bite and DON
				int size = payload_size - 3;
				while (size >= 6)
				{
					uint16_t nal_size = 0;
					nal_size = (buf[0] << 8) | buf[1];
					uint8_t dond = buf[2];
					int32_t ts_offset = (buf[3] << 16) | (buf[4] << 8) | buf[5];
					const uint8_t* nal_data = buf + 6;
					if (nal_size > size - 6)
						break;

					h264::nal_header_t nalh = { 0 };
					h264::nal_header_set(&nalh, *nal_data);
					if (nalh.t == 7 || nalh.t == 8 || nalh.t == 5)
					{
						waiting_for_keyframe_ = false;
						waiting_logged_ = false;
					}

					std::string frame_data;
					frame_data.reserve(nal_size + 4);
					frame_data.append(3, 0);
					frame_data.append(1, 1);
					frame_data.append((const char*)nal_data, nal_size);

					frame.data = (uint8_t*)frame_data.data();
					frame.data_size = (uint32_t)frame_data.size();

					frame.pts = ts + ts_offset;
					frame.dts = frame.pts;

					
					if (!waiting_for_keyframe_)
					{
						push_nal(frame);
					}
					else
					{
						stats_.frames_droped++;
					}
					buf += (nal_size + 6);
					size -= (nal_size + 6);
				}
			}
			else if (fui.t == 28 || fui.t == 29)  //FU-A
			{
				frame.pts = pkt->header()->ts;
				frame.dts = frame.pts;


				h264::fu_header_t fuh = { 0 };
				h264::fu_header_set(&fuh, payload[1]);

				if (fuh.s == 1)
				{
					first_pkt = pkt;

					fu_frame_data = "";
					fu_frame_data.clear();
					fu_frame_data.reserve(MAX_RTP_PAYLOAD_SIZE* pkts.size() + 4);
					fu_frame_data.append(3, 0);
					fu_frame_data.append(1, 1);

					h264::nal_header_t nalh = { 0 };
					nalh.f = fui.f;
					nalh.nri = fui.nri;
					nalh.t = fuh.t;
					uint8_t nalhv = h264::nal_header_get(&nalh);
					fu_frame_data.append(1, nalhv);
				}
				if (!first_pkt)
				{
					// FU-A 分片首包缺失（首片丢失或未收齐即被超时丢弃），整个分片 NAL 无法还原；
					// 只记一次，避免刷屏
					if (!fu_start_missing_logged_)
					{
						fu_start_missing_logged_ = true;
						if (log_)
						{
							log_->warn("H264 FU-A start packet missing, fragmented nal dropped")->flush();
						}
					}
					continue;
				}

				


				int skip_nal_data = 2;
				if (fui.t == 29) //FU-B
				{
					skip_nal_data += 2;  //skip don;
				}

				const uint8_t* buf = payload + skip_nal_data; //skip fui and fu header (and don for FU-B)
				size_t size = payload_size - skip_nal_data;
				fu_frame_data.append((const char*)buf, size);


				if (fuh.e == 1) //end
				{
					if (fu_frame_data.size() > 0)
					{
						av_frame_t frame;
						memset(&frame, 0, sizeof(frame));
						frame.ct = codec_type_h264;
						frame.mt = media_type_video;
						frame.pts = first_pkt->header()->ts;
						frame.dts = frame.pts;
						frame.data = (uint8_t*)fu_frame_data.data();
						frame.data_size = (uint32_t)fu_frame_data.size();

						h264::nal_header_t nalh = { 0 };
						h264::nal_header_set(&nalh, frame.data[4]);
						if (nalh.t == 5 || nalh.t == 7 || nalh.t == 8)
						{
							waiting_for_keyframe_ = false;
							waiting_logged_ = false;
						}

						if (!waiting_for_keyframe_)
						{
							push_nal(frame);
						}
						else
						{
							stats_.frames_droped++;
							// 等关键帧期间的静默丢弃只记一次，避免排障时误判为"没有任何帧到达"
							if (!waiting_logged_)
							{
								waiting_logged_ = true;
								if (log_)
								{
									log_->warn("H264 frame suppressed while waiting for keyframe")->flush();
								}
							}
						}
					}
				}
			}
			else
			{
				flush_au();
				return false;
			}
		}

		flush_au();
		return true;
	}


}
