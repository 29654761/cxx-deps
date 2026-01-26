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

		uint64_t total_bytes() const;
		uint64_t cur_bytes();

		uint64_t add_bytes(uint64_t bytes);
	private:
		std::atomic<uint64_t> cur_bytes_;
		std::atomic<uint64_t> total_bytes_;
	};

}
