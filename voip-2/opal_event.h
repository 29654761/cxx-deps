#pragma once
#include <ptlib.h>
#include <opal/mediastrm.h>
#include <opal/connection.h>
#include <sip/sippdu.h>
#include <h323/h323ep.h>

namespace voip
{
	class opal_event
	{
	public:
		virtual bool on_incoming_call(const PString& token, const PString& alias) = 0;
		virtual bool on_outgoing_call(const PString& token, const PString& alias) = 0;
		virtual OpalConnection::AnswerCallResponse on_answer_call(OpalConnection& connection, const PString& caller) = 0;
		virtual PBoolean on_forwarded(OpalConnection& connection, const PString& remote_party) = 0;


		// Events for server
		virtual bool on_require_password(const PString& prefix,const PString& alias, PString& password) = 0;
		virtual bool on_registration(const PString& prefix, const PString& username) = 0;
		virtual void on_unregistration(const PString& prefix, const PString& username) = 0;

		// Events for client register to server
		virtual void on_h323_reg_status(H323Gatekeeper& gk, H323Gatekeeper::RegistrationFailReasons status) = 0;
		virtual void on_sip_reg_status(const PString& aor, PBoolean wasRegistering, PBoolean reRegistering, SIP_PDU::StatusCodes reason) = 0;

	};

	class opal_call_event
	{
	public:
		//virtual OpalConnection::AnswerCallResponse on_answer_call(OpalConnection& connection, const PString& caller) = 0;
		virtual void on_established_call() = 0;
		virtual void on_alerting(OpalConnection& connection,bool with_media) = 0;
		virtual void on_cleared() = 0;
		virtual void on_media_frame_received(const OpalMediaStream& mediaStream,const uint8_t* frame,size_t size,uint64_t ts) = 0;
		virtual bool on_create_media_stream(const OpalMediaFormat& mediaFormat, unsigned sessionID) = 0;
		virtual void on_closed_media_stream(const OpalMediaStream& stream) = 0;
	};

}

