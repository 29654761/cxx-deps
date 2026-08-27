#pragma once
#include <cstdint>


namespace sys
{
	// 采集本机系统资源使用率的工具类，支持 Windows 与 Linux。
	// 所有接口返回百分比 (0.0 ~ 100.0)，采集失败或平台不支持时返回 -1.0f。
	class system_info
	{
	public:
		system_info();
		~system_info();

		// CPU 总体使用率。返回的是自上一次调用以来这段时间内的平均使用率，
		// 首次调用只做基准采样，返回 0.0f。
		float get_cpu_usage();

		// GPU 使用率（best-effort，依赖 nvidia-smi），无 NVIDIA 显卡或取不到时返回 -1.0f。
		float get_gpu_usage();

		// 物理内存使用率。
		float get_memory_usage();

		// 系统盘（Windows 为系统盘所在卷，Linux 为 "/"）使用率。
		float get_disk_usage();

	private:
		// 用于计算 CPU 使用率增量的上一次采样值。
		uint64_t last_total_ = 0;
		uint64_t last_idle_ = 0;
		bool cpu_inited_ = false;
	};

}
