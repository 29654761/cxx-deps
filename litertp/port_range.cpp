#include "port_range.h"
#include <sys2/util.h>

namespace litertp
{
	port_range::port_range(uint16_t min_port, uint16_t max_port)
	{
		min_port_ = min_port;
		max_port_ = max_port;
		pos_ = min_port_;
	}

	port_range::~port_range()
	{
	}

	uint16_t port_range::get_next_port()
	{
		std::lock_guard<std::recursive_mutex> lk(mutex_);

		uint16_t port = pos_++;
		if (pos_ > max_port_)
		{
			pos_ = min_port_;
		}

		return port;
	}

	uint16_t port_range::get_next_even_port()
	{
		std::lock_guard<std::recursive_mutex> lk(mutex_);

		uint16_t port = 0;
		for (int i=0;i<2;i++)
		{
			port = pos_++;
			if ((port & 1) == 0)
			{
				pos_++;

				if (pos_ > max_port_)
				{
					pos_ = min_port_;
				}
				break;
			}

			if (pos_ > max_port_)
			{
				pos_ = min_port_;
			}
		}

		

		return port;
	}

	uint16_t port_range::random_even_port()
	{
		std::lock_guard<std::recursive_mutex> lk(mutex_);
		uint16_t port = sys::util::random_number(min_port_, max_port_);
		port = port / 2 * 2;
		pos_ = port + 2;
		return port;
	}
}

