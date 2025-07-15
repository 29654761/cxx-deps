#pragma once

#include <hypertext/request.h>
#include <hypertext/uri.h>
#include <sstream>


class http_request:public hypertext::request
{
public:
	http_request();
	http_request(const std::string& method,const hypertext::uri& url);
	~http_request();


	std::string user_agent()const;
	void set_user_agent(const std::string& user_agent);


	virtual void reset();
};

