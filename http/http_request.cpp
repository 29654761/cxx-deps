#include "http_request.h"

http_request::http_request()
{
	reset();
}

http_request::http_request(const std::string& method, const hypertext::uri& url)
{
	this->set_method(method);
	this->set_url(url.to_string());
}

http_request::~http_request()
{

}




std::string http_request::user_agent()const
{
	std::string value;
	headers.get("User-Agent", value);
	return value;
}

void http_request::set_user_agent(const std::string& user_agent)
{
	headers.set("User-Agent", user_agent);
}


void http_request::reset()
{
	hypertext::request::reset();
	set_request_version("HTTP/1.0");
}
