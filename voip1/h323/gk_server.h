#pragma once
#include "reg_endpoint.h"
#include "../call.h"
#include <sys2/async_socket.h>
#include <sys2/signal.h>
#include <sys2/echo.hpp>
#include <sys2/callback.hpp>
#include <ptclib/guid.h>

namespace voip
{
	namespace h323
	{

		class gk_server
		{
		public:
			typedef void(*on_reg_endpoint_t)(void* ctx, const reg_endpoint& ep);
			typedef void(*on_reg_endpoint_failed_t)(void* ctx, const PStringArray& aliases, H225_RegistrationRejectReason::Choices reason);
			typedef void(*on_unreg_endpoint_t)(void* ctx, const reg_endpoint& ep);
			typedef void(*require_password_t)(void* ctx, voip::call_type_t type, const std::string& alias,bool& success, std::string& password);
			typedef void(*on_admission_request_t)(void* ctx, const PStringArray& called_aliases,PString& signal_address,WORD& signal_port,bool& result);
			gk_server();
			~gk_server();

			void set_nat_address(const PString& address) { nat_address_ = address; }
			void set_gk_identifier(const PString& identifier) { gk_identifier_ = identifier; }
			bool start(const std::string& addr,int port, int worknum);
			void stop();

			bool call(const PString& alias, 
				const PString& signal_address, int signal_port,
				const std::string& call_id="", const std::string& conf_id="");



		private:
			static void s_on_read_event(void* ctx, sys::async_socket_ptr socket, const char* buffer, uint32_t size, const sockaddr* addr, socklen_t addr_size);

			void on_gatekeeper_request(sys::async_socket_ptr socket, 
				const H225_GatekeeperRequest& request, 
				const sockaddr* addr, socklen_t addr_size);
			void on_registration_request(sys::async_socket_ptr socket, 
				const H225_RegistrationRequest& request, 
				const sockaddr* addr, socklen_t addr_size);
			void on_unregistration_request(sys::async_socket_ptr socket, 
				const H225_UnregistrationRequest& request,
				const sockaddr* addr, socklen_t addr_size);
			void on_info_request_response(sys::async_socket_ptr socket,
				const H225_InfoRequestResponse& request,
				const sockaddr* addr, socklen_t addr_size);
			void on_admission_request(sys::async_socket_ptr socket,
				const H225_AdmissionRequest& request,
				const sockaddr* addr, socklen_t addr_size);
			void on_disengage_request(sys::async_socket_ptr socket,
				const H225_DisengageRequest& request,
				const sockaddr* addr, socklen_t addr_size);
			void on_location_request(sys::async_socket_ptr socket,
				const H225_LocationRequest& request,
				const sockaddr* addr, socklen_t addr_size);
		private:
			bool send_gatekeeper_confirm(sys::async_socket_ptr socket,
				const H225_GatekeeperRequest& request, 
				const sockaddr* addr, socklen_t addr_size);
			bool send_gatekeeper_reject(sys::async_socket_ptr socket,
				const H225_GatekeeperRequest& request,
				H225_GatekeeperRejectReason::Choices reason,
				const sockaddr* addr, socklen_t addr_size);

			bool send_registration_confirm(sys::async_socket_ptr socket,
				const H225_RegistrationRequest& request, 
				const PString& ep_identifier,
				const sockaddr* addr, socklen_t addr_size);
			bool send_registration_reject(sys::async_socket_ptr socket, 
				const H225_RegistrationRequest& request, 
				H225_RegistrationRejectReason::Choices reason, const
				sockaddr* addr, socklen_t addr_size);

			bool send_unregistration_confirm(sys::async_socket_ptr socket,
				const H225_UnregistrationRequest& request, 
				const sockaddr* addr, socklen_t addr_size);
			bool send_unregistration_reject(sys::async_socket_ptr socket, 
				const H225_UnregistrationRequest& request,
				H225_UnregRejectReason::Choices reason,
				const sockaddr* addr, socklen_t addr_size);
			bool send_admission_confrim(sys::async_socket_ptr socket,
				const H225_AdmissionRequest& request,
				const PIPSocket::Address& signal_address,WORD signal_port,
				int bandwidth,
				const sockaddr* addr, socklen_t addr_size);
			bool send_admission_reject(sys::async_socket_ptr socket,
				const H225_AdmissionRequest& request,
				H225_AdmissionRejectReason::Choices reason,
				const sockaddr* addr, socklen_t addr_size);
			bool send_disengage_confrim(sys::async_socket_ptr socket,
				const H225_DisengageRequest& request,
				const sockaddr* addr, socklen_t addr_size);
			bool send_disengage_reject(sys::async_socket_ptr socket,
				const H225_DisengageRequest& request,
				H225_DisengageRejectReason::Choices reason,
				const sockaddr* addr, socklen_t addr_size);
			bool send_location_confrim(sys::async_socket_ptr socket,
				const H225_LocationRequest& request,
				int bandwidth,
				const sockaddr* addr, socklen_t addr_size);
			bool send_location_reject(sys::async_socket_ptr socket,
				const H225_LocationRequest& request,
				H225_LocationRejectReason::Choices reason,
				const sockaddr* addr, socklen_t addr_size);

			bool send_service_control_indication(
				sys::async_socket_ptr socket,
				const PGloballyUniqueID& call_id,
				const PGloballyUniqueID& conf_id,
				const PString& signal_address,int signal_port,
				const sockaddr* addr, socklen_t addr_size);

			bool service_control_indication(
				sys::async_socket_ptr socket,
				const PGloballyUniqueID& call_id,
				const PGloballyUniqueID& conf_id,
				const PString& signal_address, int signal_port,
				const sockaddr* addr, socklen_t addr_size, H225_RasMessage& rsp);

			bool send_info_request(
				sys::async_socket_ptr socket,
				uint32_t seq,
				const PGloballyUniqueID& call_id,
				const uint32_t call_ref,
				const sockaddr* addr, socklen_t addr_size);

			bool send_info_request_ack(
				sys::async_socket_ptr socket,
				uint32_t seq,
				const PString& signal_address, int signal_port,
				const sockaddr* addr, socklen_t addr_size);

			
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
			void on_timer();
			bool require_aliases_password(const PStringArray& aliases,PString& password);
		public:
			sys::callback<on_reg_endpoint_t> on_reg_endpoint;
			sys::callback<on_reg_endpoint_failed_t> on_reg_endpoint_failed;
			sys::callback<on_unreg_endpoint_t> on_unreg_endpoint;
			sys::callback<require_password_t> on_require_password;
			sys::callback<on_admission_request_t> on_admission;
		private:
			bool active_ = false;
			sys::signal signal_;
			std::thread* timer_ = nullptr;
			sys::async_socket_ptr skt_;
			sys::async_service ios_;

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

