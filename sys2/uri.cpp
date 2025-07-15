#include "uri.h"
#include <sys2/string_util.h>
#include <sstream>
#include <algorithm>

namespace sys {

	uri::uri()
	{
	}

	uri::~uri()
	{
	}

	void uri::add(const std::string& name, const std::string& value)
	{
		args.push_back(std::make_pair(name, value));
	}

	void uri::remove(const std::string& name)
	{
		auto itr = std::find_if(args.begin(), args.end(), [&name](const std::pair<std::string, std::string>& kv) {
			return kv.first == name;
			});

		if (itr != args.end())
		{
			args.erase(itr);
		}
	}

	void uri::clear()
	{
		args.clear();
	}


	const std::string& uri::get(const std::string& name, const std::string& def_value)const
	{
		auto itr = std::find_if(args.begin(), args.end(), [&name](const std::pair<std::string, std::string>& kv) {
			return kv.first == name;
			});

		if (itr == args.end())
		{
			return def_value;
		}
		return itr->second;
	}

	void uri::get(const std::string& name, std::vector<std::string>& values)const
	{
		values.clear();
		for (auto itr = args.begin(); itr != args.end(); itr++)
		{
			if (itr->first == name)
			{
				values.push_back(itr->second);
			}
		}
	}

	int64_t uri::get_int64(const std::string& name, int64_t def_value)const
	{
		auto itr = std::find_if(args.begin(), args.end(), [&name](const std::pair<std::string, std::string>& kv) {
			return kv.first == name;
			});

		if (itr == args.end())
		{
			return def_value;
		}

		char* endptr = nullptr;
		return strtoll(itr->second.c_str(), &endptr, 0);
	}

	uint64_t uri::get_uint64(const std::string& name, uint64_t def_value)const
	{
		auto itr = std::find_if(args.begin(), args.end(), [&name](const std::pair<std::string, std::string>& kv) {
			return kv.first == name;
			});

		if (itr == args.end())
		{
			return def_value;
		}

		char* endptr = nullptr;
		return strtoull(itr->second.c_str(), &endptr, 0);
	}

	double uri::get_double(const std::string& name, double def_value)const
	{
		auto itr = std::find_if(args.begin(), args.end(), [&name](const std::pair<std::string, std::string>& kv) {
			return kv.first == name;
			});

		if (itr == args.end())
		{
			return def_value;
		}

		char* endptr = nullptr;
		return strtod(itr->second.c_str(), &endptr);
	}


	bool uri::parse(const std::string& s)
	{
		raw = s;
		reset();

		std::string path_query;
		auto vec = sys::string_util::split(s, "://", 2);
		if (vec.size() >= 2)
		{
			protocal = vec[0];
			path_query = vec[1];
		}
		else if(vec.size()>=1)
		{
			path_query = vec[0];
		}
		else
		{
			return true;
		}
		

		auto vec_hp_q = sys::string_util::split(path_query, "?", 2);
		if (vec_hp_q.size() > 1)
		{
			if (!parse_query_string(vec_hp_q[1]))
			{
				return false;
			}
		}

		if (vec_hp_q[0].size() > 0 && vec_hp_q[0][0] == '/')
		{
			path = vec_hp_q[0];
		}
		else
		{
			auto vec_host_path = sys::string_util::split(vec_hp_q[0], "/", 2);
			if (vec_host_path.size() >= 2)
			{
				path = vec_host_path[1];
			}

			auto vec_host_port = sys::string_util::split(vec_host_path[0], ":", 2);
			host = vec_host_port[0];
			if (vec_host_port.size() > 1)
			{
				char* endptr = nullptr;
				port = strtol(vec_host_port[1].c_str(), &endptr, 10);
				is_default_port = false;
			}
			else {
				port = 0;
				is_default_port = true;
			}
			
		}

		return true;
	}

	std::string uri::to_string()const
	{
		std::stringstream ss;
		ss << protocal << "://" << host;
		if (!is_default_port)
		{
			ss << ":" << port;
		}
		if (path.size() > 0)
		{
			if (path[0] != '/')
			{
				ss << "/";
			}
			ss << path;
		}
		if (args.size() > 0)
		{
			ss << "?" << to_query_string();
		}

		return ss.str();
	}

	std::string uri::to_path_string()const
	{
		std::string url = path;
		if (args.size() > 0)
		{
			url += "?";
			url += to_query_string();
		}
		return url;
	}

	bool uri::parse_query_string(const std::string& s)
	{
		clear();
		auto vec = sys::string_util::split(s, "&");
		for (auto itr = vec.begin(); itr != vec.end(); itr++)
		{
			auto vec2 = sys::string_util::split(*itr, "=", 2);
			if (vec2.size() >= 2)
			{
				add(vec2[0], url_decode(vec2[1]));
			}
		}

		return true;
	}

	std::string uri::to_query_string()const
	{
		std::stringstream ss;
		for (auto itr = args.begin(); itr != args.end(); itr++)
		{
			ss << itr->first << "=" << url_encode(itr->second) << "&";
		}

		if (args.size() > 0)
		{
			ss.seekp(-1, std::ios::end);
			ss.put(0);
		}

		return ss.str();
	}

	void uri::reset()
	{
		protocal = "";
		host = "";
		port = 0;
		is_default_port = false;
		path = "";
		args.clear();
	}

	bool uri::match_path(const std::string& path)const
	{
		size_t size = path.size();
		if (size == 0||size>this->path.size())
			return false;

		auto itr1 = this->path.rbegin();
		auto itr2 = path.rbegin();
		for (size_t i = 0; i < size; i++)
		{
			if (std::tolower(*itr1) != std::tolower(*itr2))
				return false;
			itr1++;
			itr2++;
		}
		return true;
	}

	std::string uri::url_encode(const std::string& str)
	{
		std::string str_temp = "";
		size_t length = str.length();
		for (size_t i = 0; i < length; i++)
		{
			if (isalnum((unsigned char)str[i]) ||
				(str[i] == '-') ||
				(str[i] == '_') ||
				(str[i] == '.') ||
				(str[i] == '~'))
				str_temp += str[i];
			else if (str[i] == ' ')
				str_temp += "+";
			else
			{
				str_temp += '%';
				str_temp += to_hex((unsigned char)str[i] >> 4);
				str_temp += to_hex((unsigned char)str[i] % 16);
			}
		}
		return str_temp;
	}

	std::string uri::url_decode(const std::string& str)
	{
		std::string str_temp = "";
		size_t length = str.length();
		for (size_t i = 0; i < length; i++)
		{
			if (str[i] == '+') str_temp += ' ';
			else if (str[i] == '%')
			{
				unsigned char high = from_hex((unsigned char)str[++i]);
				unsigned char low = from_hex((unsigned char)str[++i]);
				str_temp += high * 16 + low;
			}
			else str_temp += str[i];
		}
		return str_temp;
	}






	unsigned char uri::to_hex(unsigned char x)
	{
		return  x > 9 ? x + 55 : x + 48;
	}

	unsigned char uri::from_hex(unsigned char x)
	{
		unsigned char y;
		if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
		else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
		else if (x >= '0' && x <= '9') y = x - '0';
		else;
		return y;
	}

}
