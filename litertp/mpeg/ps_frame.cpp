#include "ps_frame.h"

namespace litertp {
	namespace mpeg {

		ps_frame::ps_frame()
		{
		}

		ps_frame::~ps_frame()
		{
		}
		/*
		bool ps_frame::deserialize2(const uint8_t* buffer, size_t size)
		{
			int pos = 0;
			reset();

			if (size < 4) {
				return false;
			}

			uint8_t start_code = buffer[3];
			pos += 4;
			if (start_code != PS_STREAM_ID_PACK_HEADER) {
				return false;
			}
			int c=psh.deserialize(buffer+pos, size-pos);
			if (c <= 0)
				return false;

			pos += c;

			c = deserialize_blocks(buffer + pos, size - pos);
			if (c <= 0)
				return false;
			pos += c;
			return true;
		}
		*/
		bool ps_frame::deserialize(const uint8_t* buffer, size_t size)
		{
			size_t pos = 0;
			reset();
			while (pos < size)
			{
				if (pos + 3 > size-1)
					break;

				uint8_t start_code = buffer[pos+3];
				if (start_code == PS_STREAM_ID_PACK_HEADER)
				{
					pos += 4;
					int c = psh.deserialize(buffer + pos, size - pos);
					if (c <= 0)
					{
						return false;
					}
					pos += c;
				}
				else
				{
					ps_block block;
					int c = block.deserialize(buffer + pos, size - pos);
					if (c <= 0)
					{
						return false;
					}
					blocks.push_back(block);
					pos += c;
				}
			}
			return true;
		}

		std::string ps_frame::serialize()
		{
			std::string buffer;
			buffer.append(2, (const char)0);
			buffer.append(1, (const char)1);
			buffer.append(1, (const char)PS_STREAM_ID_PACK_HEADER);
			buffer.append(psh.serialize());

			for (auto itr = blocks.begin(); itr != blocks.end(); itr++)
			{
				buffer.append(itr->serialize());
			}

			return buffer;
		}

		void ps_frame::reset()
		{
			psh.reset();
			blocks.clear();
		}

		int ps_frame::deserialize_blocks(const uint8_t* buffer, size_t size)
		{
			int pos = 0;
			if (size < 6)
				return 0;

			while (pos < (int)size)
			{
				ps_block block;
				int c = block.deserialize(buffer+pos, size-pos);
				if (c > 0)
				{
					blocks.push_back(block);
					pos += c;
				}
				else
				{
					break;
				}
			}
			return pos;
		}

		bool ps_frame::has_header(uint8_t start_code)
		{
			for (auto itr = blocks.begin(); itr != blocks.end(); itr++)
			{
				if (itr->start_code == start_code)
					return true;
			}
			return false;
		}
	}
}

