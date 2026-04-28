#pragma once
#include <atomic>
#include <stdint.h>

namespace sys {

	class traffic_stat
	{
	public:
		traffic_stat();
		~traffic_stat();

		void reset();

		uint64_t total() const;
		uint64_t current();

		uint64_t add(uint64_t val);
	private:
		std::atomic<uint64_t> cur_;
		std::atomic<uint64_t> total_;
	};

}
