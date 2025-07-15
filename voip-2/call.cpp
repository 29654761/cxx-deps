#include "call.h"
#include "sip/connection.h"
#include "h323/connection.h"

namespace voip
{

	call::call(OpalManager& manager):OpalCall(manager)
	{
	}

	call::~call()
	{
	}

	void call::set_event(opal_call_event* e)
	{
		std::unique_lock<std::shared_mutex> lk(event_mutex_);
		event_ = e;
	}

	PSafePtr<local::connection> call::find_local_connection()
	{
		return this->GetConnectionAs<local::connection>();
	}

	PSafePtr<OpalConnection> call::find_remote_connection()
	{
		PSafePtr<OpalConnection> con;
		con=GetConnectionAs<h323::connection>();
		if (con)
			return con;

		con = GetConnectionAs<sip::connection>();
		if (con)
			return con;

		return nullptr;
	}

	opal_call_event* call::get_event()
	{
		std::shared_lock<std::shared_mutex> lk(event_mutex_);
		return event_;
	}

	bool call::input_audio_frame(const uint8_t* frame, uint32_t size, uint32_t duration)
	{
		auto con = this->GetConnectionAs<local::connection>();
		if (!con)
			return false;

		return con->input_audio_frame(frame, size, duration);
	}

	bool call::input_video_frame(const uint8_t* frame, uint32_t size, uint32_t duration)
	{
		auto con = this->GetConnectionAs<local::connection>();
		if (!con)
			return false;
		return con->input_video_frame(frame, size, duration);
	}

	bool call::require_keyframe()
	{
		auto con = this->GetConnectionAs<local::connection>();
		if (!con)
			return false;
		return con->require_keyframe();
	}

	/*
	OpalConnection::AnswerCallResponse call::OnAnswerCall(OpalConnection& connection, const PString& caller)
	{
		auto* e = get_event();
		if (e)
		{
			return e->on_answer_call(connection, caller);
		}
		else
		{
			return OpalCall::OnAnswerCall(connection, caller);
		}
	}
	*/

	void call::OnEstablishedCall()
	{
		OpalCall::OnEstablishedCall();
		auto* e = get_event();
		if (e)
		{
			return e->on_established_call();
		}
	}

	PBoolean call::OnEstablished(
		OpalConnection& connection   ///<  Connection that indicates it is alerting
	)
	{
		return OpalCall::OnEstablished(connection);
	}

	void call::OnAlerting(OpalConnection& connection, bool withMedia)
	{
		OpalCall::OnAlerting(connection, withMedia);
		auto* e = get_event();
		if (e)
		{
			e->on_alerting(connection, withMedia);
		}
	}

	void call::OnCleared()
	{
		auto* e = get_event();
		if (e)
		{
			e->on_cleared();
		}
		OpalCall::OnCleared();
	}


	void call::OnMediaFrameReceived(const OpalMediaStream& mediaStream, const uint8_t* frame, size_t size, uint64_t ts)
	{
		auto* e = get_event();
		if (e)
		{
			e->on_media_frame_received(mediaStream,frame,size,ts);
		}
	}

	bool call::OnCreateMediaStream(const OpalMediaFormat& mediaFormat, unsigned sessionID)
	{
		auto* e = get_event();
		if (e)
		{
			return e->on_create_media_stream(mediaFormat, sessionID);
		}
		else
		{
			return true;
		}
	}

	void call::OnClosedMediaStream(const OpalMediaStream& stream)
	{
		auto* e = get_event();
		if (e)
		{
			e->on_closed_media_stream(stream);
		}
	}
}
