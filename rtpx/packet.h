/**
 * @file packet.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#pragma once
#include <memory>
#include <vector>
#include <sys2/socket.h>

#include "rtp/rtp_packet.h"


namespace rtpx {

	class packet;
	typedef std::shared_ptr<packet> packet_ptr;

	class packet
	{
	public:
		packet();
		packet(uint8_t pt, uint32_t ssrc, uint16_t seq, uint32_t ts);
		~packet();

		size_t size()const;
		size_t payload_size()const;
		const uint8_t* payload()const;
		const rtp_header* header()const;
		rtp_header* mutable_header();
		
		bool serialize(std::vector<uint8_t>& buffer)const;
		bool parse(const uint8_t* buffer, size_t size);

		bool set_payload(const uint8_t* payload, size_t size);
		void clear_payload();

		packet_ptr clone()const;
	private:
		void init(uint8_t pt, uint32_t ssrc, uint16_t seq, uint32_t ts);

	private:
		rtp_packet* handle_ = nullptr;
	};
	
}

