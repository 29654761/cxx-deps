#include "rtsp_request.h"

rtsp_request::rtsp_request()
{
	reset();
}


rtsp_request::~rtsp_request()
{

}


uint32_t rtsp_request::cseq()const
{
	std::string value;
	if (!headers.get("CSeq", value))
	{
		return 0;
	}
	char* endptr = nullptr;
	return (uint32_t)strtol(value.c_str(),&endptr,10);
}

void rtsp_request::set_cseq(uint32_t cseq)
{
	headers.set("CSeq", std::to_string(cseq));
}

std::string rtsp_request::session()const
{
	std::string value;
	headers.get("Session", value);
	return value;
}

void rtsp_request::set_session(const std::string& session)
{
	headers.set("Session", session);
}

std::string rtsp_request::user_agent()const
{
	std::string value;
	headers.get("User-Agent", value);
	return value;
}

void rtsp_request::set_user_agent(const std::string& user_agent)
{
	headers.set("User-Agent", user_agent);
}

bool rtsp_request::transport(rtsp_transport& transport)const
{
	std::string value;
	if (!headers.get("Transport", value))
	{
		return false;
	}

	return transport.parse(value);
}

void rtsp_request::set_transport(const rtsp_transport& transport)
{
	std::string value = transport.to_string();
	headers.set("Transport", value);
}

void rtsp_request::reset()
{
	base_request::reset();
	set_request_version("RTSP/1.0");
}
