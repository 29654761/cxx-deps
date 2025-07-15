/**
 * @file global.h
 * @brief Global resources for library.
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#pragma once

#include "dtls/dtls.h"

#include <mutex>

namespace litertp
{
	class global
	{
	private:
		global();
		~global();
		static global* s_instance;
		bool init();
		void cleanup();
	public:
		static global* instance();
		static void release();



		cert_ptr get_cert();
		dtls_ptr get_dtls();

	private:
#ifdef LITERTP_SSL
		std::recursive_mutex cert_mutex_;
		cert_ptr cert_;
#endif
	};


}