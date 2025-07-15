/**
 * @file sdp_media.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#include "sdp_media.h"
#include "sys2/string_util.h"

#include <algorithm>

namespace litertp {


	sdp_media::sdp_media()
	{
		protos.insert("UDP");
		protos.insert("RTP");
		protos.insert("AVP");
	}

	sdp_media::~sdp_media()
	{
	}

	bool sdp_media::parse_header(const std::string& m)
	{
		std::vector<std::string> ss=sys::string_util::split(m, " ");
		if (ss.size() < 3)
		{
			return false;
		}

		//media type
		if (ss[0] == "video")
		{
			media_type = media_type_video;
		}
		else if (ss[0] == "audio")
		{
			media_type = media_type_audio;
		}
		else if (ss[0] == "application")
		{
			media_type = media_type_application;
		}
		else
		{
			media_type = media_type_unknown;
		}

		//port
		rtp_port = atoi(ss[1].c_str());

		//protos
		this->protos.clear();
		std::vector<std::string> protos = sys::string_util::split(ss[2], "/");
		for (auto proto : protos)
		{
			std::transform(proto.begin(), proto.end(), proto.begin(), ::toupper);
			this->protos.insert(proto);
		}

		if (media_type == media_type_application)
		{
			if (ss.size() >= 4) {
				app_data_type = ss[3];
			}
		}
		else
		{
			//payload type
			rtpmap.clear();
			for (size_t i = 3; i < ss.size(); i++)
			{
				sdp_format fmt;
				fmt.pt = atoi(ss[i].c_str());
				rtpmap.insert(std::make_pair(fmt.pt, fmt));
			}
		}

		return true;
	}

	bool sdp_media::parse_c(const std::string& c)
	{
		std::vector<std::string> ss=sys::string_util::split(c, " ");
		if (ss.size() < 3)
		{
			return false;
		}
		this->has_rtp_address = true;
		this->rtp_network_type = ss[0];
		this->rtp_address_type = ss[1];
		this->rtp_address = ss[2];
		return true;
	}

	bool sdp_media::parse_a(const std::string& s)
	{
		sdp_pair attr(":");
		if (!attr.parse(s))
		{
			return false;
		}

		parse_a(attr);
		return true;
	}

	void sdp_media::parse_a(sdp_pair& attr)
	{

		if (attr.key == "rtcp")
		{
			std::vector<std::string> ss = sys::string_util::split(attr.val, " ");
			if (ss.size() >= 4)
			{
				this->has_rtcp = true;
				this->rtcp_port = atoi(ss[0].c_str());
				this->rtcp_network_type = ss[1];
				this->rtcp_address_type = ss[2];
				this->rtcp_address = ss[3];
			}
		}
		else if (attr.key == "ice-ufrag")
		{
			this->ice_ufrag = attr.val;
		}
		else if (attr.key == "ice-pwd")
		{
			this->ice_pwd = attr.val;
		}
		else if (attr.key == "ice-options")
		{
			this->ice_options = attr.val;
		}
		else if (attr.key == "fingerprint")
		{
			std::vector<std::string> ss = sys::string_util::split(attr.val, " ", 2);
			if (ss.size() >= 2)
			{
				this->fingerprint_sign = ss[0];
				this->fingerprint = ss[1];
			}
		}
		else if (attr.key == "setup")
		{
			if (attr.val == "actpass")
			{
				this->setup = sdp_setup_actpass;
			}
			else if (attr.val == "active")
			{
				this->setup = sdp_setup_active;
			}
			else if (attr.val == "passive")
			{
				this->setup = sdp_setup_passive;
			}
		}
		else if (attr.key == "mid")
		{
			this->mid = attr.val;
		}
		else if (attr.key == "msid")
		{
			if (attr.val.find('-') == 0)
			{
				attr.val.erase(0, 1);
			}
			sys::string_util::trim(attr.val);
			this->msid = attr.val;
		}
		else if (attr.key == "extmap")
		{
			std::vector<std::string> ss = sys::string_util::split(attr.val, " ", 2);
			if (ss.size() >= 2)
			{
				int pt = atoi(ss[0].c_str());
				auto iter = extmap.insert(std::make_pair(pt, ss[1]));
				if (!iter.second)
				{
					iter.first->second = ss[1];
				}
			}
		}
		else if (attr.key == "sendonly")
		{
			this->trans_mode = rtp_trans_mode_sendonly;
		}
		else if (attr.key == "recvonly")
		{
			this->trans_mode = rtp_trans_mode_recvonly;
		}
		else if (attr.key == "sendrecv")
		{
			this->trans_mode = rtp_trans_mode_sendrecv;
		}
		else if (attr.key == "rtcp-mux")
		{
			this->rtcp_mux = true;
		}
		else if (attr.key == "rtcp-rsize")
		{
			this->rtcp_rsize = true;
		}
		else if (attr.key == "candidate")
		{
			candidate cd;
			if (cd.parse(attr.val))
			{
				candidates.push_back(cd);
			}
		}
		else if (attr.key == "rtpmap")
		{
			std::vector<std::string> ss = sys::string_util::split(attr.val, " ", 2);
			if (ss.size() >= 2)
			{
				int pt = atoi(ss[0].c_str());
				std::vector<std::string> ss2 = sys::string_util::split(ss[1], "/");

				auto iter = this->rtpmap.find(pt);
				if (iter != this->rtpmap.end())
				{
					if (ss2.size() >= 1)
					{
						iter->second.set_codec(ss2[0]);
					}
					if (ss2.size() >= 2)
					{
						iter->second.frequency = atoi(ss2[1].c_str());
					}
					if (ss2.size() >= 3)
					{
						iter->second.channels = atoi(ss2[2].c_str());
					}
				}
			}
		}
		else if (attr.key == "rtcp-fb")
		{
			std::vector<std::string> ss = sys::string_util::split(attr.val, " ", 2);
			if (ss.size() >= 2)
			{
				if (ss[0] == "*")
				{
					for (auto iter = rtpmap.begin(); iter != rtpmap.end(); iter++)
					{
						iter->second.rtcp_fb.insert(ss[1]);
					}
				}
				else
				{
					char* endptr = nullptr;
					int pt = strtol(ss[0].c_str(), &endptr, 10);
					auto iter = this->rtpmap.find(pt);
					if (iter != this->rtpmap.end())
					{
						iter->second.rtcp_fb.insert(ss[1]);
					}
				}
			}

		}
		else if (attr.key == "fmtp")
		{
			std::vector<std::string> ss = sys::string_util::split(attr.val, " ", 2);
			if (ss.size() >= 2)
			{
				int pt = atoi(ss[0].c_str());
				auto iter = this->rtpmap.find(pt);
				if (iter != this->rtpmap.end())
				{
					iter->second.fmtp.parse(ss[1]);
				}
			}
		}
		else if (attr.key == "ssrc-group")
		{
			std::vector<std::string> ss = sys::string_util::split(attr.val, " ");
			this->ssrc_group = ss[0];
			for (size_t i = 1; i < ss.size(); i++)
			{
				ssrc_t ssrc;
				char* endptr = nullptr;
				ssrc.ssrc = strtoul(ss[i].c_str(), &endptr, 0);


				auto itr = std::find_if(ssrcs.begin(), ssrcs.end(), [&ssrc](auto& a) {
					return a.ssrc == ssrc.ssrc;
				});

				if (itr == ssrcs.end())
				{
					ssrcs.push_back(ssrc);
				}
			}
		}
		else if (attr.key == "ssrc")
		{
			ssrc_t ssrc;
			std::vector<std::string> ss = sys::string_util::split(attr.val, " ", 2);
			char* endptr = nullptr;
			ssrc.ssrc = strtoul(ss[0].c_str(), &endptr, 0);

			auto itr = std::find_if(ssrcs.begin(), ssrcs.end(), [&ssrc](auto& a) {
				return a.ssrc == ssrc.ssrc;
			});
			if (itr == ssrcs.end())
			{
				itr = ssrcs.insert(ssrcs.end(), ssrc);
			}


			if (ss.size() > 1)
			{
				std::vector<std::string> ss2 = sys::string_util::split(ss[1], ":", 2);
				if (ss2.size() >= 2)
				{
					const std::string& key = ss2[0];
					std::string& val = ss2[1];
					if (key == "cname")
					{
						itr->cname = val;
					}
					else if (key == "msid")
					{
						if (val.find('-') == 0)
						{
							val.erase(0, 1);
						}
						sys::string_util::trim(val);
						itr->msid = val;
					}
				}
			}


		}
		else if (attr.key == "control")
		{
			control = attr.val;
		}
		else
		{
			attrs.push_back(attr);
		}
	}

	//protocols need to order
	void sdp_media::to_protocols_string(std::stringstream& ss)const
	{
		bool has_protocal = false;
		auto ps = protos;
		if (ps.find("UDP") != ps.end())
		{
			ss << "UDP/";
			ps.erase("UDP");
			has_protocal = true;
		}
		else if (ps.find("TCP") != ps.end())
		{
			ss << "TCP/";
			ps.erase("TCP");
			has_protocal = true;
		}


		if (ps.find("TLS") != ps.end())
		{
			ss << "TLS/";
			ps.erase("TLS");
			has_protocal = true;
		}
		if (ps.find("DTLS") != ps.end())
		{
			ss << "DTLS/";
			ps.erase("DTLS");
			has_protocal = true;
		}

		if (ps.find("RTP") != ps.end())
		{
			ss << "RTP/";
			ps.erase("RTP");
			has_protocal = true;
		}


		if (ps.find("SAVPF") != ps.end())
		{
			ss << "SAVPF/";
			ps.erase("SAVPF");
			has_protocal = true;
		}
		else if (ps.find("AVPF") != ps.end())
		{
			ss << "AVPF/";
			ps.erase("AVPF");
			has_protocal = true;
		}
		else if (ps.find("SAVP") != ps.end())
		{
			ss << "SAVP/";
			ps.erase("SAVP");
			has_protocal = true;
		}
		else if (ps.find("AVP") != ps.end())
		{
			ss << "AVP/";
			ps.erase("AVP");
			has_protocal = true;
		}

		if (ps.find("SCTP") != ps.end())
		{
			ss << "SCTP/";
			ps.erase("SCTP");
			has_protocal = true;
		}

		for (const auto& p : ps)
		{
			ss << p << "/";
			has_protocal = true;
		}
		
		if (has_protocal)
		{
			ss.seekp(-1, std::ios_base::end);
		}
	}

	void sdp_media::to_string(std::stringstream& ss)const
	{
		//m
		ss << "m=";
		if (media_type == media_type_audio)
		{
			ss << "audio";
		}
		else if (media_type == media_type_video)
		{
			ss << "video";
		}
		else if (media_type == media_type_application)
		{
			ss << "application";
		}
		else
		{
			ss << "unknown";
		}
		
		ss<< " " << rtp_port << " ";
		to_protocols_string(ss);
		ss << " ";

		if (rtpmap.size() > 0)
		{
			for (auto fmt : rtpmap)
			{
				ss << (int)fmt.second.pt << " ";
			}
			ss.seekp(-1, std::ios_base::end);
		}

		if (app_data_type.size() > 0)
		{
			ss << app_data_type;
		}
		

		ss << "\r\n";

		//c
		if (has_rtp_address) {
			ss << "c=" << rtp_network_type << " " << rtp_address_type << " " << rtp_address << "\r\n";
		}

		for (auto itr=b.begin();itr!=b.end();itr++)
		{
			ss << "b=" << *itr << "\r\n";
		}

		if (has_rtcp) {
			ss << "a=rtcp:" << rtcp_port << " " << rtcp_network_type << " " << rtcp_address_type << " " << rtcp_address << "\r\n";
		}

		if (candidates.size() > 0)
		{
			for (auto& ca : candidates)
			{
				ss << "a=candidate:" << ca.to_string() << "\r\n";
			}
			ss << "a=end-of-candidates" << "\r\n";
		}

		if (ice_ufrag.size() > 0) 
		{
			ss << "a=ice-ufrag:" << ice_ufrag << "\r\n";
		}
		if (ice_pwd.size() > 0)
		{
			ss << "a=ice-pwd:" << ice_pwd << "\r\n";
		}
		if (ice_options.size() > 0) 
		{
			ss << "a=ice-options:" << ice_options << "\r\n";
		}
		if (fingerprint.size() > 0)
		{
			ss << "a=fingerprint:" << fingerprint_sign << " " << fingerprint << "\r\n";
		}


		if (setup == sdp_setup_active)
		{
			ss << "a=setup:active" << "\r\n";
		}
		else if (setup == sdp_setup_actpass)
		{
			ss << "a=setup:actpass" << "\r\n";
		}
		else if (setup == sdp_setup_passive)
		{
			ss << "a=setup:passive" << "\r\n";
		}
		if (mid.size() > 0) 
		{
			ss << "a=mid:" << mid << "\r\n";
		}
		if (msid.size() > 0)
		{
			ss << "a=msid:- " << msid << "\r\n";
		}
		if (rtcp_mux)
		{
			ss << "a=rtcp-mux" << "\r\n";
		}
		if (rtcp_rsize)
		{
			ss << "a=rtcp-rsize" << "\r\n";
		}
		if (control.size() > 0)
		{
			ss << "a=control:" << control << "\r\n";
		}

		for (auto& exm : this->extmap)
		{
			ss << "a=extmap:" << exm.first << " " << exm.second << "\r\n";
		}

		if (trans_mode == rtp_trans_mode_inactive) 
		{
			ss << "a=inactive" << "\r\n";
		}
		else if (trans_mode == rtp_trans_mode_recvonly)
		{
			ss << "a=recvonly" << "\r\n";
		}
		else if(trans_mode == rtp_trans_mode_sendonly)
		{
			ss << "a=sendonly" << "\r\n";
		}
		else if (trans_mode == rtp_trans_mode_sendrecv)
		{
			ss << "a=sendrecv" << "\r\n";
		}


		for (auto& fmt : rtpmap)
		{
			fmt.second.to_string(ss);
		}

		if (ssrc_group.size() > 0)
		{
			ss << "a=ssrc-group:" << ssrc_group;
			for (auto& ssrct : ssrcs)
			{
				ss << " " << ssrct.ssrc;
			}
			ss << "\r\n";
		}

		for (auto& ssrct : ssrcs)
		{
			if (ssrct.cname.size() == 0 && ssrct.msid.size() == 0)
			{
				ss << "a=ssrc:" << ssrct.ssrc << "\r\n";
			}
			else
			{
				if (ssrct.cname.size() > 0)
				{
					ss << "a=ssrc:" << ssrct.ssrc << " cname:" << ssrct.cname << "\r\n";
				}
				if (ssrct.msid.size() > 0)
				{
					ss << "a=ssrc:" << ssrct.ssrc << " msid:" << ssrct.msid << "\r\n";
				}
			}
		}

		for (auto& attr : attrs)
		{
			ss << "a=" << attr.to_string() << "\r\n";
		}

		if (y > 0) {
			ss << "y=" << y << "\r\n";
		}
	}


	bool sdp_media::negotiate(const sdp_format* fmt_in, sdp_format* fmt_out)
	{
		for (auto& itr : rtpmap)
		{
			if (itr.second.codec == fmt_in->codec && itr.second.frequency == fmt_in->frequency)
			{
				if (fmt_in->fmtp.packetization_mode() != itr.second.fmtp.packetization_mode())
					continue;
				if (fmt_in->fmtp.profile_id() != itr.second.fmtp.profile_id())
					continue;


				if (!fmt_in->fmtp.level_asymmetry_allowed() && !itr.second.fmtp.level_asymmetry_allowed())
				{
					if (fmt_in->fmtp.level_id() != itr.second.fmtp.level_id())
						continue;
				}

				if (fmt_out)
				{
					*fmt_out = itr.second;
				}

				return true;
			}
		}
		return false;
	}

	uint32_t sdp_media::get_default_ssrc()const
	{
		auto itr=ssrcs.begin();
		if (itr == ssrcs.end())
		{
			return 0;
		}
		return itr->ssrc;
	}

	bool sdp_media::has_ssrc(uint32_t ssrc)const
	{
		for (auto itr = ssrcs.begin(); itr != ssrcs.end(); itr++)
		{
			if (itr->ssrc == ssrc)
			{
				return true;
			}
		}
		return false;
	}

	void sdp_media::set_msid(const std::string& msid)
	{
		this->msid = msid;
		for (auto itr = ssrcs.begin(); itr != ssrcs.end(); itr++)
		{
			itr->msid = msid;
		}
	}

	bool sdp_media::has_attribute(const std::string& key)const
	{
		auto itr = std::find_if(attrs.begin(), attrs.end(), [&key](const sdp_pair& kv) {return kv.key == key; });
		return itr != attrs.end();
	}

	std::string sdp_media::get_attribute(const std::string& key, const std::string& def)const
	{
		auto itr = std::find_if(attrs.begin(), attrs.end(), [&key](const sdp_pair& kv) {return kv.key == key; });
		if (itr == attrs.end())
			return def;

		return itr->val;
	}

	void sdp_media::remove_attrs(const std::string& key)
	{
		for (auto itr = attrs.begin(); itr != attrs.end();)
		{
			if (itr->key == key)
			{
				itr = attrs.erase(itr);
			}
			else
			{
				itr++;
			}
		}
	}

	bool sdp_media::is_security()const
	{
		return protos.find("TLS") != protos.end() || protos.find("DTLS") != protos.end();
	}
}

