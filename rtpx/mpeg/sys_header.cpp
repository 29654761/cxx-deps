#include "sys_header.h"

namespace rtpx {
	namespace mpeg {
		sys_header::sys_header()
		{
			reset();
		}

		sys_header::~sys_header()
		{
		}

		bool sys_header::deserialize(const uint8_t* buffer, size_t size)
		{
			reset();
			if (size < 6) {
				return false;
			}
			int pos = 0;
			rate_bound |= (buffer[pos] & 0x7f) << 15;
			rate_bound |= (buffer[pos + 1]) << 7;
			rate_bound |= (buffer[pos + 2] & 0xfe)>>1;
			pos += 3;

			audio_bound |= (buffer[pos] & 0xfc) >> 2;
			fixed_flag = (buffer[pos] & 0x02) >> 1;
			csps_flag = (buffer[pos] & 0x01);
			pos++;

			system_audio_lock_flag = (buffer[pos] & 0x80) >> 7;
			system_video_lock_flag = (buffer[pos] & 0x40) >> 6;
			video_bound = (buffer[pos] & 0x1f);
			pos++;

			packet_rate_restriction_flag= (buffer[pos] & 0x80) >> 7;
			pos++;

			int pos1 = 0;
			while (pos1 < size-6)
			{
				if (pos + pos1 > size)
					return false;
				stream_t stream = {};
				stream.stream_id = buffer[pos + pos1];
				stream.flag = (buffer[pos + pos1 +1] & 0xc0) >> 6;
				stream.p_std_buffer_bound_scale= (buffer[pos + pos1 +1] & 0x20) >> 5;
				stream.p_std_buffer_size_bound |= (buffer[pos + pos1 + 1] & 0x1f) << 8;
				stream.p_std_buffer_size_bound |= (buffer[pos + pos1 +2]);
				streams.push_back(stream);
				pos1 += 3;
			}
			pos += pos1;
			return true;
		}

		std::string sys_header::serialize()
		{
			std::string buffer;

			uint16_t length = (uint16_t)(6 + streams.size() * 3);

			buffer.reserve(length);

			uint8_t v = 0;
			v |= 0x80;
			v |= (rate_bound & 0x3f8000) >> 15;
			buffer.append(1, v);

			v = (rate_bound & 0x7f80) >> 7;
			buffer.append(1, v);

			v = 0;
			v |= (rate_bound & 0x7f) << 1;
			v |= 1;
			buffer.append(1, v);

			v = 0;
			v |= audio_bound << 2;
			v |= fixed_flag << 1;
			v |= csps_flag;
			buffer.append(1, v);

			v = 0;
			v |= system_audio_lock_flag << 7;
			v |= system_video_lock_flag << 6;
			v |= 1 << 5;
			v |= video_bound;
			buffer.append(1, v);

			v = 0;
			v |= packet_rate_restriction_flag << 7;
			v |= 0x7F; //reserved_bits
			buffer.append(1, v);

			for (auto itr = streams.begin(); itr != streams.end(); itr++)
			{
				buffer.append(1,itr->stream_id);
				v = 0;
				v |= itr->flag << 6;
				v |= itr->p_std_buffer_bound_scale << 5;
				v |= (itr->p_std_buffer_size_bound & 0x1f00) >> 8;
				buffer.append(1, v);
				buffer.append(1, itr->p_std_buffer_size_bound & 0xff);
			}

			return buffer;
		}

		void sys_header::reset()
		{
			rate_bound = 0;
			audio_bound = 0;
			fixed_flag = 0;
			csps_flag = 0;
			system_audio_lock_flag = 0;
			system_video_lock_flag = 0;
			video_bound = 0;
			packet_rate_restriction_flag = 0;
			streams.clear();
		}
	}
}
