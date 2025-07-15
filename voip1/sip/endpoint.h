#pragma once
#include "reg_endpoint.h"
#include "sip_call.h"
#include "gk_client.h"
#include <hypertext/sip/sip_server.h>


namespace voip
{
	namespace sip
	{
		class endpoint
		{
			friend gk_client;
		public:
			typedef void(*on_reg_endpoint_t)(void* ctx, const reg_endpoint& ep);
			typedef void(*on_reg_endpoint_failed_t)(void* ctx, const std::string& alias);
			typedef void(*on_unreg_endpoint_t)(void* ctx, const reg_endpoint& ep);
			typedef void(*on_require_password_t)(void* ctx, voip::call_type_t type, const std::string& alias, bool& success, std::string& password);
			typedef void (*on_incoming_call_t)(void* ctx, voip::call_ptr call,const std::string& called_alias,const std::string& remote_alias,
				const std::string& remote_addr,int remote_port);
			typedef void (*on_gk_status_t)(void* ctx, bool status);
			endpoint();
			~endpoint();

			std::string local_alias()const;
			void set_local_alias(const std::string& alias);
			
			void set_realm(const std::string& realm);
			std::string realm()const;

			void set_nat_address(const std::string& nat_address);
			const std::string& nat_address()const;
			
			
			void set_rtp_ports(litertp::port_range_ptr ports);
			litertp::port_range_ptr rtp_ports()const;
			
			void set_max_bitrate(int v);
			int max_bitrate()const;

			void set_logger(spdlogger_ptr log) { log_ = log; }
			void set_incoming_media(bool audio, bool video) { incoming_audio_ = audio; incoming_video_ = video; }


			bool start(const std::string& addr,int port, int worknum);
			void stop();
			void notify_clear_call() { signal_.notify(); }
			bool start_gk_client(const std::string& alias, const std::string& username, const std::string& password,
				const std::string& addr, int port, bool is_tcp);
			void stop_gk_client();
			bool is_gk_started()const;
			bool gk_status()const;

			call_ptr make_call(const std::string& url,bool tcp=true);
			call_ptr make_reg_server_call(const std::string& alias);
			call_ptr make_reg_client_call(const std::string& alias);
		public:

			bool set_reg_endpoint(const reg_endpoint& ep);
			bool get_reg_endpoint(const std::string& name, reg_endpoint& item);
			bool remove_reg_endpoint(const std::string& name);
			void all_reg_endpoints(std::vector<reg_endpoint>& vec);
			size_t count_reg_endpoints();
			void clear_reg_endpoints();

			bool set_call(std::shared_ptr<sip_call> call);
			std::shared_ptr<sip_call> get_call(const std::string& call_id);
			std::shared_ptr<sip_call> find_call_by_session(const std::string& sess_id);
			bool remove_call(std::shared_ptr<sip_call> call);
			void all_calls(std::vector<std::shared_ptr<sip_call>>& vec);
			size_t count_calls();
			void clear_calls();

			sip_message create_request(const std::string& method, const voip_uri& url,
				uint32_t cseq, const sip_address& from, const sip_address& to,const sip_address* contact,
				const std::vector<sip_via>& vias, bool allow);
			sip_message create_response(const sip_message& request, const sip_address* contact, bool allow);
			static std::string create_branch();
			static std::string create_tag();
		private:
			void set_gkclient(std::shared_ptr<gk_client> gkclient);
			std::shared_ptr<gk_client> get_gkclient()const;
			static void s_on_gk_status(void* ctx, bool status);
		private:
			void run_timer();
			static void s_session_created(void* ctx, sip_session_ptr session);
			static void s_session_destroyed(void* ctx, sip_session_ptr session);
			static void s_session_message(void* ctx, sip_session_ptr session, const sip_message& message);
			void on_register(sip_session_ptr session, const sip_message& message);
			void on_message(sip_session_ptr session, const sip_message& message);
			void on_invite(sip_session_ptr session, const sip_message& message);
			void on_ack(sip_session_ptr session, const sip_message& message);
			void on_info(sip_session_ptr session, const sip_message& message);
			void on_bye(sip_session_ptr session, const sip_message& message);
			void on_options(sip_session_ptr session, const sip_message& message);
			void on_response(sip_session_ptr session, const sip_message& message);
		public:
			sys::callback<on_reg_endpoint_t> on_reg_endpoint;
			sys::callback<on_reg_endpoint_failed_t> on_reg_endpoint_failed;
			sys::callback<on_unreg_endpoint_t> on_unreg_endpoint;
			sys::callback<on_require_password_t> on_require_password;
			sys::callback<on_incoming_call_t> on_incoming_call;
			sys::callback<on_gk_status_t> on_gk_status;
		private:
			mutable std::shared_mutex mutex_;
			sip_server server_;
			bool active_ = false;
			std::shared_ptr<std::thread> timer_;
			sys::signal signal_;
			std::recursive_mutex reg_endpoints_mutex_;
			std::map<std::string, reg_endpoint> reg_endpoints_;
			std::recursive_mutex calls_mutex_;
			std::map<std::string, std::shared_ptr<sip_call>> calls_;

			std::string alias_;
			std::string realm_;
			std::string nat_address_;
			int port_ = 5060;

			litertp::port_range_ptr rtp_ports_;
			spdlogger_ptr log_;

			bool incoming_audio_ = true;
			bool incoming_video_ = true;
			int max_bitrate_ = 7865;

			std::shared_ptr<gk_client> gkclient_;
		};


	}
}

