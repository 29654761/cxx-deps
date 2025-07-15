/**
 * @file fmtp.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#pragma once

#include <string>
#include <map>

namespace rtpx {
	class fmtp
	{
	public:
		fmtp();
		~fmtp();

		void parse(const std::string& s);
		std::string to_string()const;

		std::string get(const std::string& key, const std::string& def = "")const;
		int64_t get(const std::string& key, int64_t def = 0)const;
		uint64_t get(const std::string& key, uint64_t def = 0)const;
		uint64_t get_hex(const std::string& key, uint64_t def = 0)const;
		long double get(const std::string& key, long double def = 0)const;

		void set(const std::string& key, const std::string& val);
		void set(const std::string& key, int64_t val);
		void set(const std::string& key, uint64_t val);
		void set(const std::string& key, double val);

		bool is_empty()const { return map.empty(); }
		void clear() { map.clear(); }

		void set_profile_level_id(uint32_t v);
		uint32_t profile_level_id()const;

		void set_profile_id(uint8_t v);
		uint8_t profile_id()const;

		void set_level_id(uint16_t v);
		uint16_t level_id()const;

		void set_packetization_mode(int mode);
		int packetization_mode()const;

		void set_level_asymmetry_allowed(int allowed);
		int level_asymmetry_allowed()const;

		void set_mbr(uint32_t mbr);
		uint32_t mbr()const;

		void set_mbps(uint32_t mbps);
		uint32_t mbps()const;

		void set_mfs(uint32_t mfs);
		uint32_t mfs()const;

		void set_apt(uint8_t apt);
		uint8_t apt()const;

		void set_minptime(int minptime);
		int minptime()const;

		void set_useinbandfec(bool useinbandfec);
		bool useinbandfec()const;
	private:
		std::map<std::string, std::string> map;
	};

}

