#pragma once
#include "h323_call.h"
#include "gk_client.h"

namespace voip
{
	namespace h323
	{
		class gk_server;
		class endpoint
		{
			friend gk_client;
		public:
			typedef void (*on_incoming_call_t)(void* ctx,voip::call_ptr call, const std::string& called_alias, const std::string& remote_alias,
				const std::string& remote_addr, int remote_port);
			typedef void (*on_gk_status_t)(void* ctx, bool status);
			endpoint();
			~endpoint();

			PString local_alias()const;
			void set_local_alias(const PString& alias);

			void set_nat_address(const PString& nat_address);
			PString nat_address()const;

			void set_h245_ports(litertp::port_range_ptr ports);
			litertp::port_range_ptr h245_ports()const;

			void set_rtp_ports(litertp::port_range_ptr ports);
			litertp::port_range_ptr rtp_ports()const;

			void set_max_bitrate(int v);
			int max_bitrate()const;

			void set_logger(spdlogger_ptr log) { log_ = log; }
			void set_incoming_media(bool audio, bool video) { incoming_audio_ = audio; incoming_video_ = video; }


			bool start(const std::string& addr,int port, int worknum);
			void stop();
			void notify_clear_call() { signal_.notify(); }

			bool start_gk_client(const PString& ras_address, WORD ras_port,const PString& alias, const PString& username,const PString& password);
			void stop_gk_client();
			bool is_gk_started()const;
			bool gk_status()const;

			call_ptr make_call(const std::string& url,const std::string& call_id="",const std::string& conf_id="",int call_ref=-1);
			call_ptr make_reg_server_call(const std::string& alias,gk_server& gkserver);
			call_ptr make_reg_client_call(const std::string& alias);
		private:
			static void on_accept_event(void* ctx, sys::async_socket_ptr socket, sys::async_socket_ptr newsocket, const sockaddr* addr, socklen_t addr_size);
			static void s_on_facility(void* ctx, call_ptr call, const PGloballyUniqueID& call_id, const PGloballyUniqueID& conf_id);
			static void s_on_gk_status(void* ctx, bool status);
			static void s_on_gk_incoming_call(void* ctx, const H225_ServiceControlIndication& body, H225_ServiceControlResponse_result::Choices& result);

			void run_timer();

			bool add_call(std::shared_ptr<h323_call> call);
			std::shared_ptr<h323_call> get_call(const std::string& call_id);
			bool remove_call(std::shared_ptr<h323_call> call);
			void all_calls(std::vector<std::shared_ptr<h323_call>>& vec);
			size_t count_calls();
			void clear_calls();

			void set_gkclient(std::shared_ptr<gk_client> gkclient);
			std::shared_ptr<gk_client> get_gkclient()const;
		private:
			
		public:
			sys::callback<on_incoming_call_t> on_incoming_call;
			sys::callback<on_gk_status_t> on_gk_status;
		private:
			mutable std::shared_mutex mutex_;
			bool active_ = false;
			WORD port_ = 1720;
			sys::async_socket_ptr skt_;
			sys::async_service ios_;
			PString alias_;
			PString nat_address_;

			std::shared_ptr<gk_client> gkclient_;

			std::shared_ptr<std::thread> timer_;
			sys::signal signal_;

			spdlogger_ptr log_;
			PStringArray media_crypto_suites_;
			litertp::port_range_ptr h245_ports_;
			litertp::port_range_ptr rtp_ports_;

			std::recursive_mutex calls_mutex_;
			std::vector<std::shared_ptr<h323_call>> calls_;

			bool incoming_audio_ = true;
			bool incoming_video_ = true;
			int max_bitrate_ = 7865;

			sys::echo<std::string, voip::call_ptr> echo_gks_call_;
		};

	}
}

