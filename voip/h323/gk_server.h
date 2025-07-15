#pragma once
#include "reg_endpoint.h"
#include "../call.h"
#include <sys2/echo.hpp>
#include <sys2/callback.hpp>
#include <ptclib/guid.h>
#include <asio.hpp>

namespace voip
{
	namespace h323
	{
		class gk_server;
		typedef std::shared_ptr<gk_server> gk_server_ptr;

		class gk_server:public std::enable_shared_from_this<gk_server>
		{
		public:

			typedef std::function<void(gk_server_ptr, const reg_endpoint&)> on_reg_endpoint_t;
			typedef std::function<void(gk_server_ptr, const PStringArray&, H225_RegistrationRejectReason::Choices)>on_reg_endpoint_failed_t;
			typedef std::function<void(gk_server_ptr, const reg_endpoint&)>on_unreg_endpoint_t;
			typedef std::function<void(voip::call_type_t, const std::string&,bool&, std::string&)>require_password_t;
			typedef std::function<void(gk_server_ptr, const PStringArray&,PString&,WORD&,bool&)>on_admission_request_t;

			gk_server(asio::io_context& ioc);
			~gk_server();

			void set_on_reg_endpoint(on_reg_endpoint_t handler) { on_reg_endpoint = handler; };
			void set_on_reg_endpoint_failed(on_reg_endpoint_failed_t handler) { on_reg_endpoint_failed = handler; }
			void set_on_unreg_endpoint(on_unreg_endpoint_t handler) { on_unreg_endpoint = handler; }
			void set_on_require_password(require_password_t handler) {on_require_password = handler;}
			void set_on_admission_request(on_admission_request_t handler) {on_admission = handler;}

			void set_nat_address(const PString& address) { nat_address_ = address; }
			void set_gk_identifier(const PString& identifier) { gk_identifier_ = identifier; }
			bool start(const std::string& addr,int port);
			void stop();

			bool call(const PString& alias, 
				const PString& signal_address, int signal_port,
				const std::string& call_id="", const std::string& conf_id="");



		private:
			void handle_timer(gk_server_ptr self, const std::error_code& ec);
			void handle_read(gk_server_ptr self, const std::error_code& ec, std::size_t bytes);


			void on_gatekeeper_request(asio::ip::udp::socket& socket,
				const H225_GatekeeperRequest& request, 
				const asio::ip::udp::endpoint& ep);
			void on_registration_request(asio::ip::udp::socket& socket,
				const H225_RegistrationRequest& request, 
				const asio::ip::udp::endpoint& ep);
			void on_unregistration_request(asio::ip::udp::socket& socket,
				const H225_UnregistrationRequest& request,
				const asio::ip::udp::endpoint& ep);
			void on_info_request_response(asio::ip::udp::socket& socket,
				const H225_InfoRequestResponse& request,
				const asio::ip::udp::endpoint& ep);
			void on_admission_request(asio::ip::udp::socket& socket,
				const H225_AdmissionRequest& request,
				const asio::ip::udp::endpoint& ep);
			void on_disengage_request(asio::ip::udp::socket& socket,
				const H225_DisengageRequest& request,
				const asio::ip::udp::endpoint& ep);
			void on_location_request(asio::ip::udp::socket& socket,
				const H225_LocationRequest& request,
				const asio::ip::udp::endpoint& ep);
		private:
			bool send_gatekeeper_confirm(asio::ip::udp::socket& socket,
				const H225_GatekeeperRequest& request, 
				const asio::ip::udp::endpoint& ep);
			bool send_gatekeeper_reject(asio::ip::udp::socket& socket,
				const H225_GatekeeperRequest& request,
				H225_GatekeeperRejectReason::Choices reason,
				const asio::ip::udp::endpoint& ep);

			bool send_registration_confirm(asio::ip::udp::socket& socket,
				const H225_RegistrationRequest& request, 
				const PString& ep_identifier,
				const asio::ip::udp::endpoint& ep);
			bool send_registration_reject(asio::ip::udp::socket& socket,
				const H225_RegistrationRequest& request, 
				H225_RegistrationRejectReason::Choices reason,
				const asio::ip::udp::endpoint& ep);

			bool send_unregistration_confirm(asio::ip::udp::socket& socket,
				const H225_UnregistrationRequest& request, 
				const asio::ip::udp::endpoint& ep);
			bool send_unregistration_reject(asio::ip::udp::socket& socket,
				const H225_UnregistrationRequest& request,
				H225_UnregRejectReason::Choices reason,
				const asio::ip::udp::endpoint& ep);
			bool send_admission_confrim(asio::ip::udp::socket& socket,
				const H225_AdmissionRequest& request,
				const PIPSocket::Address& signal_address,WORD signal_port,
				int bandwidth,
				const asio::ip::udp::endpoint& ep);
			bool send_admission_reject(asio::ip::udp::socket& socket,
				const H225_AdmissionRequest& request,
				H225_AdmissionRejectReason::Choices reason,
				const asio::ip::udp::endpoint& ep);
			bool send_disengage_confrim(asio::ip::udp::socket& socket,
				const H225_DisengageRequest& request,
				const asio::ip::udp::endpoint& ep);
			bool send_disengage_reject(asio::ip::udp::socket& socket,
				const H225_DisengageRequest& request,
				H225_DisengageRejectReason::Choices reason,
				const asio::ip::udp::endpoint& ep);
			bool send_location_confrim(asio::ip::udp::socket& socket,
				const H225_LocationRequest& request,
				int bandwidth,
				const asio::ip::udp::endpoint& ep);
			bool send_location_reject(asio::ip::udp::socket& socket,
				const H225_LocationRequest& request,
				H225_LocationRejectReason::Choices reason,
				const asio::ip::udp::endpoint& ep);

			bool send_service_control_indication(
				asio::ip::udp::socket& socket,
				const PGloballyUniqueID& call_id,
				const PGloballyUniqueID& conf_id,
				const PString& signal_address,int signal_port,
				const asio::ip::udp::endpoint& ep);

			bool service_control_indication(
				asio::ip::udp::socket& socket,
				const PGloballyUniqueID& call_id,
				const PGloballyUniqueID& conf_id,
				const PString& signal_address, int signal_port,
				const asio::ip::udp::endpoint& ep, H225_RasMessage& rsp);

			bool send_info_request(
				asio::ip::udp::socket& socket,
				uint32_t seq,
				const PGloballyUniqueID& call_id,
				const uint32_t call_ref,
				const asio::ip::udp::endpoint& ep);

			bool send_info_request_ack(
				asio::ip::udp::socket& socket,
				uint32_t seq,
				const PString& signal_address, int signal_port,
				const asio::ip::udp::endpoint& ep);

			
		private:
			bool check_auth(const H225_ArrayOf_CryptoH323Token& tokens,const PString& password);
			bool check_auth_md5(const H225_CryptoH323Token_cryptoEPPwdHash& hash,const PString& password);

			bool set_reg_endpoint(reg_endpoint& endpoint);
			bool get_reg_endpoint(const std::string& id, reg_endpoint& endpoint);
			bool get_reg_endpoint(const PStringArray& aliases, reg_endpoint& endpoint);
			bool remove_reg_endpoint(const std::string& id);
			void clear_reg_endpoint();
			void all_reg_endpoint(std::vector<reg_endpoint>& endpoints);
			size_t clear_reg_endpoints();
			bool require_aliases_password(const PStringArray& aliases,PString& password);

		private:
			on_reg_endpoint_t on_reg_endpoint;
			on_reg_endpoint_failed_t on_reg_endpoint_failed;
			on_unreg_endpoint_t on_unreg_endpoint;
			require_password_t on_require_password;
			on_admission_request_t on_admission;

			bool active_ = false;
			asio::io_context& ioc_;
			asio::ip::udp::socket skt_;
			asio::steady_timer timer_;

			std::array<uint8_t, 10240> recv_buffer_;
			asio::ip::udp::endpoint recv_ep_;

			PString nat_address_;
			WORD port_ = 1719;
			PString gk_identifier_ = "H323GK";
			int def_time_to_live_ = 60;

			std::recursive_mutex mutex_;
			std::map<std::string, reg_endpoint> endpoints_;

			std::atomic<uint32_t> request_seq_;
			sys::echo<H225_RasMessage, H225_RasMessage> echo_;
		};

	}
}

