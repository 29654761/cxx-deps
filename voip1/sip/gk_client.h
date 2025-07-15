#pragma once
#include <string>
#include <spdlog/spdlogger.hpp>
#include <sys2/callback.hpp>
#include <sys2/signal.h>
#include <hypertext/sip/sip_session.h>
#include <hypertext/sip/challenge.h>
#include <hypertext/sip/credentials.h>


namespace voip
{
	namespace sip
	{
		class endpoint;

		class gk_client
		{
		public:
			typedef void (*on_status_t)(void* ctx, bool status);

			gk_client(endpoint& ep);
			~gk_client();

			void set_nat_address(const std::string& address) { nat_address_ = address; }
			void set_logger(spdlogger_ptr log) { log_ = log; }
			void set_alias(const std::string& alias,const std::string& username, const std::string& password) { 
				alias_ = alias;
				username_ = username;
				password_ = password;
			}
			bool status()const { return is_registered_; }

			bool start(const std::string& addr, int port,bool is_tcp,int interval=30);
			void stop();

			uint32_t get_cseq();

			sip_session_ptr get_session()const;
			void set_session(sip_session_ptr sess);
		public:
			
			void on_register_rsp(sip_session_ptr sess,const sip_message& msg);
			
		private:
			bool send_register(const credentials* cred);
			bool send_unregister();
		private:
			void set_status(bool status);
			void run_timer();

			void set_credentials(const credentials& cred);
			bool get_credentials(credentials& cred);
			void clear_credentials();
		public:
			sys::callback<on_status_t> on_status;
		private:
			mutable std::recursive_mutex mutex_;
			spdlogger_ptr log_;
			endpoint& ep_;
			sip_session_ptr sip_;
			bool active_ = false;
			std::shared_ptr<std::thread> timer_;
			sys::signal signal_;
			std::atomic<int> interval_;
			int time_to_live_ = 60;

			voip_uri url_;
			bool is_tcp_ = true;

			std::atomic<int64_t> updated_at_;
			bool is_registered_ = false;

			std::string alias_;
			std::string username_;
			std::string password_;
			std::string nat_address_;

			std::atomic<uint32_t> cseq_base_;
			std::string my_tag_;
			credentials cred_;
			bool has_cred_ = false;
		};


	}
}

