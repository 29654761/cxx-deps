
/**
 * @file candidate.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#include "candidate.h"
#include <sys2/string_util.h>

namespace rtpx
{
	candidate::candidate()
	{

	}

	candidate::~candidate()
	{

	}

	uint32_t candidate::make_priority(uint32_t type, uint32_t local, uint32_t component)
	{
		return (1 << 24) * type + (1 << 8) * local + (256 - component);
	}

	bool candidate::parse(const std::string& s)
	{
		std::vector<std::string> ss = sys::string_util::split(s, " ");
		if (ss.size() < 7)
		{
			return false;
		}
		char* endptr = nullptr;
		foundation = (uint32_t)strtoul(ss[0].c_str(),&endptr, 10);
		component = (uint8_t)strtoul(ss[1].c_str(),&endptr, 10);
		protocol = ss[2];
		priority = strtoul(ss[3].c_str(),&endptr,10);
		address = ss[4];
		port = (uint16_t)strtoul(ss[5].c_str(),&endptr,10);
		type = ss[6];
		return true;
	}

	std::string candidate::to_string() const
	{
		std::string s;
		s += std::to_string(foundation);
		s += " ";
		s += std::to_string(component);
		s += " ";
		s += protocol;
		s += " ";
		s += std::to_string(priority);
		s += " ";
		s += address;
		s += " ";
		s += std::to_string(port);
		s += " ";
		s += "typ " + type;
		s += " ";
		s += "ufrag " + ufrag;
		//s += "generation 0";
		//s += " ";
		//s += "network-id 1";
		//s += " ";
		//s += "network-cost 10";
		return s;
	}

}