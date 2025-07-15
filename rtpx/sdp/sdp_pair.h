/**
 * @file sdp_pair.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */



#pragma once

#include <string>

namespace rtpx {

	class sdp_pair
	{
	public:
		sdp_pair(const std::string& separator);
		sdp_pair(const std::string& key, const std::string& val,const std::string& separator);
		~sdp_pair();


		std::string to_string()const;
		bool parse(const std::string& s);

	public:
		std::string key;
		std::string val;
		std::string separator;
	};

}
