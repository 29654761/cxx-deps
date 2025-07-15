#include "update_time.h"
#include <chrono>

namespace sys
{
	update_time::update_time()
	{
		update();
	}

	update_time::~update_time()
	{
	}

	void update_time::update()
	{
		auto tp = std::chrono::system_clock::now();
		auto ms=std::chrono::time_point_cast<std::chrono::milliseconds>(tp);
		ts_ = ms.time_since_epoch().count();
	}

	bool update_time::is_timeout(int64_t timeout)const
	{
		auto tp = std::chrono::system_clock::now();
		auto ms = std::chrono::time_point_cast<std::chrono::milliseconds>(tp);
		int64_t now = ms.time_since_epoch().count();
		int64_t span = now - ts_;
		return span >= timeout;
	}
}