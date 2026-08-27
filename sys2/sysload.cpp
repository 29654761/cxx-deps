#include "sysload.h"

#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <thread>
#include <fstream>
#include <sstream>

namespace sys
{
	namespace
	{
		// cgroup v2 / procfs 文件路径。仅 Linux 存在，其他平台读取失败即退化为 0。
		const char* const kCgroupCpuStatPath = "/sys/fs/cgroup/cpu.stat";
		const char* const kCgroupCpuMaxPath  = "/sys/fs/cgroup/cpu.max";
		const char* const kCgroupMemCurPath  = "/sys/fs/cgroup/memory.current";
		const char* const kCgroupMemMaxPath  = "/sys/fs/cgroup/memory.max";
		const char* const kCgroupMemStatPath = "/sys/fs/cgroup/memory.stat";
		const char* const kProcStatPath      = "/proc/stat";
		const char* const kProcMeminfoPath   = "/proc/meminfo";

		double clamp_pct(double v)
		{
			if (v < 0) return 0;
			if (v > 100) return 100;
			return v;
		}

		// 一次性把整个文件读进字符串；文件不存在/打不开返回 false。
		bool read_file(const char* path, std::string& out)
		{
			std::ifstream fs(path, std::ios::binary);
			if (!fs.is_open())
				return false;
			std::ostringstream ss;
			ss << fs.rdbuf();
			out = ss.str();
			return true;
		}

		// 解析无符号整数，成功返回 true。对应 Go 的 strconv.ParseUint。
		bool parse_u64(const std::string& s, uint64_t& out)
		{
			if (s.empty())
				return false;
			errno = 0;
			char* end = nullptr;
			unsigned long long v = strtoull(s.c_str(), &end, 10);
			if (errno != 0 || end == s.c_str() || *end != '\0')
				return false;
			out = static_cast<uint64_t>(v);
			return true;
		}

		// 从 cgroup v2 cpu.stat 内容里取 usage_usec 累计值。
		bool parse_cgroup_usage_usec(const std::string& data, uint64_t& out)
		{
			std::istringstream ss(data);
			std::string line;
			while (std::getline(ss, line))
			{
				std::istringstream ls(line);
				std::string key, val;
				if ((ls >> key >> val) && key == "usage_usec")
				{
					std::string extra;
					if (ls >> extra) // 恰好两个字段才算(对齐 Go 的 len(fields)==2)
						return false;
					return parse_u64(val, out);
				}
			}
			return false;
		}

		// 解析 cgroup v2 cpu.max("$MAX $PERIOD" 或 "max $PERIOD")为配额核数。
		// 返回 false 表示无配额或格式异常，调用方应退回宿主核数。
		bool parse_cgroup_cpu_max(const std::string& data, double& cores)
		{
			std::istringstream ss(data);
			std::string quota_s, period_s, extra;
			if (!(ss >> quota_s >> period_s))
				return false;
			if (ss >> extra) // 多于两个字段视为异常
				return false;
			if (quota_s == "max")
				return false;
			char* e1 = nullptr;
			char* e2 = nullptr;
			double quota = strtod(quota_s.c_str(), &e1);
			double period = strtod(period_s.c_str(), &e2);
			if (e1 == quota_s.c_str() || *e1 != '\0' ||
				e2 == period_s.c_str() || *e2 != '\0' ||
				quota <= 0 || period <= 0)
				return false;
			cores = quota / period;
			return true;
		}

		// 解析 /proc/stat 首行(cpu 汇总行)，返回非 idle 与总 jiffies。
		// idle 取 idle+iowait(iowait 期间 CPU 实际空闲，算 busy 会高估)。
		bool parse_proc_stat_busy_total(const std::string& data, uint64_t& busy, uint64_t& total)
		{
			std::istringstream ss(data);
			std::string line;
			if (!std::getline(ss, line))
				return false;
			std::istringstream ls(line);
			std::string label;
			if (!(ls >> label) || label != "cpu")
				return false;
			// cpu user nice system idle iowait irq softirq steal [guest guest_nice]
			uint64_t vals[16];
			int n = 0;
			std::string f;
			while (n < 16 && (ls >> f))
			{
				if (!parse_u64(f, vals[n]))
					return false;
				++n;
			}
			if (n < 8) // 至少 user..steal 共 8 项
				return false;
			total = 0;
			for (int i = 0; i < n; i++)
				total += vals[i];
			uint64_t idle = vals[3] + vals[4]; // idle + iowait
			busy = total - idle;
			return true;
		}

		// 解析 memory.max 为配额字节数；"max"(无配额)时返回 false，调用方应改用宿主总内存作分母。
		bool parse_cgroup_mem_max_bytes(const std::string& data, uint64_t& out)
		{
			std::string s = data;
			// 去掉首尾空白
			size_t b = s.find_first_not_of(" \t\r\n");
			size_t e = s.find_last_not_of(" \t\r\n");
			if (b == std::string::npos)
				return false;
			s = s.substr(b, e - b + 1);
			if (s == "max")
				return false;
			uint64_t v = 0;
			if (!parse_u64(s, v) || v == 0)
				return false;
			out = v;
			return true;
		}

		// 从 memory.stat 内容里取 inactive_file 字节数；没有该行返回 0。
		uint64_t parse_cgroup_mem_stat_inactive_file(const std::string& data)
		{
			std::istringstream ss(data);
			std::string line;
			while (std::getline(ss, line))
			{
				std::istringstream ls(line);
				std::string key, val;
				if ((ls >> key >> val) && key == "inactive_file")
				{
					std::string extra;
					if (ls >> extra)
						return 0;
					uint64_t v = 0;
					return parse_u64(val, v) ? v : 0;
				}
			}
			return 0;
		}

		// 从 /proc/meminfo 取 MemTotal(kB)。
		bool parse_meminfo_total_kb(const std::string& data, uint64_t& out)
		{
			std::istringstream ss(data);
			std::string line;
			while (std::getline(ss, line))
			{
				std::istringstream ls(line);
				std::string key, val;
				if ((ls >> key >> val) && key == "MemTotal:")
				{
					uint64_t v = 0;
					if (parse_u64(val, v) && v > 0)
					{
						out = v;
						return true;
					}
					return false;
				}
			}
			return false;
		}

		// 从 /proc/meminfo 算 (1 - MemAvailable/MemTotal) × 100。
		bool parse_meminfo_used_pct(const std::string& data, double& out)
		{
			uint64_t total_kb = 0, avail_kb = 0;
			std::istringstream ss(data);
			std::string line;
			while (std::getline(ss, line))
			{
				std::istringstream ls(line);
				std::string key, val;
				if (!(ls >> key >> val))
					continue;
				if (key == "MemTotal:")
					parse_u64(val, total_kb);
				else if (key == "MemAvailable:")
					parse_u64(val, avail_kb);
				if (total_kb > 0 && avail_kb > 0)
					break;
			}
			if (total_kb == 0 || avail_kb == 0)
				return false;
			out = clamp_pct((1.0 - static_cast<double>(avail_kb) / static_cast<double>(total_kb)) * 100.0);
			return true;
		}

		// 读本 cgroup 的实际内存占用：memory.current - inactive_file。
		// 对齐 docker stats 口径；memory.stat 读不到时不减(宁可略偏高，不虚报为 0)。
		bool cgroup_mem_used_bytes(double& out)
		{
			std::string cur;
			if (!read_file(kCgroupMemCurPath, cur))
				return false;
			// 去首尾空白
			size_t b = cur.find_first_not_of(" \t\r\n");
			size_t e = cur.find_last_not_of(" \t\r\n");
			if (b == std::string::npos)
				return false;
			uint64_t cur_v = 0;
			if (!parse_u64(cur.substr(b, e - b + 1), cur_v))
				return false;
			uint64_t inactive_file = 0;
			std::string stat;
			if (read_file(kCgroupMemStatPath, stat))
				inactive_file = parse_cgroup_mem_stat_inactive_file(stat);
			if (inactive_file > cur_v)
				inactive_file = cur_v;
			out = static_cast<double>(cur_v - inactive_file);
			return true;
		}

		// 采宿主整机内存使用率 (1 - MemAvailable/MemTotal) × 100。
		double sample_host_memory()
		{
			std::string data;
			if (read_file(kProcMeminfoPath, data))
			{
				double pct = 0;
				if (parse_meminfo_used_pct(data, pct))
					return pct;
			}
			return 0;
		}
	} // namespace

	sysload_sampler::sysload_sampler()
	{
	}

	sysload_sampler::~sysload_sampler()
	{
	}

	sysload sysload_sampler::sample()
	{
		auto now = std::chrono::steady_clock::now();
		sysload load;

		// 先定容量(各使用率的分母)，用量都基于它换算
		load.host_cpu_cores = static_cast<int>(std::thread::hardware_concurrency());
		load.cpu_cores = static_cast<double>(load.host_cpu_cores);
		std::string data;
		if (read_file(kCgroupCpuMaxPath, data))
		{
			double cores = 0;
			if (parse_cgroup_cpu_max(data, cores))
				load.cpu_cores = cores;
		}
		if (read_file(kProcMeminfoPath, data))
		{
			uint64_t total_kb = 0;
			if (parse_meminfo_total_kb(data, total_kb))
				load.host_mem_total_bytes = total_kb * 1024ull;
		}
		load.mem_limit_bytes = load.host_mem_total_bytes;
		if (read_file(kCgroupMemMaxPath, data))
		{
			uint64_t limit_bytes = 0;
			if (parse_cgroup_mem_max_bytes(data, limit_bytes))
				load.mem_limit_bytes = limit_bytes;
		}

		// 宿主整机用量
		double host_cpu = 0;
		bool host_cpu_ok = sample_host_cpu(host_cpu);
		load.host_cpu_usage = host_cpu;
		load.host_mem_usage = sample_host_memory();

		// 本服务用量；裸机/cgroup v1 拿不到 cgroup 视角时退回整机值作近似(单机单服务部署下可接受)
		double cpu = 0;
		if (sample_cgroup_cpu(now, load.cpu_cores, cpu))
			load.cpu_usage = cpu;
		else if (host_cpu_ok)
			load.cpu_usage = host_cpu;

		double used_bytes = 0;
		if (cgroup_mem_used_bytes(used_bytes) && load.mem_limit_bytes > 0)
			load.mem_usage = clamp_pct(used_bytes / static_cast<double>(load.mem_limit_bytes) * 100.0);
		else
			load.mem_usage = load.host_mem_usage;

		last_sample_at_ = now;
		has_last_ = true;
		return load;
	}

	bool sysload_sampler::sample_cgroup_cpu(std::chrono::steady_clock::time_point now, double cores, double& out)
	{
		std::string data;
		if (!read_file(kCgroupCpuStatPath, data) || cores <= 0)
			return false;
		uint64_t usage = 0;
		if (!parse_cgroup_usage_usec(data, usage))
			return false;
		uint64_t prev = last_cgroup_usage_usec_;
		auto prev_at = last_sample_at_;
		bool had_prev = has_last_;
		last_cgroup_usage_usec_ = usage;
		if (prev > 0 && usage >= prev && had_prev)
		{
			double elapsed_usec = static_cast<double>(
				std::chrono::duration_cast<std::chrono::microseconds>(now - prev_at).count());
			if (elapsed_usec > 0)
			{
				out = clamp_pct(static_cast<double>(usage - prev) / (elapsed_usec * cores) * 100.0);
				return true;
			}
		}
		out = 0; // 首次采样没有基准区间，报 0
		return true;
	}

	bool sysload_sampler::sample_host_cpu(double& out)
	{
		std::string data;
		if (!read_file(kProcStatPath, data))
			return false;
		uint64_t busy = 0, total = 0;
		if (!parse_proc_stat_busy_total(data, busy, total))
			return false;
		uint64_t prev_busy = last_proc_busy_;
		uint64_t prev_total = last_proc_total_;
		last_proc_busy_ = busy;
		last_proc_total_ = total;
		if (prev_total > 0 && total > prev_total && busy >= prev_busy)
		{
			out = clamp_pct(static_cast<double>(busy - prev_busy) /
				static_cast<double>(total - prev_total) * 100.0);
			return true;
		}
		out = 0; // 首次采样没有基准区间，报 0
		return true;
	}
}
