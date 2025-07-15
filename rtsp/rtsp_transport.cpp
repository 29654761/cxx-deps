#include "rtsp_transport.h"

#include <sys2/string_util.h>


rtsp_transport::rtsp_transport()
{
}

rtsp_transport::~rtsp_transport()
{
}

bool rtsp_transport::parse(const std::string& s)
{
	auto vec = sys::string_util::split(s, ";");
	if (vec.size() < 1)
	{
		return false;
	}
	auto itr = vec.begin();
	parse_protos(*itr);
	itr++;
	for (; itr != vec.end(); itr++)
	{
		auto vec2 = sys::string_util::split(*itr, "=", 2);
		if (vec2[0] == "unicast")
		{
			unicast = true;
		}
		else if (vec2[0] == "mode")
		{
			mode = vec2[1];
		}
		else if (vec2[0] == "ssrc")
		{
			char* endptr = nullptr;
			ssrc = strtoul(vec2[1].c_str(), &endptr, 0);
		}
		else if (vec2[0] == "destination")
		{
			destination = vec2[1];
		}
		else if (vec2[0] == "source")
		{
			source= vec2[1];
		}
		else if (vec2[0] == "client_port")
		{
			auto vec3 = sys::string_util::split(vec2[1], "-");
			if (vec3.size() >= 2)
			{
				char* endptr = nullptr;
				rtp_client_port = strtol(vec3[0].c_str(), &endptr, 0);
				rtcp_client_port = strtol(vec3[1].c_str(), &endptr, 0);
			}
		}
		else if (vec2[0] == "server_port")
		{
			auto vec3 = sys::string_util::split(vec2[1], "-");
			if (vec3.size() >= 2)
			{
				char* endptr = nullptr;
				rtp_server_port = strtol(vec3[0].c_str(), &endptr, 0);
				rtcp_server_port = strtol(vec3[1].c_str(), &endptr, 0);
			}
		}
		else if (vec2[0] == "interleaved")
		{
			auto vec3 = sys::string_util::split(vec2[1], "-");
			if (vec3.size() >= 2)
			{
				char* endptr = nullptr;
				rtp_channel = strtol(vec3[0].c_str(), &endptr, 0);
				rtcp_channel = strtol(vec3[1].c_str(), &endptr, 0);
			}
		}
	}

	return true;
}

std::string rtsp_transport::to_string()const
{
	std::stringstream ss;
	make_protos(ss);
	if (unicast)
	{
		ss << ";unicast";
	}
	if (destination.size() > 0)
	{
		ss << ";destination=" << destination;
	}
	if (source.size() > 0)
	{
		ss << ";source=" << source;
	}
	if (is_udp)
	{
		if (rtp_client_port > 0 && rtcp_client_port > 0) 
		{
			ss << ";client_port=" << rtp_client_port << "-" << rtcp_client_port;
		}
		if (rtp_server_port > 0 && rtcp_server_port > 0)
		{
			ss << ";server_port=" << rtp_server_port << "-" << rtcp_server_port;
		}
	}
	else
	{
		ss<<";interleaved=" << rtp_channel << "-" << rtcp_channel;
	}
	if (ssrc > 0)
	{
		ss << ";ssrc=" << ssrc;
	}
	if (mode.size() > 0)
	{
		ss << ";mode=" << mode;
	}
	return ss.str();
}

void rtsp_transport::parse_protos(const std::string protos)
{
	is_rtp = false;
	is_avp = false;
	is_udp = true;
	is_tcp = false;

	auto vec=sys::string_util::split(protos, "/");
	for (auto itr = vec.begin(); itr != vec.end(); itr++)
	{
		if (sys::string_util::icasecompare(*itr, "RTP"))
		{
			is_rtp = true;
		}
		else if (sys::string_util::icasecompare(*itr, "AVP"))
		{
			is_avp = true;
		}
		else if (sys::string_util::icasecompare(*itr, "TCP"))
		{
			is_tcp = true;
			is_udp = false;
		}
	}
}

void rtsp_transport::make_protos(std::stringstream& ss)const
{
	if (is_rtp)
	{
		ss << "RTP/";
	}
	if (is_avp)
	{
		ss << "AVP/";
	}
	if (is_udp)
	{
		ss << "UDP";
	}
	if (is_tcp)
	{
		ss << "TCP";
	}

}
