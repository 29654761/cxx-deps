/**
 * @file sctp_header.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#include "sctp_header.h"

namespace litertp
{

	sctp_header::sctp_header()
	{
		
	}

	sctp_header::~sctp_header()
	{
	}

	bool sctp_header::deserialize(const uint8_t* data, size_t size)
	{
		if (size <= 12)
			return false;

		src_port = (data[0] << 8) | data[1];
		dst_port = (data[2] << 8) | data[3];
		verification_tag = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
		checksum = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
		return true;
	}

	std::vector<uint8_t> sctp_header::serialize() const
	{
		std::vector<uint8_t> buf;
		buf.reserve(12);
		buf.push_back(static_cast<uint8_t>(src_port >> 8));
		buf.push_back(static_cast<uint8_t>(src_port & 0xFF));
		buf.push_back(static_cast<uint8_t>(dst_port >> 8));
		buf.push_back(static_cast<uint8_t>(dst_port & 0xFF));
		buf.push_back(static_cast<uint8_t>(verification_tag >> 24));
		buf.push_back(static_cast<uint8_t>(verification_tag >> 16));
		buf.push_back(static_cast<uint8_t>(verification_tag >> 8));
		buf.push_back(static_cast<uint8_t>(verification_tag & 0xFF));
		buf.push_back(static_cast<uint8_t>(checksum >> 24));
		buf.push_back(static_cast<uint8_t>(checksum >> 16));
		buf.push_back(static_cast<uint8_t>(checksum >> 8));
		buf.push_back(static_cast<uint8_t>(checksum & 0xFF));
		return buf;
	}
}
