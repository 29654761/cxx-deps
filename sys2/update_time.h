#pragma once
#include <atomic>

namespace sys
{
	class update_time
	{
	public:
		update_time();
		~update_time();

		void update();
		bool is_timeout(int64_t timeout)const;
		void disable() { ts_ = 0; }
	private:
		std::atomic<int64_t> ts_;
	};

}