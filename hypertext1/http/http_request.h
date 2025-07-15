#pragma once

#include "../base/base_request.h"
#include <sstream>

class http_request:public base_request
{
public:
	http_request();
	http_request(const std::string& method,const sys::uri& url);
	~http_request();


	std::string user_agent()const;
	void set_user_agent(const std::string& user_agent);


	virtual void reset();
};