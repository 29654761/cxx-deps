/**
 * @file sdp_format.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */



#include "sdp_format.h"
#include <sys2/string_util.h>

namespace litertp
{
	sdp_format::sdp_format()
	{
	}

	sdp_format::sdp_format(int payload_type, const codec_type_t& codec, int frequency, int channels)
	{
		this->pt = payload_type;
		this->codec = codec;
		this->frequency = frequency;
		this->channels = channels;
	}

	sdp_format::~sdp_format()
	{
	}

	void sdp_format::set_codec(const std::string& codec)
	{
		this->codec_text = codec;
		this->codec = convert_codec(codec);
	}

	const std::string sdp_format::get_codec()const
	{
		std::string ret = this->codec_text;
		if (this->codec != codec_type_unknown)
		{
			ret = convert_codec_text(this->codec);
		}
		return ret;
	}

	void sdp_format::to_string(std::stringstream& ss)const
	{
		std::string codec_str = get_codec();
		
		ss << "a=rtpmap:" << (int)pt << " " << codec_str << "/" << frequency;
		if (channels > 1)
		{
			ss << "/" << channels;
		}
		ss << "\r\n";

		for (auto& rtcp_fb : rtcp_fb)
		{
			ss << "a=rtcp-fb:" << (int)pt << " " << rtcp_fb << "\r\n";
		}
		if (!fmtp.is_empty())
		{
			ss << "a=fmtp:" << (int)pt << " " << fmtp.to_string() << "\r\n";
		}
		for (auto& attr : attrs)
		{
			ss << "a=" << attr.to_string() << "\r\n";
		}
	}

	codec_type_t sdp_format::convert_codec(const std::string& codec)
	{
		size_t c = sizeof(codec_def) / sizeof(codec_def_t);
		for (size_t i = 0; i < c; i++)
		{
			if (strcasecmp(codec_def[i].def,codec.c_str())==0)
			{
				return  codec_def[i].type;
			}
		}
		return codec_type_unknown;
	}

	std::string sdp_format::convert_codec_text(codec_type_t codec)
	{
		size_t c = sizeof(codec_def) / sizeof(codec_def_t);
		for (size_t i = 0; i < c; i++)
		{
			if (codec_def[i].type == codec)
			{
				return  codec_def[i].def;
			}
		}
		return "";
	}

	/*
	bool sdp_format::extract_h264_fmtp(int* level_asymmetry_allowed, int* packetization_mode, int64_t* profile_level_id)const
	{
		bool detected = false;

		*level_asymmetry_allowed = 0;
		*packetization_mode = 0;
		*profile_level_id = 0;

		for (auto fmt : fmtp)
		{
			auto vec=sys::string_util::split(fmt, ";");
			for (auto kv : vec)
			{
				auto vec2=sys::string_util::split(kv, "=");
				if (vec2.size() == 2)
				{
					std::string key = vec2[0];
					std::string val = vec2[1];

					if (key == "level-asymmetry-allowed")
					{
						char* endptr = nullptr;
						*level_asymmetry_allowed = strtol(val.c_str(),&endptr,0);
						detected = true;
					}
					else if (key == "packetization-mode")
					{
						char* endptr = nullptr;
						*packetization_mode = strtol(val.c_str(), &endptr, 0);
						detected = true;
					}
					else if (key == "profile-level-id")
					{
						char* endptr = nullptr;
						*profile_level_id = strtoll(val.c_str(), &endptr, 16);
						detected = true;
					}
				}
			}

			if (detected)
			{
				break;
			}
		}

		return detected;
	}
	*/
}