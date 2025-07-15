#include "console.h"
#include <sys2/signal.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif defined(__linux__)
#include <csignal>
#endif

sys::signal sys::console::signal_;

#ifdef _WIN32
BOOL WINAPI handler_routine(_In_ DWORD ctrl_type)
{
	if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_CLOSE_EVENT)
	{
		sys::console::notify();
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		return TRUE;
	}
	return FALSE;
}
#elif defined(__linux__)

void signal_handler(int signal)
{
	if (signal == SIGINT || signal == SIGQUIT || signal == SIGHUP||signal== SIGTERM)
	{
		sys::console::notify();
		//std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

#endif

namespace sys
{


	void console::wait()
	{
#ifdef _WIN32
		SetConsoleCtrlHandler(handler_routine,TRUE);
#elif defined(__linux__)
		::signal(SIGINT, signal_handler);
		::signal(SIGQUIT, signal_handler);
		::signal(SIGHUP, signal_handler);
		::signal(SIGTERM, signal_handler);
#endif

		signal_.wait();
	}

	void console::notify()
	{
		signal_.notify();
	}

	sys::signal& console::signal()
	{ 
		return signal_;
	}
}

