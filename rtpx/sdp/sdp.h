/**
 * @file sdp.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#pragma once

#include "sdp_media.h"

namespace rtpx {

	class sdp
	{
	public:
		std::string to_string()const;
		bool parse(const std::string& s);

		bool has_attribute(const std::string& key)const;
		std::string get_attribute(const std::string& key,const std::string& def)const;
	public:
		std::string v = "0";
		std::string o = "- 0 0 IN IP4 127.0.0.1";
		std::string s = "-";
		std::string i;
		std::string u;
		std::string e;
		std::string p;
		std::vector<std::string> b;
		std::string z;
		std::string k;

		std::string t= "0 0";
		std::string r;

		bool has_address = false;
		std::string network_type = "IN";
		std::string address_type = "IP4";
		std::string address = "0.0.0.0";
		
		bool bundle = true;

		std::vector<sdp_media> medias;
		std::vector<sdp_pair> attrs;
		std::vector<candidate> candidates;
	};


}

