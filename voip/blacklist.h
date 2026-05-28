#pragma once
#include <asio.hpp>
#include <map>
#include <spdlog/spdlogger.hpp>


class blacklist :public std::enable_shared_from_this<blacklist>
{
public:
	struct item_t
	{
		std::string address;
		int64_t updated_at = 0;
		int times = 0;
	};

	blacklist(asio::io_service& ios, int max_times = 5, int times_duration = 30, int prohibited_duration = 3600);
	~blacklist();

	inline void set_logger(spdlogger_ptr log) { log_ = log; }

	bool start();
	void stop();

	bool can_pass(const std::string& address)const;
	void set_address(const std::string& address);
	void remove_address(const std::string& address);
	void clear_address();
	size_t count_address()const;
	std::vector<item_t> all_addresses()const;
private:
	void handle_timer(const std::error_code& ec);
private:
	std::atomic_bool active_;
	asio::io_service& ios_;
	asio::steady_timer timer_;
	spdlogger_ptr log_;

	mutable std::mutex mutex_;
	std::map<std::string, item_t> items_;

	int max_times_ = 5;
	int times_duration_ = 30;
	int prohibited_duration_ = 24 * 3600;
};

typedef std::shared_ptr<blacklist> blacklist_ptr;


