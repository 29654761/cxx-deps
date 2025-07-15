#pragma once
#include "opal_event.h"
#include "process.h"
#include "manager.h"
#include "opal.h"
#include "call.h"
#include "h323/gatekeeper.h"
#include "local/endpoint.h"
#include "h323/endpoint.h"
#include "sip/endpoint.h"
#include "local/connection.h"
#include <memory>
#include <shared_mutex>

namespace voip
{

	class instance
	{
	public:
		instance();
		~instance();



		bool init(voip::opal_event* e);
		void cleanup();

		void set_nat_address(const PString& address);
		PString get_nat_address()const;
		bool set_tcp_port(int start, int end);
		bool set_udp_port(int start, int end);
		bool set_rtp_port(int start, int end);

		bool enable_h323(const PStringArray& aliases,const PStringArray& listeners="0.0.0.0");
		void disable_h323();
		bool enable_h323_gk_client(const PString& host,const PString& username,const PString& password);
		void disable_h323_gk_client();
		bool is_h323_registered();

		bool enable_h323_gk_server(const PStringArray& listeners = "0.0.0.0");
		void disable_h323_gk_server();

		bool enable_sip(const PString& alias, const PStringArray& listeners = "0.0.0.0", const PString& realm="");
		void disable_sip();
		PString add_sip_register(const PString& host, const PString& alias, const PString& authname = PString::Empty(), const PString& password = PString::Empty(), const PString& authrealm = PString::Empty(),int interval=30);
		bool remove_sip_register(const PString& aor);
		void remove_all_sip_registers();
		bool is_sip_registered(const PString& aor);

		/// <summary>
		/// Make a call
		/// </summary>
		/// <param name="url">Remote Url</param>
		/// <param name="call">return a call</param>
		/// <returns>
		/// 0: success;
		/// -1: Not initialized;
		/// -2: Bad request paramster;
		/// -3: Not found (Not online);
		/// -4: Call id conflict;
		/// </returns>
		int call(const PString& url, PSafePtr<voip::call>& call, const PString& audio="sendrecv", const PString& video = "sendrecv");
		bool clear_call(const PString& token, OpalConnection::CallEndReason reason= OpalConnection::EndedByLocalUser);
		PSafePtr<voip::call> find_call(const PString& token);
		

		void clear_all_calls();
	protected:
		virtual bool on_incoming_call(const PString& token, const PString& alias);
		virtual bool on_outgoing_call(const PString& token, const PString& alias);
		virtual OpalConnection::AnswerCallResponse on_answer_call(const PString& token, const PString& alias);
		virtual void on_alerting(const PString& token);

		virtual bool on_require_password(const PString& alias, PString& password);
		virtual void on_registration(const PString& prefix, const PString& username);
		virtual void on_unregistration(const PString& prefix, const PString& username);

		virtual void on_rtp_frame(const OpalMediaStream& mediaStream, RTP_DataFrame& frame);

	private:
		voip::opal_event* event_ = nullptr;
		mutable std::shared_mutex mutex_;
		std::shared_ptr<process> process_;
		std::shared_ptr<manager> mgr_;
		std::shared_ptr<h323::gatekeeper> gk_;
		local::endpoint* localep_ = nullptr;
		h323::endpoint* h323ep_ = nullptr;
		sip::endpoint* sipep_ = nullptr;

		/*
		int tcp_port_start_= 1730;
		int tcp_port_end_= 1739;
		int udp_port_start_= 1730;
		int udp_port_end_= 1739;
		int rtp_port_start_=1740;
		int rtp_port_end_ = 1749;
		*/
		PString nat_address_;
	};

}

