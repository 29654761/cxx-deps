#pragma once
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif __linux__
#include <spawn.h>
#include <sys/wait.h>
#endif

namespace sys
{
	class process
	{
	public:
		process();
		~process();

        static int64_t run(const std::string& cmd, std::string* output=nullptr);

		bool start(const std::string& cmd, const std::string& args);
		void stop();


	private:
#ifdef _WIN32
        static int64_t run_win(const std::string& cmd, std::string* output);

		bool start_win(const std::string& cmd, const std::string& args);
		void stop_win();
#elif __linux__
		static int64_t run_linux(const std::string& cmd, std::string* output);

		bool start_linux(const std::string& cmd, const std::string& args);
		void stop_linux();
#endif

	private:

#ifdef _WIN32
		PROCESS_INFORMATION pi_;
#elif __linux__
		pid_t pid_=-1;
#endif
	};
}