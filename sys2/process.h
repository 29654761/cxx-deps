#pragma once
#include <string>
namespace sys
{
	class process
	{
	public:
        static int64_t run(const std::string& cmd, std::string* output=nullptr);


	private:
#ifdef _WIN32
        static int64_t run_win(const std::string& cmd, std::string* output);
#elif __linux__
		static int64_t run_linux(const std::string& cmd, std::string* output);
#endif

	};
}