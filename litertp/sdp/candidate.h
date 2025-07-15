/**
 * @file candidate.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#pragma once

#include <string>

namespace litertp
{
	class candidate
	{
	public:
		candidate();
		~candidate();

		bool parse(const std::string& s);
		std::string to_string() const;

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
