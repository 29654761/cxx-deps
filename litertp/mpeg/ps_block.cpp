#include "ps_block.h"

namespace litertp {
	namespace mpeg {


		ps_block::ps_block()
		{
			reset();
		}

		ps_block::~ps_block()
		{
		}

		int ps_block::deserialize(const uint8_t* buffer, size_t size)
		{
			reset();
			if (size < 6)
				return 0;

			int pos = 0;
			start_code = buffer[3];
			pos += 4;

			uint16_t length = 0;
			length |= ((uint16_t)buffer[pos] << 8);
			length |= ((uint16_t)buffer[pos+1]);
			pos += 2;

			if (pos + length > size)
				return 0;

			block.append((const char*)buffer + pos, length);
			pos += length;
			return pos;
		}

		std::string ps_block::serialize()
		{
			std::string buffer;
			buffer.append(2, (const char)0);
			buffer.append(1, (const char)1);
			buffer.append(1, (const char)start_code);

			uint16_t length = (uint16_t)block.size();
			buffer.append(1, (length & 0xFF00) >> 8);
			buffer.append(1, length & 0x00FF);

			buffer.append(block);
			return buffer;
		}

		void ps_block::reset()
		{
			start_code = 0;
			block = "";
		}
	}
}

