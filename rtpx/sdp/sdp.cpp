/**
 * @file sdp.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#include "sdp.h"

#include <sstream>
#include "sys2/string_util.h"


namespace rtpx {




	std::string sdp::to_string()const
	{
		std::stringstream ss;
		ss << "v=" << v << "\r\n";
		ss << "o=" << o << "\r\n";
		ss << "s=" << s << "\r\n";
		if (i.size() > 0) {
			ss << "i=" << i << "\r\n";
		}
		if (u.size() > 0) {
			ss << "u=" << u << "\r\n";
		}
		if (e.size() > 0) {
			ss << "e=" << e << "\r\n";
		}
		if (p.size() > 0) {
			ss << "p=" << p << "\r\n";
		}

		if (has_address) {
			ss << "c=" << network_type << " " << address_type << " " << address << "\r\n";
		}
		for (auto itr = b.begin(); itr != b.end(); itr++)
		{
			ss << "b=" << *itr << "\r\n";
		}
		if (z.size() > 0) {
			ss << "z=" << z << "\r\n";
		}
		if (k.size() > 0) {
			ss << "k=" << k << "\r\n";
		}
		if (t.size() > 0) {
			ss << "t=" << t << "\r\n";
		}
		if (r.size() > 0) {
			ss << "r=" << r << "\r\n";
		}

		if (bundle)
		{
			ss << "a=group:BUNDLE";
			for (auto m : medias)
			{
				ss << " " << m.mid;
			}
			ss << "\r\n";
		}

		for (auto a : attrs)
		{
			ss << "a=" << a.to_string() << "\r\n";
		}
		if (candidates.size() > 0)
		{
			for (auto& ca : candidates)
			{
				ss << "a=candidate:" << ca.to_string() << "\r\n";
			}
			ss << "a=end-of-candidates" << "\r\n";
		}

		for (auto m : medias)
		{
			m.to_string(ss);
		}

		return ss.str();
	}

	bool sdp::parse(const std::string& s)
	{
		std::vector<std::string> lines;
		sys::string_util::split(s, "\n", lines);

		this->attrs.clear();
		this->medias.clear();
		this->has_address = false;
		
		for (auto line : lines)
		{
			std::vector<std::string> vec;
			sys::string_util::split(line, "=", vec, 2);
			if (vec.size() < 2)
				continue;
			sys::string_util::trim(vec[1]);
			const std::string& key = vec[0];
			const std::string& val = vec[1];

			if (key == "v")
			{
				this->v = val;
			}
			else if (key == "o")
			{
				this->o = val;
			}
			else if (key == "s")
			{
				this->s = val;
			}
			else if (key == "t")
			{
				this->t = val;
			}
			else if (key == "m")
			{
				sdp_media m;
				m.parse_header(val);
				medias.push_back(m);
			}
			else if(key=="a")
			{
				auto iter = this->medias.rbegin();
				if (iter != medias.rend())
				{
					iter->parse_a(val);
				}
				else
				{
					sdp_pair attr(":");
					if (attr.parse(val)) 
					{
						if (attr.key == "group")
						{
							std::vector<std::string> vec=sys::string_util::split(attr.val, " ");
							if (vec.size() >= 2) 
							{
								if (vec[0] == "BUNDLE")
								{
									bundle = true;
								}
							}
						}
						else if (attr.key == "candidate")
						{
							candidate cd;
							if (cd.parse(attr.val))
							{
								candidates.push_back(cd);
							}
						}

						this->attrs.push_back(attr);
					}
				}
			}
			else if (key == "b")
			{
				auto iter = this->medias.rbegin();
				if (iter != medias.rend())
				{
					iter->b.push_back(val);
				}
				else
				{
					this->b.push_back(val);
				}
			}
			else if (key == "c")
			{
				auto iter = this->medias.rbegin();
				if (iter != medias.rend())
				{
					iter->parse_c(val);
				}
				else
				{
					std::vector<std::string> ss = sys::string_util::split(val, " ");
					if (ss.size() >= 3)
					{
						this->has_address = true;
						this->network_type = ss[0];
						this->address_type = ss[1];
						this->address = ss[2];
					}
				}
			}
			else if (key == "y")
			{
				auto iter = this->medias.rbegin();
				if (iter != medias.rend())
				{
					char* endptr = nullptr;
					iter->y=strtoul(val.c_str(),&endptr,0);
				}
			}
		}

		auto itr_a = std::find_if(this->attrs.begin(), this->attrs.end(), [](sdp_pair& a) {
			return a.key == "fingerprint";
		});

		for (auto itr = medias.begin(); itr != medias.end(); itr++)
		{
			if (itr_a != this->attrs.end()&&itr->fingerprint.size()==0)
			{
				itr->parse_a(*itr_a);
			}

			if (!itr->has_rtp_address)
			{
				itr->rtp_network_type = this->network_type;
				itr->rtp_address_type = this->address_type;
				itr->rtp_address = this->address;
				itr->has_rtp_address = true;
			}
			if (!itr->has_rtcp)
			{
				itr->rtcp_network_type = this->network_type;
				itr->rtcp_address_type = this->address_type;
				itr->rtcp_address = this->address;
				itr->has_rtcp = true;
			}

			if (itr->rtcp_port == 0&& itr->rtp_port!=0&& itr->rtp_port!=9)
			{
				if (itr->rtcp_mux)
					itr->rtcp_port = itr->rtp_port;
				else
					itr->rtcp_port = itr->rtp_port+1;
			}


		}

		return true;
	}

	bool sdp::has_attribute(const std::string& key)const
	{
		auto itr = std::find_if(attrs.begin(), attrs.end(), [&key](const sdp_pair& kv) {return kv.key == key; });
		return itr != attrs.end();
	}

	std::string sdp::get_attribute(const std::string& key, const std::string& def)const
	{
		auto itr = std::find_if(attrs.begin(), attrs.end(), [&key](const sdp_pair& kv) {return kv.key == key; });
		if (itr == attrs.end())
			return def;

		return itr->val;
	}
}
