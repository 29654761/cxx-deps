
/**
 * @file sdp_pair.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#include "sdp_pair.h"

#include <sstream>
#include "sys2/string_util.h"


namespace litertp {

	sdp_pair::sdp_pair(const std::string& separator)
	{
		this->separator = separator;
	}

	sdp_pair::sdp_pair(const std::string& key, const std::string& val, const std::string& separator)
	{
		this->key = key;
		this->val = val;
		this->separator = separator;
	}

	sdp_pair::~sdp_pair()
	{

	}

	std::string sdp_pair::to_string()const
	{
		if (val.size() > 0) {
			return key + separator + val;
		}
		else {
			return key;
		}
	}

	bool sdp_pair::parse(const std::string& s)
	{
		std::vector<std::string> vec;
		sys::string_util::split(s, separator, vec, 2);
		if (vec.size() >= 1) {
			key = vec[0];
		}

		if (vec.size() >= 2) {
			val = vec[1];
		}
		return true;
	}

}
