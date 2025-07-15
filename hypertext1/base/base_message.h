#pragma once

#include "base_headers.h"
#include <sys2/uri.h>

class base_message
{
public:
	base_message();
	virtual ~base_message();

	int64_t content_length()const;
	std::string content_type()const;
	void set_content_type(const std::string& content_type);

	bool set_first_line(const std::string& line);
	bool set_header_line(const std::string& line);
	void set_body(const std::string& body);

	bool parse(const std::string& data);
	//int64_t parse(const std::string& data, size_t offset);
	int64_t parse(const char* data, size_t size);
	std::string to_string()const;

	virtual void reset();

	bool is_request()const;

	const std::string& method()const { return filed1; }
	void set_method(const std::string& method) { filed1 = method; }

	const std::string& url()const { return filed2; }
	void set_url(const std::string& url) { filed2 = url; }

	const std::string& request_version()const { return filed3; }
	void set_request_version(const std::string& version) { filed3 = version; }






	const std::string& response_version()const { return filed1; }
	void set_response_version(const std::string& version) { filed1 = version; }
	
	const std::string& status()const { return filed2; }
	void set_status(const std::string& status) { filed2 = status; }

	const std::string& message()const { return filed3; }
	void set_message(const std::string& message) { filed3 = message; }

	bool match_method(const std::string& method)const;
public:
	std::string filed1;
	std::string filed2;
	std::string filed3;

	base_headers headers;
	std::string body;
};

