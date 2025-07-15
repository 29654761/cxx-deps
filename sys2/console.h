#pragma once
#include <sys2/signal.h>

namespace sys
{
	class console
	{
	public:
		
		static void wait();
		static void notify();
		static sys::signal& signal();

	private:
		static sys::signal signal_;
	};

}

