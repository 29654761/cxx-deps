#pragma once
#include "h323_call.h"
#include "gk_client.h"

namespace voip
{
	namespace h323
	{
		class gk_server;
		typedef std::shared_ptr<gk_server> gk_server_ptr;

		class endpoint;
		typedef std::shared_ptr<endpoint> endpoint_ptr;

		class endpoint:public std::enable_shared_from_this<endpoint>
		{
			friend gk_client;
			friend h323_call;
		public:
			typedef std::function<void (voip::call_ptr call, const std::string& called_alias, const std::string& remote_alias,
				const std::string& remote_addr, int remote_port)> on_incoming_call_t;
			typedef std::function<void(endpoint_ptr, bool)> on_gk_status_t;

			typedef std::shared_ptr<asio::ip::tcp::socket> tcp_socket_ptr;
			typedef std::shared_ptr<asio::ip::udp::socket> udp_socket_ptr;

			endpoint(asio::io_context& ioc);
			~endpoint();

			void set_on_incoming_call(on_incoming_call_t handler) { on_incoming_call = handler; }
			void set_on_gk_status(on_gk_status_t handler) { on_gk_status = handler; }

			asio::io_context& io_context() { return ioc_; }

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


			bool start(const std::string& addr,int port);
			void stop();

			bool start_gk_client(const PString& ras_address, WORD ras_port,const PString& alias, const PString& username,const PString& password);
			void stop_gk_client();
			bool is_gk_started()const;
			bool gk_status()const;

			call_ptr make_call(const std::string& url,const std::string& local_alias="", const std::string& call_id = "", const std::string& conf_id = "", int call_ref = -1);
			call_ptr make_reg_server_call(const std::string& alias,gk_server_ptr gkserver);
			call_ptr make_reg_client_call(const std::string& alias);
		private:
			void handle_timer(endpoint_ptr self, const std::error_code& ec);
			void handle_accept(endpoint_ptr self, const std::error_code& ec, asio::ip::tcp::socket socket);
			void handle_destroy_call(endpoint_ptr self, call_ptr call,call::reason_code_t reason);
			void handle_incoming_call(endpoint_ptr self, call_ptr call,const std::string& local_alias, const std::string& remote_alias,
				const std::string& remote_addr, int remote_port);
			void handle_facility(endpoint_ptr self, call_ptr call, const PGloballyUniqueID& call_id, const PGloballyUniqueID& conf_id);
			void handle_gk_status(endpoint_ptr self, gk_client_ptr gk, bool status);
			void handle_gk_incoming_call(endpoint_ptr self,gk_client_ptr gk, const H225_ServiceControlIndication& body, H225_ServiceControlResponse_result::Choices& result);


			bool add_call(std::shared_ptr<h323_call> call);
			std::shared_ptr<h323_call> get_call(const std::string& call_id);
			bool remove_call(std::shared_ptr<h323_call> call,call::reason_code_t reason);
			void all_calls(std::vector<std::shared_ptr<h323_call>>& vec);
			size_t count_calls();
			void clear_calls();

			void set_gkclient(std::shared_ptr<gk_client> gkclient);
			std::shared_ptr<gk_client> get_gkclient()const;

		private:
			on_incoming_call_t on_incoming_call;
			on_gk_status_t on_gk_status;

			asio::io_context& ioc_;
			asio::ip::tcp::acceptor acceptor_;
			mutable std::shared_mutex mutex_;
			bool active_ = false;
			WORD port_ = 1720;
			PString alias_;
			PString nat_address_;

			std::shared_ptr<gk_client> gkclient_;

			asio::steady_timer timer_;

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

