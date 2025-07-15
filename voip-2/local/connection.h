#pragma once
#include "../manager.h"
#include "../h264/rtp_h264_receiver.h"
#include <ep/localep.h>
#include <sys2/blocking_queue.hpp>

namespace voip
{
	namespace local
	{
		class connection :public OpalLocalConnection
		{
			PCLASSINFO(connection, OpalLocalConnection)
		public:
			connection(
				OpalCall& call,              ///<  Owner call for connection
				OpalLocalEndPoint& endpoint, ///<  Owner endpoint for connection
				void* userData,              ///<  Arbitrary data to pass to connection
				unsigned options,             ///< Option bit mask to pass to connection
				OpalConnection::StringOptions* stringOptions, ///< Options to pass to connection
				char tokenPrefix = 'L'        ///< Prefix for token generation
			);
			~connection();

			bool has_audio_stream()const { return has_audio_stream_; }
			bool has_video_stream()const { return has_video_stream_; }

			virtual void AdjustMediaFormats(
				bool local,                             ///<  Media formats a local ones to be presented to remote
				const OpalConnection* otherConnection, ///<  Other connection we are adjusting media for
				OpalMediaFormatList& mediaFormats      ///<  Media formats to use
			) const;

			virtual OpalMediaStream* CreateMediaStream(
				const OpalMediaFormat& mediaFormat, ///<  Media format for stream
				unsigned sessionID,                  ///<  Session number for stream
				PBoolean isSource                    ///<  Is a source stream
			);

			virtual void OnClosedMediaStream(
				const OpalMediaStream& stream     ///<  Media stream being closed
			);

			virtual bool OnReadMediaData(OpalMediaStream& mediaStream, void* data, PINDEX size, PINDEX& length);
			virtual bool OnWriteMediaData(const OpalMediaStream& mediaStream, const void* data, PINDEX length, PINDEX& written);

			
			virtual bool OnReadMediaFrame(
				const OpalMediaStream& mediaStream,    ///<  Media stream data is required for
				RTP_DataFrame& frame                   ///<  RTP frame for data
			);

			virtual bool OnWriteMediaFrame(
				const OpalMediaStream& mediaStream,    ///<  Media stream data is required for
				RTP_DataFrame& frame                   ///<  RTP frame for data
			);

			


			bool input_audio_packet(const BYTE* payload, PINDEX size, bool marker, uint32_t timestamp);
			bool input_video_packet(const BYTE* payload, PINDEX size, bool marker, uint32_t timestamp);

			bool input_audio_frame(const uint8_t* frame, uint32_t size, uint32_t duration);
			bool input_video_frame(const uint8_t* frame, uint32_t size, uint32_t duration);

			bool require_keyframe();
		private:
			void send_h264_nal(uint32_t duration, const uint8_t* nal, uint32_t nal_size, bool islast);
			static void s_frame_event(void* ctx, const uint8_t* frame, size_t size, int64_t ts);
		private:
			std::recursive_mutex mutex_;
			int max_rtp_packet_size_ = 1200;
			rtp_h264_recevier h264_receiver_;

			bool has_audio_stream_ = false;
			bool has_video_stream_ = false;

			std::atomic<uint32_t> video_session_id_;
			std::atomic<uint32_t> video_sync_source_id_;
			
			uint32_t video_ts_ = 0;
			uint32_t audio_ts_ = 0;
			WORD video_seq_ = 0;
			WORD audio_seq_ = 0;
			sys::blocking_queue<RTP_DataFrame> video_queue_;
			sys::blocking_queue<RTP_DataFrame> audio_queue_;

		};

	}
}

