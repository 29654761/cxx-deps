#include "blacklist.h"
#include <sys2/util.h>


blacklist::blacklist(asio::io_service& ios, int max_times, int times_duration, int prohibited_duration)
	:ios_(ios), timer_(ios), active_(false)
{
	max_times_ = max_times;
	times_duration_ = times_duration;
	prohibited_duration_ = prohibited_duration;
}

blacklist::~blacklist()
{
}

bool blacklist::start()
{
	bool expected = false;
	if (!active_.compare_exchange_strong(expected, true))
		return false;

	items_.clear();

	auto self = shared_from_this();
	timer_.expires_after(std::chrono::seconds(10));
	timer_.async_wait(std::bind(&blacklist::handle_timer, self, std::placeholders::_1));
	return true;
}

void blacklist::stop()
{
	bool expected = true;
	if (!active_.compare_exchange_strong(expected, false))
		return;

	std::error_code ec;
	timer_.cancel(ec);
}

bool blacklist::can_pass(const std::string& address)const
{
	if (log_)
	{
		log_->debug("Blacklist can_pass addr={}", address)->flush();
	}
	std::lock_guard<std::mutex> lock(mutex_);
	auto itr = items_.find(address);
	if (itr == items_.end())
		return true;

	if (itr->second.times < max_times_)
		return true;

	return false;
}

void blacklist::set_address(const std::string& address)
{
	if (log_)
	{
		log_->debug("Blacklist set_address addr={}", address)->flush();
	}
	item_t item;
	item.address = address;
	item.updated_at = sys::util::cur_time_ms();

	std::lock_guard<std::mutex> lock(mutex_);
	auto r = items_.insert(std::make_pair(address, item));
	if (!r.second)
	{
		r.first->second.updated_at = item.updated_at;
		r.first->second.times++;
	}
}

void blacklist::remove_address(const std::string& address)
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		items_.erase(address);
	}
	if (log_)
	{
		log_->debug("Blacklist remove_address: address={}", address)->flush();
	}
}

void blacklist::clear_address()
{
	std::lock_guard<std::mutex> lock(mutex_);
	items_.clear();
}

size_t blacklist::count_address()const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return items_.size();
}

std::vector<blacklist::item_t> blacklist::all_addresses()const
{
	std::vector<blacklist::item_t> rs;
	std::lock_guard<std::mutex> lock(mutex_);
	rs.reserve(items_.size());
	for (auto itr = items_.begin(); itr != items_.end(); itr++)
	{
		rs.push_back(itr->second);
	}
	return rs;
}

void blacklist::handle_timer(const std::error_code& ec)
{
	if (!active_ || ec)
		return;

	int64_t now = sys::util::cur_time_ms();
	auto items = all_addresses();
	for (auto itr = items.begin(); itr != items.end(); itr++)
	{
		int64_t sec = (now - itr->updated_at) / 1000;
		if (itr->times < max_times_)
		{
			if (sec >= times_duration_)
			{
				remove_address(itr->address);
			}
		}
		else
		{
			if (sec >= prohibited_duration_)
			{
				remove_address(itr->address);
			}
		}
	}

	//items = all_addresses();
	if (items.size() > 0)
	{
		if (log_)
		{
			std::stringstream ss;
			
			for (auto itr = items.begin(); itr != items.end(); itr++)
			{
				ss << itr->address << ", ";
			}
			log_->debug("Blacklist count={},addrs={}", items.size(), ss.str())->flush();
		}
	}

	auto self = shared_from_this();
	timer_.expires_after(std::chrono::seconds(10));
	timer_.async_wait(std::bind(&blacklist::handle_timer, self, std::placeholders::_1));
}


