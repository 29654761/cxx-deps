
#include "http_client.h"
#include <sys2/string_util.h>

#ifdef HYPERTEXT_CURL

http_client::http_client()
{
	curl_ = curl_easy_init();
}

http_client::~http_client()
{
	if (curl_)
	{
		curl_easy_cleanup(curl_);
		curl_ = nullptr;
	}
}


bool http_client::global_init()
{
	CURLcode r = curl_global_init(CURL_GLOBAL_ALL);
	return r == CURLcode::CURLE_OK;
}

void http_client::global_cleanup()
{
	curl_global_cleanup();
}


bool http_client::request(const http_request& request, http_response& response)
{
	if(!config_curl(request))
	{
		return false;
	}
	curl_slist* headers = nullptr;
	for (auto itr = request.headers.items.begin(); itr != request.headers.items.end(); itr++)
	{
		headers = curl_slist_append(headers, (itr->name + ": " + itr->value).c_str());
	}
	curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);

	std::string rsp;
	curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, http_client::s_write_data);
	curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &rsp);
	curl_easy_setopt(curl_, CURLOPT_HEADER, 1L);


	CURLcode rc = curl_easy_perform(curl_);

	//long http_code = 0;
	//curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &http_code);

	curl_slist_free_all(headers);
	if (rc != CURLcode::CURLE_OK) {
		return false;
	}

	if(!response.parse(rsp))
	{
		return false;
	}

	return true;
}



size_t http_client::s_write_data(void* ptr, size_t size, size_t nmemb, void* stream)
{
	std::string* r = (std::string*)stream;
	r->append((const char*)ptr, size * nmemb);
	return size * nmemb;
}

bool http_client::config_curl(const http_request& request)
{
	if (!curl_) {
		return false;
	}
	curl_easy_reset(curl_);

	if (sys::string_util::icasecompare(request.method(), "POST"))
	{
		curl_easy_setopt(curl_, CURLOPT_POST, 1L);

		curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, request.body.c_str());
		curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, request.body.size());
	}
	curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 5L);
	curl_easy_setopt(curl_, CURLOPT_AUTOREFERER, 1L);
	curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl_, CURLOPT_MAXREDIRS, 3L);

	curl_easy_setopt(curl_, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(curl_, CURLOPT_TCP_KEEPIDLE, 120L);
	curl_easy_setopt(curl_, CURLOPT_TCP_KEEPINTVL, 20L);

	curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);

	std::string url = request.url();
	curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());


	curl_easy_setopt(curl_, CURLOPT_FORBID_REUSE, 0L);
	return true;
}

#endif // HYPERTEXT_CURL

