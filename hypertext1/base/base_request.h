#pragma once

#include "base_message.h"
#include <sys2/uri.h>

class base_request:public base_message
{
public:
	base_request();
	virtual ~base_request();



	bool match_method(const std::string& method);

};

