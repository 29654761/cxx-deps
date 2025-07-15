#pragma once
#include <vector>
#include "bfcp_attribute.h"

enum class bfcp_packet_enum_t:uint8_t
{
	unknown=0,
	floor_request=1,
	floor_release=2,
	floor_request_query=3,
	floor_request_status=4,
	user_query=5,
	user_status=6,
	floor_query=7,
	floor_status=8,
	chair_action=9,
	chair_action_ack=10,
	hello=11,
	hello_ack=12,
	error=13,
	floor_request_status_ack=14,
	floor_status_ack=15,
	goodbye=16,
	goodbye_ack=17,
};

class bfcp_packet
{
public:
	bfcp_packet();
	~bfcp_packet();

	bool deserialize(const uint8_t* data,int size);
	std::string serialize()const;
public:
	uint8_t ver : 3;
	uint8_t r : 1;
	uint8_t f : 1;
	uint8_t res : 3;
	bfcp_packet_enum_t primitive = bfcp_packet_enum_t::unknown;
	uint32_t conference_id = 0;
	uint16_t transaction_id = 0;
	uint16_t user_id = 0;
	uint16_t fragment_offset = 0;
	uint16_t fragment_length = 0;
	std::vector<bfcp_attribute> attributes;
};

