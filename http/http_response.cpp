#include "http_response.h"

http_response::http_response()
{
	reset();
}


http_response::~http_response()
{

}


std::string http_response::user_agent()const
{
	std::string value;
	headers.get("User-Agent", value);
	return value;
}

void http_response::set_user_agent(const std::string& user_agent)
{
	headers.set("User-Agent", user_agent);
}

void http_response::reset()
{
	hypertext::response::reset();
	set_response_version("HTTP/1.0");
}

