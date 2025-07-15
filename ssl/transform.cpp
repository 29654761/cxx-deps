#include "transform.h"
#include <sstream>
#include <iomanip>

namespace ssl
{

	transform::transform()
	{
	}

	transform::~transform()
	{
	}

	void transform::hex_string(const std::string& hex, std::string& out)
	{
		std::stringstream ss;
		for (int i = 0; i < hex.size(); i++) {
			ss << std::hex << std::nouppercase << std::setfill('0') << std::setw(2) << (int)(unsigned char)hex[i];
		}
		out = ss.str();
	}
}