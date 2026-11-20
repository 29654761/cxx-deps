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

	// 设置超时（秒）：connect_sec 为建连超时，total_sec 为整个请求的总超时（0 表示不限）。
	// 默认 connect=30、total=0（不限总时长，仅靠 30 秒低速判定兜底），大文件上传等长请求保持默认即可；
	// 心跳等控制面请求应设较短的总超时，避免对端挂住（如进程被暂停、TCP 已建连却不回包）时无限期阻塞调用方
	void set_timeout(long connect_sec, long total_sec) { connect_timeout_ = connect_sec; total_timeout_ = total_sec; }

private:
	static size_t s_write_data(void* ptr, size_t size, size_t nmemb, void* stream);

	bool config_curl(const http_request& request);
private:
	spdlogger_ptr log_;
	CURL* curl_ = nullptr;
	long connect_timeout_ = 30;	// 建连超时（秒）
	long total_timeout_ = 0;	// 请求总超时（秒），0 表示不限
};

#endif // HYPERTEXT_CURL

