#include "system_info.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#pragma comment(lib, "kernel32.lib")
#else
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <fstream>
#include <string>
#include <sstream>
#endif


namespace sys
{

	system_info::system_info()
	{}

	system_info::~system_info()
	{}

#ifdef _WIN32

	static uint64_t filetime_to_u64(const FILETIME& ft)
	{
		ULARGE_INTEGER ul;
		ul.LowPart = ft.dwLowDateTime;
		ul.HighPart = ft.dwHighDateTime;
		return ul.QuadPart;
	}

	float system_info::get_cpu_usage()
	{
		FILETIME idle_ft, kernel_ft, user_ft;
		if (!GetSystemTimes(&idle_ft, &kernel_ft, &user_ft))
			return -1.0f;

		// kernel 时间已经包含了 idle 时间。
		uint64_t idle = filetime_to_u64(idle_ft);
		uint64_t total = filetime_to_u64(kernel_ft) + filetime_to_u64(user_ft);

		if (!cpu_inited_)
		{
			last_idle_ = idle;
			last_total_ = total;
			cpu_inited_ = true;
			return 0.0f;
		}

		uint64_t total_delta = total - last_total_;
		uint64_t idle_delta = idle - last_idle_;
		last_idle_ = idle;
		last_total_ = total;

		if (total_delta == 0)
			return 0.0f;

		float busy = static_cast<float>(total_delta - idle_delta);
		return busy * 100.0f / static_cast<float>(total_delta);
	}

	float system_info::get_memory_usage()
	{
		MEMORYSTATUSEX status;
		status.dwLength = sizeof(status);
		if (!GlobalMemoryStatusEx(&status))
			return -1.0f;
		return static_cast<float>(status.dwMemoryLoad);
	}

	float system_info::get_disk_usage()
	{
		// 取 Windows 系统盘所在卷。
		char win_dir[MAX_PATH] = { 0 };
		if (GetWindowsDirectoryA(win_dir, MAX_PATH) == 0)
			return -1.0f;
		// 仅保留盘符根目录，如 "C:\\"。
		char root[4] = { win_dir[0], ':', '\\', 0 };

		ULARGE_INTEGER avail, total, free_bytes;
		if (!GetDiskFreeSpaceExA(root, &avail, &total, &free_bytes))
			return -1.0f;
		if (total.QuadPart == 0)
			return 0.0f;

		uint64_t used = total.QuadPart - free_bytes.QuadPart;
		return static_cast<float>(used) * 100.0f / static_cast<float>(total.QuadPart);
	}

#else // Linux

	float system_info::get_cpu_usage()
	{
		std::ifstream fs("/proc/stat");
		if (!fs.is_open())
			return -1.0f;

		std::string cpu_label;
		uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0,
			irq = 0, softirq = 0, steal = 0;
		fs >> cpu_label >> user >> nice >> system >> idle >> iowait
			>> irq >> softirq >> steal;
		if (cpu_label != "cpu")
			return -1.0f;

		uint64_t idle_all = idle + iowait;
		uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;

		if (!cpu_inited_)
		{
			last_idle_ = idle_all;
			last_total_ = total;
			cpu_inited_ = true;
			return 0.0f;
		}

		uint64_t total_delta = total - last_total_;
		uint64_t idle_delta = idle_all - last_idle_;
		last_idle_ = idle_all;
		last_total_ = total;

		if (total_delta == 0)
			return 0.0f;

		float busy = static_cast<float>(total_delta - idle_delta);
		return busy * 100.0f / static_cast<float>(total_delta);
	}

	float system_info::get_memory_usage()
	{
		// 优先从 /proc/meminfo 读取 MemAvailable，更贴近实际可用内存。
		std::ifstream fs("/proc/meminfo");
		if (fs.is_open())
		{
			uint64_t mem_total = 0, mem_available = 0;
			std::string line;
			while (std::getline(fs, line))
			{
				std::istringstream ss(line);
				std::string key;
				uint64_t value = 0;
				std::string unit;
				ss >> key >> value >> unit;
				if (key == "MemTotal:")
					mem_total = value;
				else if (key == "MemAvailable:")
					mem_available = value;
				if (mem_total > 0 && mem_available > 0)
					break;
			}
			if (mem_total > 0)
			{
				uint64_t used = mem_total > mem_available ? mem_total - mem_available : 0;
				return static_cast<float>(used) * 100.0f / static_cast<float>(mem_total);
			}
		}

		// 回退到 sysinfo()。
		struct sysinfo info;
		if (::sysinfo(&info) != 0 || info.totalram == 0)
			return -1.0f;
		uint64_t total = info.totalram;
		uint64_t used = total - info.freeram - info.bufferram;
		return static_cast<float>(used) * 100.0f / static_cast<float>(total);
	}

	float system_info::get_disk_usage()
	{
		struct statvfs st;
		if (statvfs("/", &st) != 0)
			return -1.0f;

		uint64_t total = static_cast<uint64_t>(st.f_blocks) * st.f_frsize;
		uint64_t avail = static_cast<uint64_t>(st.f_bavail) * st.f_frsize;
		if (total == 0)
			return 0.0f;

		uint64_t used = total - static_cast<uint64_t>(st.f_bfree) * st.f_frsize;
		return static_cast<float>(used) * 100.0f / static_cast<float>(total);
	}

#endif

	// GPU 使用率：跨平台通过调用 nvidia-smi 采集（best-effort）。
	// 无 NVIDIA 驱动 / 未安装 nvidia-smi / 解析失败时返回 -1.0f。
	float system_info::get_gpu_usage()
	{
		const char* cmd =
			"nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits";

#ifdef _WIN32
		FILE* pipe = _popen(cmd, "r");
#else
		FILE* pipe = popen(cmd, "r");
#endif
		if (!pipe)
			return -1.0f;

		char buf[128] = { 0 };
		float total = 0.0f;
		int count = 0;
		while (fgets(buf, sizeof(buf), pipe) != nullptr)
		{
			// 每行是一块 GPU 的利用率数字（0~100）。
			char* end = nullptr;
			float v = strtof(buf, &end);
			if (end != buf)
			{
				total += v;
				++count;
			}
		}

#ifdef _WIN32
		_pclose(pipe);
#else
		pclose(pipe);
#endif

		if (count == 0)
			return -1.0f;
		// 多块 GPU 时取平均值。
		return total / count;
	}

}

