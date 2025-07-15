/**
 * @file sctp_packet.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#pragma once
#include "sctp_header.h"
#include "sctp_chunk.h"

namespace litertp
{
    /*
    +------------------------+  
    | SCTP Header            |  <- SCTP Common Header (12 bytes)
    +------------------------+
    | Chunk #1               |  <- DATA / HEARTBEAT / SACK
    +------------------------+
    | Chunk #2               |
    +------------------------+
    | Chunk #N               |
    +------------------------+
	*/
	class sctp_packet
	{
	public:
		sctp_packet();
		~sctp_packet();

		bool deserialize(const uint8_t* data, size_t size);
		std::vector<uint8_t> serialize() const;

		static uint32_t check_crc32(const uint8_t* data, size_t size);
		static bool sign_crc32(uint8_t* data, size_t size);

		void add_chunk(sctp_chunk_type type, sctp_chunk_flags flags, const std::vector<uint8_t>& payload);
	public:
		sctp_header header;
		std::vector<sctp_chunk> chunks;

	};


}

