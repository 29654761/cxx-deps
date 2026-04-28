#include "traffic_stat.h"


namespace sys {

	traffic_stat::traffic_stat()
	{
		reset();
	}

	traffic_stat::~traffic_stat()
	{

	}

	void traffic_stat::reset()
	{
		cur_ = 0;
		total_ = 0;
	}

	uint64_t traffic_stat::total() const
	{
		return total_.load();
	}

	uint64_t traffic_stat::current()
	{
		return cur_.exchange(0);
	}

	uint64_t traffic_stat::add(uint64_t val)
	{
		total_.fetch_add(val);
		return cur_.fetch_add(val);
	}

}
