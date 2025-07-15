#pragma once

#include <hypertext/response.h>

#include <sstream>

class http_response:public hypertext::response
{
public:
	http_response();
	~http_response();

	std::string user_agent()const;
	void set_user_agent(const std::string& user_agent);

	virtual void reset();
};