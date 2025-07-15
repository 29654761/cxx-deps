#include "rtsp_response.h"

rtsp_response::rtsp_response()
{
	reset();
}


rtsp_response::~rtsp_response()
{

}


uint32_t rtsp_response::cseq()const
{
	std::string value;
	if (!headers.get("CSeq", value))
	{
		return 0;
	}
	char* endptr = nullptr;
	return (uint32_t)strtol(value.c_str(),&endptr,10);
}

void rtsp_response::set_cseq(uint32_t cseq)
{
	headers.set("CSeq", std::to_string(cseq));
}

std::string rtsp_response::session()const
{
	std::string value;
	headers.get("Session", value);
	return value;
}

void rtsp_response::set_session(const std::string& session)
{
	headers.set("Session", session);
}

std::string rtsp_response::user_agent()const
{
	std::string value;
	headers.get("User-Agent", value);
	return value;
}

void rtsp_response::set_user_agent(const std::string& user_agent)
{
	headers.set("User-Agent", user_agent);
}

bool rtsp_response::transport(rtsp_transport& transport)const
{
	std::string value;
	if (!headers.get("Transport", value))
	{
		return false;
	}

	return transport.parse(value);
}

void rtsp_response::set_transport(const rtsp_transport& transport)
{
	std::string value = transport.to_string();
	headers.set("Transport", value);
}

void rtsp_response::reset()
{
	hypertext::response::reset();
	set_response_version("RTSP/1.0");
}

