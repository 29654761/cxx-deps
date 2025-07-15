#pragma once
#include "../call.h"
#include <sys2/async_socket.h>
#include <sys2/echo.hpp>
#include <spdlog/spdlogger.hpp>
#include <ptlib.h>
#include <opal.h>
#include <ptclib/guid.h>
#include <asn/h225.h>
#include <atomic>
#include <sys2/signal.h>


namespace voip
{
	namespace h323
	{
		class endpoint;
		class gk_client
		{
		public:
			typedef void (*on_status_t)(void* ctx, bool status);
			typedef void (*on_incoming_call_t)(void* ctx, const H225_ServiceControlIndication& body, H225_ServiceControlResponse_result::Choices& result);
			gk_client(endpoint& ep,sys::async_service& ios);
			~gk_client();
			void set_nat_address(const PString& address) { nat_address_ = address; }
			void set_logger(spdlogger_ptr log) { log_ = log; }
			void set_alias(const std::string& alias, const std::string& username, const std::string& password) {
				alias_ = alias;
				username_ = username;
				password_ = password;
			}
			bool start(const std::string& ras_addr,int ras_port, int local_port=0,int interval=30);
			void stop();

			bool status()const { return is_registered_; }

			call_ptr make_call(const PString& alias,
				const PGloballyUniqueID& call_id,
				const PGloballyUniqueID& conf_id,
				int call_ref, int bandwidth = 15360);

		public:
			bool send_service_control_response(const H225_ServiceControlIndication& request, 
				H225_ServiceControlResponse_result::Choices result);

			bool send_admission_request(const PString& dst_alias,
				const PGloballyUniqueID& call_id,
				const PGloballyUniqueID& conf_id,
				int call_ref,int bandwidth=15360);

			bool admission_request(const PString& dst_alias,
				const PGloballyUniqueID& call_id,
				const PGloballyUniqueID& conf_id,
				int call_ref, int bandwidth, H225_RasMessage& rsp);

			bool send_disengage_request(
				H225_DisengageReason::Choices reason,
				const PGloballyUniqueID& call_id,
				const PGloballyUniqueID& conf_id,
				int call_ref);

			bool send_gatekeeper_request();
			bool send_registration_request();
			bool send_unregistration_request(H225_UnregRequestReason::Choices reason);

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
			void set_status(bool status);
			void run_timer();
			static void s_on_read_event(void* ctx, sys::async_socket_ptr socket, const char* buffer, uint32_t size, const sockaddr* addr, socklen_t addr_size);
			void on_gatekeeper_confirm(const H225_GatekeeperConfirm& body);
			void on_gatekeeper_reject(const H225_GatekeeperReject& body);
			void on_registration_confirm(const H225_RegistrationConfirm& body);
			void on_registration_reject(const H225_RegistrationReject& body);
			void on_service_control_indication(const H225_ServiceControlIndication& body);
			void on_info_request_ack(const H225_InfoRequestAck& body);
			void on_disengage_confirm(const H225_DisengageConfirm& body);
			void on_disengage_reject(const H225_DisengageReject& body);
			void on_admission_confirm(const H225_RasMessage& ras);
			void on_admission_reject(const H225_RasMessage& ras);
		public:
			sys::callback<on_status_t> on_status;
			sys::callback<on_incoming_call_t> on_incoming_call;
		private:
			bool active_ = false;
			std::recursive_mutex mutex_;
			std::shared_ptr<std::thread> timer_;
			sys::signal signal_;
			sys::async_socket_ptr socket_;
			sys::async_service& ios_;
			spdlogger_ptr log_;
			endpoint& ep_;

			sockaddr_storage addr_ = {};
			PString alias_;
			PString username_;
			PString password_;
			std::atomic<uint32_t> request_seq_;
			PString endpoint_identifier_;
			PString gk_identifier_;
			int time_to_live_ = 60;
			std::atomic<int> interval_;
			PString nat_address_;
			WORD signal_port_ = 1720;
			WORD local_port_ = 0;

			PString ras_addr_;
			WORD ras_port_ = 1719;

			std::atomic<int64_t> updated_at_;
			bool is_registered_ = false;

			sys::echo<H225_RasMessage, H225_RasMessage> echo_;
		};


	}
}
