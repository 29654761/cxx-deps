/**
 * @file fmtp.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */



#include "fmtp.h"


#include <sys2/string_util.h>
#include <sstream>
#include <iostream>
#include <iomanip>

namespace litertp {
	fmtp::fmtp()
	{
	}

	fmtp::~fmtp()
	{
	}

	void fmtp::parse(const std::string& s)
	{
		auto vec = sys::string_util::split(s, ";");
		map.clear();
		for (auto& s2 : vec)
		{
			auto vec2 = sys::string_util::split(s2, "=");
			if (vec2.size() >= 2)
			{
				map[vec2[0]] = vec2[1];
			}
		}
	}

	std::string fmtp::to_string()const
	{
		std::stringstream ss;
		for (auto& kv : map)
		{
			ss << kv.first << "=" << kv.second << ";";
		}
		std::string s = ss.str();
		if (map.size() > 0)
		{
			s.erase(s.size() - 1, 1);
		}
		return s;
	}

	std::string fmtp::get(const std::string& key, const std::string& def)const
	{
		auto itr = map.find(key);
		if (itr == map.end())
		{
			return def;
		}

		return itr->second;
	}

	int64_t fmtp::get(const std::string& key, int64_t def)const
	{
		auto itr = map.find(key);
		if (itr == map.end())
		{
			return def;
		}

		char* endptr = nullptr;
		return strtoll(itr->second.c_str(), &endptr, 0);
	}

	uint64_t fmtp::get(const std::string& key, uint64_t def)const
	{
		auto itr = map.find(key);
		if (itr == map.end())
		{
			return def;
		}

		char* endptr = nullptr;
		return strtoull(itr->second.c_str(), &endptr, 0);
	}

	uint64_t fmtp::get_hex(const std::string& key, uint64_t def)const
	{
		auto itr = map.find(key);
		if (itr == map.end())
		{
			return def;
		}

		char* endptr = nullptr;
		return strtoull(itr->second.c_str(), &endptr, 16);
	}

	long double fmtp::get(const std::string& key, long double def)const
	{
		auto itr = map.find(key);
		if (itr == map.end())
		{
			return def;
		}

		char* endptr = nullptr;
		return strtold(itr->second.c_str(), &endptr);
	}

	void fmtp::set(const std::string& key, const std::string& val)
	{
		map[key] = val;
	}

	void fmtp::set(const std::string& key, int64_t val)
	{
		map[key] = std::to_string(val);
	}
	void fmtp::set(const std::string& key, uint64_t val)
	{
		map[key] = std::to_string(val);
	}

	void fmtp::set(const std::string& key, double val)
	{
		map[key] = std::to_string(val);
	}

	void fmtp::set_profile_level_id(uint32_t v)
	{
		std::stringstream ss;
		ss << std::hex << std::setw(6) << std::setfill('0') << v;
		map["profile-level-id"] = ss.str();
	}
	uint32_t fmtp::profile_level_id()const
	{
		return (uint32_t)get_hex("profile-level-id");
	}

	void fmtp::set_profile_id(uint8_t v)
	{
		std::stringstream ss;
		ss << std::hex << (int)v;
		map["profile-id"] = ss.str();
	}

	uint8_t fmtp::profile_id()const
	{
		auto itr = map.find("profile-id");
		if (itr != map.end())
		{
			char* endptr = nullptr;
			return strtoull(itr->second.c_str(), &endptr, 16);
		}

		uint32_t id = profile_level_id();
		return (id & 0xFF0000) >> 16;
	}

	void fmtp::set_level_id(uint16_t v)
	{
		std::stringstream ss;
		ss << std::hex << std::setfill('0') << v;
		map["level-id"] = ss.str();
	}
	uint16_t fmtp::level_id()const
	{
		auto itr = map.find("level-id");
		if (itr != map.end())
		{
			char* endptr = nullptr;
			return strtoull(itr->second.c_str(), &endptr, 16);
		}

		uint32_t id = profile_level_id();
		return id & 0xFFFF;
	}

	void fmtp::set_packetization_mode(int mode)
	{
		map["packetization-mode"] = std::to_string(mode);
	}

	int fmtp::packetization_mode()const
	{
		return (int)get("packetization-mode",(int64_t)0);
	}

	void fmtp::set_level_asymmetry_allowed(int allowed)
	{
		map["level-asymmetry-allowed"] = std::to_string(allowed);
	}

	int fmtp::level_asymmetry_allowed()const
	{
		return (int)get("level-asymmetry-allowed", (int64_t)0);
	}

	void fmtp::set_mbr(uint32_t mbr)
	{
		map["max-br"] = std::to_string(mbr);
	}

	uint32_t fmtp::mbr()const
	{
		return (uint32_t)get("max-br", (int64_t)0);
	}

	void fmtp::set_mbps(uint32_t mbps)
	{
		map["max-mbps"] = std::to_string(mbps);
	}

	uint32_t fmtp::mbps()const
	{
		return (uint32_t)get("max-mbps", (int64_t)0);
	}

	void fmtp::set_mfs(uint32_t mfs)
	{
		map["max-fs"] = std::to_string(mfs);
	}
	uint32_t fmtp::mfs()const
	{
		return (uint32_t)get("max-fs", (int64_t)0);
	}

	void fmtp::set_apt(uint8_t apt)
	{
		map["apt"] = std::to_string(apt);
	}

	uint8_t fmtp::apt()const
	{
		return (uint8_t)get("apt", (int64_t)0);
	}

	void fmtp::set_minptime(int minptime)
	{
		map["minptime"] = std::to_string(minptime);
	}

	int fmtp::minptime()const
	{
		return (int)get("minptime", (int64_t)0);
	}

	void fmtp::set_useinbandfec(bool useinbandfec)
	{
		map["useinbandfec"] = useinbandfec?"1":"0";
	}

	bool fmtp::useinbandfec()const
	{
		int v= (int)get("useinbandfec", (int64_t)0);
		return v != 0;
	}
}


