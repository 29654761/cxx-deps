#pragma once

#include <mutex>
#include <memory>

namespace rtpx
{

	class port_range
	{
	public:
		port_range(uint16_t min_port = 40000, uint16_t max_port = 49999);
		~port_range();

		uint16_t get_next_port();
		uint16_t get_next_even_port();

		uint16_t min_port()const { return min_port_; }
		uint16_t max_port()const { return max_port_; }

		void reset(uint16_t min_port = 40000, uint16_t max_port = 49999);

		int count()const { return max_port_ - min_port_; }

		uint16_t random_even_port();
	private:
		std::recursive_mutex mutex_;
		uint16_t min_port_;
		uint16_t max_port_;
		uint16_t pos_;
	};

	typedef std::shared_ptr<port_range> port_range_ptr;

}
