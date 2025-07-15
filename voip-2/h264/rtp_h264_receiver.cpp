#include "rtp_h264_receiver.h"
#include "sn.hpp"
#include "time.h"
#include "h264.h"

rtp_h264_recevier::rtp_h264_recevier()
{
}

rtp_h264_recevier::~rtp_h264_recevier()
{
}

bool rtp_h264_recevier::insert_packet(const RTP_DataFrame& pkt)
{
	auto pkt_saved = std::make_shared<RTP_DataFrame>();
	pkt_saved->Copy(pkt);

	std::lock_guard<std::recursive_mutex> lk(mutex_);

	uint16_t pkt_seq = pkt_saved->GetSequenceNumber();
	if (sn::ahead_of<uint16_t>(begin_seq_, pkt_seq))
	{
		reset_ = true;
	}

	if (reset_)
	{
		begin_seq_ = pkt_seq;
		end_seq_ = pkt_seq;
		frame_begin_ts_ = std::chrono::high_resolution_clock::now();

		for (int i = 0; i < PACKET_BUFFER_SIZE; i++)
		{
			recv_packs_[i].reset();
		}

		reset_ = false;
	}





	if (first_sec_ == 0)
	{
		first_sec_ = litertp::time_util::cur_time();
		first_ts_ = pkt_saved->GetTimestamp();
	}

	timestamp_ = pkt_saved->GetTimestamp();

	int idx = pkt_seq % PACKET_BUFFER_SIZE;

	
	recv_packs_[idx] = pkt_saved;


	if (sn::ahead_of<uint16_t>(end_seq_, pkt_seq))
	{
		//the lost seq is received.
	}
	else
	{
		auto n = sn::forward_diff<uint16_t>(end_seq_, pkt_seq);
		if (n > 1)
		{
			// loss packet
		}
		end_seq_ = pkt_seq;
	}


	// packet buffer is empty or full
	if (sn::ahead_of<uint16_t, PACKET_BUFFER_SIZE>(begin_seq_, end_seq_))
	{
		begin_seq_ = end_seq_;
	}



	std::vector<std::shared_ptr<RTP_DataFrame>> frames;
	while (find_a_frame(frames))
	{
		// A completed nal
		combin_frame(frames);
	}

	check_for_drop();


	return true;
}

bool rtp_h264_recevier::find_a_frame(std::vector<std::shared_ptr<RTP_DataFrame>>& pkts)
{
	pkts.clear();
	if (begin_seq_ < 0 || end_seq_ < 0 || sn::ahead_of<uint16_t>(begin_seq_, end_seq_))
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

		
		if (pkt->GetMarker())
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

void rtp_h264_recevier::check_for_drop()
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

		nal_header_t fui = { 0 };
		h264::nal_header_set(&fui, pkt->GetPayloadPtr()[0]);

		if (fui.nri > 0)  // nal ref idc
		{
			waiting_for_keyframe_ = true;
		}

		recv_packs_[idx].reset();

		if (pkt->GetMarker())
		{
			//new start
			frame_begin_ts_ = std::chrono::high_resolution_clock::now();
			break;
		}
	}

	begin_seq_ = i;
}



bool rtp_h264_recevier::combin_frame(const std::vector<std::shared_ptr<RTP_DataFrame>>& pkts)
{
	if (pkts.size() <= 0) {
		return false;
	}

	std::shared_ptr<RTP_DataFrame> first_pkt;
	std::string fu_frame_data;

	for (auto pkt : pkts)
	{
		const uint8_t* payload = pkt->GetPayloadPtr();
		int payload_size = pkt->GetPayloadSize();

		if (payload_size < 2)
		{
			continue;
		}

		nal_header_t fui = { 0 };
		h264::nal_header_set(&fui, payload[0]);
		if (fui.f != 0)
		{
			continue;
		}

		if (fui.t <= 23)
		{
			int64_t ts = pkt->GetTimestamp();

			std::string frame_data;
			frame_data.reserve(payload_size + 4);
			frame_data.append(3, 0);
			frame_data.append(1, 1);
			frame_data.append((const char*)payload, payload_size);

			if (fui.t == 5 || fui.t == 7 || fui.t == 8)
			{
				waiting_for_keyframe_ = false;
			}

			if (!waiting_for_keyframe_)
			{
				frame_event.invoke((const uint8_t*)frame_data.data(), frame_data.size(), ts);
			}
		}
		else if (fui.t == 24)  //STAP-A
		{
			int64_t ts = pkt->GetTimestamp();
			const uint8_t* buf = payload + 1; //skip fui
			int size = payload_size - 1;

			std::string frame_data;
			frame_data.reserve(payload_size + 8);

			while (size >= 2)
			{
				uint16_t nal_size = 0;
				nal_size = (buf[0] << 8) | buf[1];

				//nal_size = std::min((int)nal_size, size - 2);
				if (nal_size > size - 2)
					break;

				frame_data.append(3, 0);
				frame_data.append(1, 1);
				frame_data.append((const char*)buf + 2, nal_size);

				buf += (nal_size + 2);
				size -= (nal_size + 2);
			}

			if (frame_data.size() >= 4)
			{
				nal_header_t nalh = { 0 };
				h264::nal_header_set(&nalh, frame_data[4]);

				if (nalh.t == 5 || nalh.t == 7 || nalh.t == 8)
				{
					waiting_for_keyframe_ = false;
				}

				if (!waiting_for_keyframe_)
				{
					frame_event.invoke((uint8_t*)frame_data.data(), frame_data.size(), ts);
				}
			}
		}
		else if (fui.t == 25)  //STAP-B
		{
			int64_t ts = pkt->GetTimestamp();
			uint16_t don = (payload[1] << 8) | payload[2];
			const uint8_t* buf = payload + 3; // Skip fui byte and DON
			int size = payload_size - 3;


			std::string frame_data;
			frame_data.reserve(payload_size + 8);


			while (size >= 2)
			{
				uint16_t nal_size = 0;
				nal_size = (buf[0] << 8) | buf[1];

				//nal_size = std::min((int)nal_size, size - 2);
				if (nal_size > size - 2)
					break;

				frame_data.append(3, 0);
				frame_data.append(1, 1);
				frame_data.append((const char*)buf + 2, nal_size);


				buf += (nal_size + 2);
				size -= (nal_size + 2);
			}

			if (frame_data.size() >= 4)
			{
				nal_header_t nalh = { 0 };
				h264::nal_header_set(&nalh, frame_data[4]);

				if (nalh.t == 5 || nalh.t == 7 || nalh.t == 8)
				{
					waiting_for_keyframe_ = false;
				}

				if (!waiting_for_keyframe_)
				{
					frame_event.invoke((uint8_t*)frame_data.data(), frame_data.size(), ts);
				}
			}
		}
		else if (fui.t == 26)  //MTAP-A
		{
			int64_t ts = pkt->GetTimestamp();
			uint16_t donb = (payload[1] << 8) | payload[2];
			const uint8_t* buf = payload + 3; // skip fui bite and DON
			int size = payload_size - 3;
			while (size >= 5)
			{
				uint16_t nal_size = 0;
				nal_size = (buf[0] << 8) | buf[1];
				uint8_t dond = buf[2];
				int32_t ts_offset = (buf[3] << 8) | buf[4];

				//nal_size = std::min((int)nal_size, size - 5);
				if (nal_size > size - 5)
					break;

				std::string frame_data;
				frame_data.reserve(nal_size + 4);
				frame_data.append(3, 0);
				frame_data.append(1, 1);
				frame_data.append((const char*)buf + 5, nal_size);


				int64_t ts2 = ts + ts_offset;

				nal_header_t nalh = { 0 };
				h264::nal_header_set(&nalh, frame_data[4]);
				if (nalh.t == 5 || nalh.t == 7 || nalh.t == 8)
				{
					waiting_for_keyframe_ = false;
				}

				if (!waiting_for_keyframe_)
				{
					frame_event.invoke((const uint8_t*)frame_data.data(), frame_data.size(), ts2);
				}
				buf += (nal_size + 5);
				size -= (nal_size + 5);
			}
		}
		else if (fui.t == 27) //MTAP-B
		{
			int64_t ts = pkt->GetTimestamp();
			uint16_t donb = (payload[1] << 8) | payload[2];
			const uint8_t* buf = payload + 3; // skip fui bite and DON
			int size = payload_size - 3;
			while (size >= 6)
			{
				uint16_t nal_size = 0;
				nal_size = (buf[0] << 8) | buf[1];
				uint8_t dond = buf[2];
				int32_t ts_offset = (buf[3] << 16) | (buf[4] << 8) | buf[5];

				//nal_size = std::min((int)nal_size, size - 6);
				if (nal_size > size - 6)
					break;

				std::string frame_data;
				frame_data.reserve(nal_size + 4);
				frame_data.append(3, 0);
				frame_data.append(1, 1);
				frame_data.append((const char*)buf + 6, nal_size);

				int64_t ts2 = ts + ts_offset;

				nal_header_t nalh = { 0 };
				h264::nal_header_set(&nalh, frame_data[4]);
				if (nalh.t == 5 || nalh.t == 7 || nalh.t == 8)
				{
					waiting_for_keyframe_ = false;
				}

				if (!waiting_for_keyframe_)
				{
					frame_event.invoke((uint8_t*)frame_data.data(), frame_data.size(), ts2);
				}
				buf += (nal_size + 6);
				size -= (nal_size + 6);
			}
		}
		else if (fui.t == 28 || fui.t == 29)  //FU-A
		{
			int64_t ts = pkt->GetTimestamp();

			fu_header_t fuh = { 0 };
			h264::fu_header_set(&fuh, payload[1]);

			if (fuh.s == 1)
			{
				first_pkt = pkt;

				fu_frame_data = "";
				fu_frame_data.clear();
				fu_frame_data.reserve(max_rtp_packet_size_* pkts.size() + 4);
				fu_frame_data.append(3, 0);
				fu_frame_data.append(1, 1);

				nal_header_t nalh = { 0 };
				nalh.f = fui.f;
				nalh.nri = fui.nri;
				nalh.t = fuh.t;
				uint8_t nalhv = h264::nal_header_get(&nalh);
				fu_frame_data.append(1, nalhv);
			}
			if (!first_pkt)
			{
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

		}
		else
		{
			return false;
		}
	}


	if (fu_frame_data.size() > 0)
	{
		int64_t ts = first_pkt->GetTimestamp();

		nal_header_t nalh = { 0 };
		h264::nal_header_set(&nalh, fu_frame_data[4]);
		if (nalh.t == 5 || nalh.t == 7 || nalh.t == 8)
		{
			waiting_for_keyframe_ = false;
		}

		if (!waiting_for_keyframe_)
		{
			frame_event.invoke((uint8_t*)fu_frame_data.data(), fu_frame_data.size(), ts);
		}
	}

	return true;
}

bool rtp_h264_recevier::is_timeout()
{
	auto now = std::chrono::high_resolution_clock::now();
	auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(now - frame_begin_ts_);
	if (msec.count() >= delay_) {
		return true;
	}
	return false;


}



