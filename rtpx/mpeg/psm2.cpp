#include "psm2.h"
#include <sys2/security/crc32.h>

namespace rtpx {
	namespace mpeg {
		psm2::psm2()
		{
			reset();
		}

		psm2::~psm2()
		{
		}

		bool psm2::deserialize(const uint8_t* buffer, size_t size)
		{
			reset();
			int pos = 0;

			if (size < 4) {
				return false;
			}

			current_next_indicator = (buffer[pos] & 0x80) >> 7;
			program_stream_map_version= buffer[pos] & 0x1f;
			pos += 2;// skip reserved & marker_bit

			uint16_t program_stream_info_length = buffer[pos] << 8 | buffer[pos + 1];
			pos += 2;

			if (pos + program_stream_info_length > size) {
				return false;
			}

			program_stream_info.append((const char*)buffer + pos, program_stream_info_length);
			pos += program_stream_info_length;

			if (pos + 2 > size) {
				return false;
			}
			uint16_t elementary_stream_map_length= buffer[pos] << 8 | buffer[pos + 1];
			pos += 2;

			for (uint16_t i = 0; i < elementary_stream_map_length/4; i++)
			{
				if (pos + 4 > size)
					return false;

				elementary_stream_map_t esm = {};
				esm.stream_type = buffer[pos++];
				esm.elementary_stream_id = buffer[pos++];
				uint16_t len= buffer[pos] << 8 | buffer[pos + 1];
				pos += 2;

				//if (pos + len > size)
				//	return -1;

				//esm.elementary_stream_info.append((const char*)buffer + pos, len);
				//pos += len;



			}

			if (pos + 4 > size)
				return false;
			uint32_t crc32 = (buffer[pos] << 24) | (buffer[pos + 1] << 16) | (buffer[pos + 2] << 8) | (buffer[pos + 3]);
			pos += 4;

			return true;
		}

		std::string psm2::serialize()
		{
			uint16_t program_stream_info_length = (uint16_t)program_stream_info.size();
			uint16_t length = 4 + program_stream_info_length;

			uint16_t elementary_stream_map_length = 0;
			for (auto itr = stream_map.begin(); itr != stream_map.end(); itr++)
			{
				elementary_stream_map_length += 4;
				length += 4;
			}

			std::string buffer;
			buffer.reserve(length);
			uint8_t v = 0;
			v = current_next_indicator << 7 | 0x60 | program_stream_map_version;
			buffer.append(1, v);

			buffer.append(1, (char)0xFF);

			v = (program_stream_info_length & 0xFF00) >> 8;
			buffer.append(1, v);
			v= program_stream_info_length & 0x00FF;
			buffer.append(1, v);
			buffer.append(program_stream_info, 0, program_stream_info_length);
			
			v = (elementary_stream_map_length & 0xFF00) >> 8;
			buffer.append(1, v);
			v = elementary_stream_map_length & 0x00FF;
			buffer.append(1, v);
			
			for (auto itr = stream_map.begin(); itr != stream_map.end(); itr++)
			{
				std::string sm;
				sm.append(1, itr->stream_type);
				sm.append(1, itr->elementary_stream_id);

				uint16_t elementary_stream_info_length = (uint16_t)itr->elementary_stream_info.size();
				v = (elementary_stream_info_length & 0xFF00) >> 8;
				sm.append(1, v);
				v = elementary_stream_info_length & 0x00FF;
				sm.append(1, v);

				sm.append(itr->elementary_stream_info, 0, elementary_stream_info_length);

				buffer.append(sm);
				uint32_t crc32 = cyg_crc32((const unsigned char*)sm.data(), (int)sm.size());
				buffer.append(1, (uint8_t)((crc32 & 0xFF000000) >> 24));
				buffer.append(1, (uint8_t)((crc32 & 0x00FF0000) >> 16));
				buffer.append(1, (uint8_t)((crc32 & 0x0000FF00) >> 8));
				buffer.append(1, (uint8_t)(crc32 & 0x000000FF));


			}
			
			return buffer;
		}

		void psm2::reset()
		{
			current_next_indicator = 0;
			program_stream_map_version = 0;
			program_stream_info = "";
			stream_map.clear();
		}
	}
}

