#pragma once
#include <cstdint>
#include <chrono>

namespace sys
{
	// 系统负载采集：为服务心跳/监控上报提供 CPU、内存使用率，同时给出两个视角——
	//
	//   - 本服务(cgroup)视角：回答"这个服务自己吃了多少资源"。CPU 用 cgroup v2 cpu.stat
	//     的 usage_usec 增量；内存用 memory.current - inactive_file(对齐 docker stats，
	//     排除可随时回收的文件页缓存——录音/日志类服务持续写文件会让 current 一路虚高)。
	//     分母有配额用配额(cpu.max / memory.max)，无配额用宿主总量(核数 / MemTotal)，
	//     即无配额时数值 = 本服务占整机资源的比例。
	//   - 宿主整机视角：回答"这台服务器现在整体多忙"。CPU 用 /proc/stat busy 增量，
	//     内存用 (1 - MemAvailable/MemTotal)。容器内读到的 /proc 就是宿主的(未挂 lxcfs)，
	//     语义正好符合。
	//
	// 平台支持：仅 Linux(cgroup v2 + procfs)。非 Linux(Windows/macOS 等)读不到这些文件，
	// 所有值恒为 0；裸机/cgroup v1 时本服务视角读不到 cgroup，退回整机值作近似
	// (单机单服务部署下可接受)。
	//
	// 实现追求"够用 + 零外部依赖"。

	// 一次负载采样的结果：使用率(0~100) + 容量。
	// 需要绝对用量时用容量换算：内存用量字节 ≈ mem_usage/100 × mem_limit_bytes。
	struct sysload
	{
		double   cpu_usage = 0;             // 本服务(cgroup) CPU 使用率
		double   mem_usage = 0;             // 本服务内存使用率
		double   host_cpu_usage = 0;        // 宿主整机 CPU 使用率
		double   host_mem_usage = 0;        // 宿主整机内存使用率

		// 容量(即上面各使用率的分母)
		double   cpu_cores = 0;             // 本服务可用核数：cgroup 配额核数(可为小数)，无配额=宿主逻辑核数
		uint64_t mem_limit_bytes = 0;       // 本服务内存上限：cgroup 配额，无配额=宿主总内存
		int      host_cpu_cores = 0;        // 宿主逻辑核数(非 Linux 也有值)
		uint64_t host_mem_total_bytes = 0;  // 宿主总内存(非 Linux 为 0)
	};

	// 记录上次采样的累计计数，用于算区间增量。默认构造即可用。
	// 增量算法决定了首次 sample() 的 CPU 恒为 0(没有基准区间)，这是预期行为。
	// 非并发安全：应由单一线程(通常是心跳循环)持有并串行调用。
	class sysload_sampler
	{
	public:
		sysload_sampler();
		~sysload_sampler();

		// 采样并返回本服务与宿主整机两个视角的 CPU / 内存使用率及容量。
		sysload sample();

	private:
		// 采本 cgroup 的 CPU 使用率：usage_usec 增量 / (区间时长 × 可用核数)。
		bool sample_cgroup_cpu(std::chrono::steady_clock::time_point now, double cores, double& out);
		// 采宿主整机 CPU busy 占比(/proc/stat 首行 jiffies 增量)。
		bool sample_host_cpu(double& out);

	private:
		bool has_last_ = false;                          // 对应 Go 的 !prevAt.IsZero()：是否已有上次采样基准
		std::chrono::steady_clock::time_point last_sample_at_;
		uint64_t last_cgroup_usage_usec_ = 0;            // cgroup v2 cpu.stat 的 usage_usec 累计值
		uint64_t last_proc_busy_ = 0;                    // /proc/stat 非 idle jiffies 累计值
		uint64_t last_proc_total_ = 0;                   // /proc/stat 总 jiffies 累计值
	};
}
