#pragma once

#ifdef HYPERTEXT_CURL



#include "http_request.h"
#include "http_response.h"
#include <curl/curl.h>
#include <spdlog/spdlogger.hpp>

class http_client
{
public:
	http_client(spdlogger_ptr log=nullptr);
	~http_client();

	static bool global_init();
	static void global_cleanup();

	bool request(const http_request& request, http_response& response);

private:
	static size_t s_write_data(void* ptr, size_t size, size_t nmemb, void* stream);

	bool config_curl(const http_request& request);
private:
	spdlogger_ptr log_;
	CURL* curl_ = nullptr;
};

#endif // HYPERTEXT_CURL

