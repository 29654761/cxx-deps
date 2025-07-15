#pragma once
#include "opal_event.h"
#include "local/connection.h"
#include <opal/call.h>
#include <shared_mutex>

namespace voip
{
	class call:public OpalCall
	{
		PCLASSINFO(OpalCall, PSafeObject);
	public:

		typedef void(*frame_event_t)(void* ctx,const OpalMediaStream* mediaStream, RTP_DataFrame* frame);

		call(OpalManager& manager);
		~call();
		void set_event(opal_call_event* e);

		PSafePtr<local::connection> find_local_connection();
		PSafePtr<OpalConnection> find_remote_connection();

		bool input_audio_frame(const uint8_t* frame, uint32_t size, uint32_t duration);
		bool input_video_frame(const uint8_t* frame, uint32_t size, uint32_t duration);

		bool require_keyframe();

		//virtual OpalConnection::AnswerCallResponse OnAnswerCall(OpalConnection& connection,const PString& caller);
		virtual void OnEstablishedCall();
		virtual PBoolean OnEstablished(
			OpalConnection& connection   ///<  Connection that indicates it is alerting
		);
		virtual void OnAlerting(OpalConnection& connection, bool withMedia);
		virtual void OnCleared();
		virtual void OnMediaFrameReceived(const OpalMediaStream& mediaStream, const uint8_t* frame, size_t size, uint64_t ts);
		
		bool OnCreateMediaStream(const OpalMediaFormat& mediaFormat, unsigned sessionID);
		void OnClosedMediaStream(const OpalMediaStream& stream);

	protected:
		opal_call_event* get_event();
	protected:
		std::shared_mutex event_mutex_;
		opal_call_event* event_ = nullptr;

	};

}

