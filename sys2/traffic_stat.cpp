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
		cur_bytes_ = 0;
		total_bytes_ = 0;
	}

	uint64_t traffic_stat::total_bytes() const
	{
		return total_bytes_.load();
	}

	uint64_t traffic_stat::cur_bytes()
	{
		return cur_bytes_.exchange(0);
	}

	uint64_t traffic_stat::add_bytes(uint64_t bytes)
	{
		total_bytes_.fetch_add(bytes);
		return cur_bytes_.fetch_add(bytes);
	}

}
