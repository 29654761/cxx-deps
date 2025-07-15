#include "psh2.h"
#include <string.h>

namespace litertp {
	namespace mpeg {

		psh2::psh2()
		{
			reset();
		}

		psh2::~psh2()
		{
		}

		int psh2::deserialize(const uint8_t* buffer, size_t size)
		{
			reset();
			if (size < 8) {
				return 0;
			}
			int pos = 0;
			
			flag = (buffer[pos] & 0xC0) >> 6;

			timestamp |= ((uint64_t)buffer[pos] & 0x38) << 27;
			timestamp |= ((uint64_t)buffer[pos] & 0x3) << 28;
			timestamp |= ((uint64_t)buffer[pos + 1] << 20);
			timestamp |= ((uint64_t)buffer[pos + 2] & 0xf8) << 12;
			timestamp |= ((uint64_t)buffer[pos + 2] & 0x03) << 13;
			timestamp |= ((uint64_t)buffer[pos + 3] << 5);
			timestamp |= (((uint64_t)buffer[pos + 4] & 0xf8) >> 3);

			timestamp_ex |= (buffer[pos + 4] & 0x03) << 7;
			timestamp_ex |= (buffer[pos + 4] & 0xfe) >> 1;
			pos += 6;

			program_mux_rate |= ((uint32_t)buffer[pos] << 14);
			program_mux_rate |= ((uint32_t)buffer[pos + 1] << 6);
			program_mux_rate |= (((uint32_t)buffer[pos + 2] &0xFC) >> 2);
			pos += 3;


			uint8_t reserved = (buffer[pos] & 0xF8) >> 3;
			pack_stuffing_length = (buffer[pos] & 0x07);
			pos++;

			if (pack_stuffing_length + 12 > size) {
				return 0;
			}

			pos += pack_stuffing_length;
			return pos;
		}

		std::string psh2::serialize()
		{
			std::string buffer;
			buffer.reserve(20);
			uint8_t tmp[6] = {};
			tmp[0] = (uint8_t)((((uint64_t)flag) << 6) | ((timestamp & 0x1c0000000) >> 27) | 1);
			tmp[0] |= (timestamp & 0x30000000) >> 28;
			tmp[1] = (uint8_t)((timestamp & 0xff00000) >> 20);
			tmp[2] = (uint8_t)((timestamp & 0xf8000) >> 12);
			tmp[2] |= 4;
			tmp[2] |= (timestamp & 0x6000) >> 13;
			tmp[3] = (uint8_t)((timestamp & 0x1fe0) >> 5);
			tmp[4] = (uint8_t)((timestamp & 0x1f) << 3);
			tmp[4] |= 4;


			tmp[4] |= (timestamp_ex & 0x180) >> 7;
			tmp[5] = (timestamp_ex & 0x7f) << 1;
			tmp[5] |= 1;
			buffer.append((const char*)tmp, 6);
			memset(tmp, 0, 6);
			tmp[0] = (program_mux_rate & 0x3FC000) >> 14;
			tmp[1]= (program_mux_rate & 0x3FC0) >> 6;
			tmp[2] = (program_mux_rate & 0x3F) << 2;
			tmp[2] |= 3;
			buffer.append((const char*)tmp, 3);
			memset(tmp, 0, 6);
			uint8_t reserved = 0;
			tmp[0] = (reserved << 3) | (pack_stuffing_length);
			buffer.append((const char*)tmp, 1);
			buffer.append((size_t)pack_stuffing_length,(char)0xFF);
			return buffer;
		}

		void psh2::reset()
		{
			flag = 1;
			timestamp = 0;
			timestamp_ex = 0;
			program_mux_rate = 0;
			pack_stuffing_length = 0;
		}
	}
}
