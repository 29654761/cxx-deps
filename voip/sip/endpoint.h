#pragma once
#include "reg_endpoint.h"
#include "sip_call.h"
#include "gk_client.h"
#include <sip/sip_server.h>


namespace voip
{
	namespace sip
	{
		class endpoint;
		typedef std::shared_ptr<endpoint> endpoint_ptr;

		class endpoint:public std::enable_shared_from_this<endpoint>
		{
			friend gk_client;
		public:
			typedef std::function<void(endpoint_ptr svr, const reg_endpoint& ep)> on_reg_endpoint_t;
			typedef std::function<void(endpoint_ptr svr, const std::string& alias)> on_reg_endpoint_failed_t;
			typedef std::function<void(endpoint_ptr svr, const reg_endpoint& ep)> on_unreg_endpoint_t;
			typedef std::function<void(voip::call_type_t type, const std::string& alias, bool& success, std::string& password)>on_require_password_t;
			typedef std::function<void(voip::call_ptr call,const std::string& called_alias,const std::string& remote_alias,
				const std::string& remote_addr,int remote_port)> on_incoming_call_t;
			typedef std::function<void(endpoint_ptr svr, bool status)>on_gk_status_t;



			endpoint(asio::io_context& ioc);
			~endpoint();


			void set_on_reg_endpoint(on_reg_endpoint_t handler) { on_reg_endpoint = handler; }
			void set_on_reg_endpoint_failed(on_reg_endpoint_failed_t handler) { on_reg_endpoint_failed = handler; }
			void set_on_unreg_endpoint(on_unreg_endpoint_t handler) { on_unreg_endpoint = handler; }
			void set_on_require_password(on_require_password_t handler) { on_require_password = handler; }
			void set_on_incoming_call(on_incoming_call_t handler) { on_incoming_call = handler; }
			void set_on_gk_status(on_gk_status_t handler) { on_gk_status = handler; }


			asio::io_context& io_context() { return ioc_; }

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


			bool start(const std::string& addr,int port);
			void stop();
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
			bool get_reg_endpoint_by_conid(const std::string& con_id,reg_endpoint& item);
			bool remove_reg_endpoint(const std::string& name);
			void all_reg_endpoints(std::vector<reg_endpoint>& vec);
			size_t count_reg_endpoints();
			void clear_reg_endpoints();

			bool set_call(std::shared_ptr<sip_call> call);
			std::shared_ptr<sip_call> get_call(const std::string& call_id);
			std::shared_ptr<sip_call> find_call_by_connection(const std::string& con_id);
			bool remove_call(std::shared_ptr<sip_call> call,call::reason_code_t reason);
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
		private:
			void handle_timer(endpoint_ptr ep, const std::error_code& ec);
			void handle_close(endpoint_ptr ep, sip_server_ptr svr, sip_connection_ptr con);
			void handle_message(endpoint_ptr ep, sip_server_ptr svr, sip_connection_ptr con, const sip_message& message);
			void handle_gkclient_status(endpoint_ptr ep,gk_client_ptr gkc, bool status);
			void handle_destroy_call(endpoint_ptr self, call_ptr call, call::reason_code_t reason);
			void handle_incoming_call(endpoint_ptr self, call_ptr call, const std::string& local_alias, const std::string& remote_alias,
				const std::string& remote_addr, int remote_port);

			void on_register(sip_connection_ptr con, const sip_message& message);
			void on_message(sip_connection_ptr con, const sip_message& message);
			void on_invite(sip_connection_ptr con, const sip_message& message);
			void on_ack(sip_connection_ptr con, const sip_message& message);
			void on_info(sip_connection_ptr con, const sip_message& message);
			void on_bye(sip_connection_ptr con, const sip_message& message);
			void on_options(sip_connection_ptr con, const sip_message& message);
			void on_response(sip_connection_ptr con, const sip_message& message);

		private:
			asio::io_context& ioc_;

			mutable std::shared_mutex mutex_;
			sip_server_ptr server_;
			bool active_ = false;
			asio::steady_timer timer_;
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
			int max_bitrate_ = 1920;

			std::shared_ptr<gk_client> gkclient_;

			on_reg_endpoint_t on_reg_endpoint;
			on_reg_endpoint_failed_t on_reg_endpoint_failed;
			on_unreg_endpoint_t on_unreg_endpoint;
			on_require_password_t on_require_password;
			on_incoming_call_t on_incoming_call;
			on_gk_status_t on_gk_status;
		};


	}
}

