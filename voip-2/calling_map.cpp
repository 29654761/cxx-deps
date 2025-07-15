#include "calling_map.h"

namespace voip
{
	calling_map::calling_map()
	{
	}

	calling_map::~calling_map()
	{
	}

	bool calling_map::start()
	{
		if (active_)
			return false;
		active_ = true;
		signal_.reset();
		clear();
		timer_=std::async(std::launch::async, &calling_map::on_timer, this);
		return true;
	}

	void calling_map::stop()
	{
		active_ = false;
		signal_.notify();
		if (timer_.valid())
		{
			timer_.wait();
		}
		clear();
	}

	bool calling_map::add(const std::string& id, PSafePtr<OpalCall> call)
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);

		calling_item item;
		item.call = call;
		item.time = time(nullptr);
		auto r=map_.insert(std::make_pair(id,item));
		return r.second;
	}

	PSafePtr<OpalCall> calling_map::pop(const std::string& id)
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		auto itr=map_.find(id);
		if (itr == map_.end())
			return nullptr;
		PSafePtr<OpalCall> call = itr->second.call;
		map_.erase(itr);
		return call;
	}


	size_t calling_map::count()
	{
		std::shared_lock<std::shared_mutex> lk(mutex_);
		return map_.size();
	}

	std::vector<PSafePtr<OpalCall>> calling_map::all()
	{
		std::vector< PSafePtr<OpalCall>> rs;
		std::shared_lock<std::shared_mutex> lk(mutex_);
		rs.reserve(map_.size());
		for (auto itr = map_.begin(); itr != map_.end(); itr++)
		{
			rs.push_back(itr->second.call);
		}
		return rs;
	}

	void calling_map::clear_expired()
	{
		time_t now = time(nullptr);
		std::shared_lock<std::shared_mutex> lk(mutex_);
		for (auto itr = map_.begin(); itr != map_.end();)
		{
			time_t ts = now - itr->second.time;
			if (ts > expired_)
			{
				itr->second.call->Clear();
				itr=map_.erase(itr);
			}
			else
			{
				itr++;
			}
		}
	}

	void calling_map::clear()
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		map_.clear();
	}


	void calling_map::on_timer()
	{
		while (active_)
		{
			clear_expired();
			signal_.wait(3000);
		}
	}

}
