/**
 * @file hardware.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#pragma once

#include <string>
#include <set>



#ifdef _WIN32
#define _WIN32_DCOM
#include <Wbemidl.h>
#include <comdef.h>
#endif

namespace sys
{
	class hardware
	{
	public:
		hardware();
		~hardware();

		bool init();
		void cleanup();

		std::string get_board_serial_number();
		std::string get_cpu_id();
		std::string get_disk_id();
		std::set<std::string> get_mac_addresses();

		std::string get_machine_id();
	private:
#ifdef _WIN32
		bool get_os_disk(int& num);
#endif
	private:

#ifdef _WIN32
		IWbemLocator* loc_ = nullptr;
		IWbemServices* svc_ = nullptr;
#endif
	};
}
