/**
 * @file candidate.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#pragma once

#include <string>

namespace rtpx
{
#define CANDIDATE_TYPE_HOST 126   //host
#define CANDIDATE_TYPE_SRFLX 100  //server-reflexive
#define CANDIDATE_TYPE_PRFLX 110  //peer-reflexive
#define CANDIDATE_TYPE_RELAY 0    //0-50, recommend 0 ro 10

	class candidate
	{
	public:
		candidate();
		~candidate();

		bool parse(const std::string& s);
		std::string to_string() const;

		static uint32_t make_priority(uint32_t type, uint32_t local, uint32_t component);
	public:
		uint32_t foundation = 0;

		/// <summary>
		/// 1-rtp  2-rtcp
		/// </summary>
		uint8_t component = 1;

		std::string protocol = "udp";

		uint32_t priority = 2043278322;

		std::string address = "";

		uint16_t port = 0;

		std::string type = "host";

		std::string ufrag;
	};
}
