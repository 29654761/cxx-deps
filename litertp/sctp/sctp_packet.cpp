/**
 * @file sctp_packet.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#include "sctp_packet.h"
#include <sys2/security/crc32c.h>

namespace litertp
{

	sctp_packet::sctp_packet()
	{
		
	}

	sctp_packet::~sctp_packet()
	{
	}

	bool sctp_packet::deserialize(const uint8_t* data, size_t size)
	{
		size_t pos = 0;
		if (!header.deserialize(data, size))
			return false;

		pos += 12;

		while (pos < size)
		{
			sctp_chunk chunk;
			size_t len=chunk.deserialize(data + pos, size - pos);
			if (len == 0)
				return false;

			chunks.push_back(chunk);
			pos += len;
		}

		return true;
	}

	std::vector<uint8_t> sctp_packet::serialize() const
	{
		std::vector<uint8_t> buf;
		buf.reserve(2048);
		auto hdr = header.serialize();
		buf.insert(buf.end(), hdr.begin(), hdr.end());

		for (auto itr = chunks.begin(); itr != chunks.end(); itr++)
		{
			auto chk = itr->serialize();
			buf.insert(buf.end(), chk.begin(), chk.end());
		}
		return buf;
	}

	uint32_t sctp_packet::check_crc32(const uint8_t* data, size_t size)
	{
		if (size < 12)
			return 0;

		uint32_t ccc=crc_c[1];
		uint32_t crc32 = ~(uint32_t)0;
		for (size_t i = 0; i < 8; i++)
			CRC32C(crc32, data[i]);

		for (size_t i = 8; i < 12; i++)
			CRC32C(crc32, 0);

		for(size_t i=12;i< size;i++)
			CRC32C(crc32, data[i]);

		uint32_t result= ~crc32;
		uint8_t byte0, byte1, byte2, byte3;
		byte0 = result & 0xff;
		byte1 = (result >> 8) & 0xff;
		byte2 = (result >> 16) & 0xff;
		byte3 = (result >> 24) & 0xff;

		crc32 = ((byte0 << 24) |
			(byte1 << 16) |
			(byte2 << 8) |
			byte3);

		return crc32;
	}

	bool sctp_packet::sign_crc32(uint8_t* data, size_t size)
	{
		if (size < 12)
			return false;

		uint32_t crc32 = check_crc32(data, size);
		if (crc32 == 0)
			return false;

		data[8] = static_cast<uint8_t>(crc32>>24);
		data[9] = static_cast<uint8_t>(crc32 >> 16);
		data[10] = static_cast<uint8_t>(crc32 >> 8);
		data[11] = static_cast<uint8_t>(crc32);
		return true;
	}

	void sctp_packet::add_chunk(sctp_chunk_type type, sctp_chunk_flags flags, const std::vector<uint8_t>& payload)
	{
		sctp_chunk chunk;
		chunk.type = type;
		chunk.flags = flags;
		chunk.payload = payload;
		chunks.push_back(chunk);
	}
}

