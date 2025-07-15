#pragma once
#include <string>
#include <spdlog/spdlogger.hpp>
#include <sys2/signal.h>
#include <sip/sip_connection.h>
#include <sip/challenge.h>
#include <sip/credentials.h>
#include <functional>

namespace voip
{
	namespace sip
	{
		class endpoint;
		typedef std::shared_ptr<endpoint> endpoint_ptr;

		class gk_client;
		typedef std::shared_ptr<gk_client> gk_client_ptr;

		class gk_client:public std::enable_shared_from_this<gk_client>
		{
		public:
			typedef std::function<void(gk_client_ptr, bool)> status_handler_t;

			gk_client(endpoint_ptr ep);
			~gk_client();

			void set_nat_address(const std::string& address) { nat_address_ = address; }
			void set_logger(spdlogger_ptr log) { log_ = log; }
			void set_alias(const std::string& alias,const std::string& username, const std::string& password) { 
				alias_ = alias;
				username_ = username;
				password_ = password;
			}
			bool status()const { return is_registered_; }
			void set_status_handler(status_handler_t handler) { status_handler_ = handler; }

			bool start(const std::string& addr, int port,bool is_tcp,int interval=30);
			void stop();

			uint32_t get_cseq();
			sip_connection_ptr get_con()const;

		public:
			
			void on_register_rsp(sip_connection_ptr con,const sip_message& msg);
			
		private:
			bool send_register(const credentials* cred);
			bool send_unregister();
		private:
			void set_status(bool status);
			void set_credentials(const credentials& cred);
			bool get_credentials(credentials& cred);
			void clear_credentials();

			void set_con(sip_connection_ptr con);
			
		private:
			void handle_timer(gk_client_ptr gk,const std::error_code& ec);
		private:
			bool active_ = false;

			mutable std::mutex mutex_;
			sip_connection_ptr con_;
			endpoint_ptr ep_;
			asio::steady_timer timer_;
			status_handler_t status_handler_;

			spdlogger_ptr log_;

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

