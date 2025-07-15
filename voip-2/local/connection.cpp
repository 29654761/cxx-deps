#include "connection.h"
#include "../call.h"
#include "../h323/connection.h"
#include "../sip/connection.h"
#include "../h264/h264.h"
#include <cassert>
#include <ptclib/random.h>
#include <sys2/socket.h>
#include <rtp/rtp_stream.h>

namespace voip
{
	namespace local
	{

		connection::connection(
			OpalCall& call,              ///<  Owner call for connection
			OpalLocalEndPoint& endpoint, ///<  Owner endpoint for connection
			void* userData,              ///<  Arbitrary data to pass to connection
			unsigned options,             ///< Option bit mask to pass to connection
			OpalConnection::StringOptions* stringOptions, ///< Options to pass to connection
			char tokenPrefix        ///< Prefix for token generation
		) :OpalLocalConnection(call, endpoint, userData, options, stringOptions, tokenPrefix)
			,video_session_id_(0)
			, video_sync_source_id_(0)
		{
			// We disabled mtu discover mode ,so set packet size slightly smaller 1200
			//max_rtp_packet_size_ = endpoint.GetManager().GetMaxRtpPayloadSize();
			h264_receiver_.set_max_rtp_packet_size(max_rtp_packet_size_);
			h264_receiver_.frame_event.add(s_frame_event, this);
		}

		connection::~connection()
		{
		}


		

		void connection::AdjustMediaFormats(
			bool local,                             ///<  Media formats a local ones to be presented to remote
			const OpalConnection* otherConnection, ///<  Other connection we are adjusting media for
			OpalMediaFormatList& mediaFormats      ///<  Media formats to use
		) const
		{
			OpalLocalConnection::AdjustMediaFormats(local, otherConnection, mediaFormats);
		}

		OpalMediaStream* connection::CreateMediaStream(
			const OpalMediaFormat& mediaFormat, ///<  Media format for stream
			unsigned sessionID,                  ///<  Session number for stream
			PBoolean isSource                    ///<  Is a source stream
		)
		{
			if (isSource)
			{
				if (mediaFormat.IsMediaType(OpalMediaType::Video()))
				{
					if (has_video_stream_)
						return nullptr;

					has_video_stream_ = true;

					voip::call& call = (voip::call&)this->GetCall();
					if (!call.OnCreateMediaStream(mediaFormat, sessionID))
						return nullptr;

					video_queue_.reset();
				}
				else if (mediaFormat.IsMediaType(OpalMediaType::Audio()))
				{
					if (has_audio_stream_)
						return nullptr;
					has_audio_stream_ = true;

					voip::call& call = (voip::call&)this->GetCall();
					if (!call.OnCreateMediaStream(mediaFormat, sessionID))
						return nullptr;

					audio_queue_.reset();
				}
			}
			return OpalLocalConnection::CreateMediaStream(mediaFormat,sessionID,isSource);
		}

		void connection::OnClosedMediaStream(
			const OpalMediaStream& stream     ///<  Media stream being closed
		)
		{
			voip::call& call = (voip::call&)this->GetCall();
			call.OnClosedMediaStream(stream);

			OpalMediaFormat fmt = stream.GetMediaFormat();
			if (fmt.IsMediaType(OpalMediaType::Video()))
			{
				video_queue_.close();
				has_video_stream_ = false;
			}
			else if (fmt.IsMediaType(OpalMediaType::Audio()))
			{
				audio_queue_.close();
				has_audio_stream_ = false;
			}
			OpalLocalConnection::OnClosedMediaStream(stream);
		}


		bool connection::OnReadMediaData(OpalMediaStream& mediaStream, void* data, PINDEX size, PINDEX& length)
		{
			//return OpalLocalConnection::OnReadMediaData(mediaStream, data, size, length);
			return false; //call here when closing.
		}

		bool connection::OnWriteMediaData(const OpalMediaStream& mediaStream, const void* data, PINDEX length, PINDEX& written)
		{
			// Nerver be called here
			OpalMediaFormat fmt = mediaStream.GetMediaFormat();
			int pt = fmt.GetPayloadType();
			printf("recv %s pt=%d %llu\n", fmt.GetName().c_str(), pt, length);
			return OpalLocalConnection::OnWriteMediaData(mediaStream, data, length, written);
		}
		
		bool connection::OnReadMediaFrame(
			const OpalMediaStream& mediaStream,    ///<  Media stream data is required for
			RTP_DataFrame& frame                   ///<  RTP frame for data
		)
		{
			auto fmt = mediaStream.GetMediaFormat();
			
			if (fmt.GetMediaType() == OpalMediaType::Video())
			{
				if (!video_queue_.pop(frame))
				{
					return false;
				}
				frame.SetPayloadType(fmt.GetPayloadType());
				frame.SetSequenceNumber(video_seq_);
				video_seq_++;
			}
			else if (fmt.GetMediaType() == OpalMediaType::Audio())
			{
				if (!audio_queue_.pop(frame))
				{
					return false;
				}
				frame.SetPayloadType(fmt.GetPayloadType());
				frame.SetSequenceNumber(audio_seq_);
				audio_seq_++;
			}
			
			return true;
		}

		bool connection::OnWriteMediaFrame(
			const OpalMediaStream& mediaStream,    ///<  Media stream data is required for
			RTP_DataFrame& frame                   ///<  RTP frame for data
		)
		{
			if (frame.GetPayloadSize() == 0)
			{
				return true;
			}
			auto fmt=mediaStream.GetMediaFormat();
			
			//printf("mt=%s,ms_pt=%d,frame_pt=%d\n", fmt.GetMediaType().c_str(), fmt.GetPayloadType(), frame.GetPayloadType());

			if (fmt.IsMediaType(OpalMediaType::Video())) {

				video_session_id_ = mediaStream.GetSessionID();
				video_sync_source_id_ = frame.GetSyncSource();

				h264_receiver_.insert_packet(frame);
			}
			else if (fmt.IsMediaType(OpalMediaType::Audio()))
			{
				voip::call& call = (voip::call&)this->GetCall();
				call.OnMediaFrameReceived(mediaStream, frame.GetPayloadPtr(), frame.GetPayloadSize(), frame.GetTimestamp());
			}
			return true;
		}

		
		
		bool connection::input_audio_packet(const BYTE* payload,PINDEX size,bool marker,uint32_t timestamp)
		{
			RTP_DataFrame frame;
			frame.SetPayload(payload, size);
			frame.SetMarker(marker);
			frame.SetTimestamp(timestamp);

			return audio_queue_.push(frame);
		}

		bool connection::input_video_packet(const BYTE* payload, PINDEX size, bool marker, uint32_t timestamp)
		{
			RTP_DataFrame frame;
			frame.SetPayload(payload, size);
			frame.SetMarker(marker);
			frame.SetTimestamp(timestamp);
			return video_queue_.push(frame);
		}

		bool connection::input_audio_frame(const uint8_t* frame, uint32_t size, uint32_t duration)
		{
			std::lock_guard<std::recursive_mutex>lk(mutex_);
			uint32_t payload_duration = 0;
			for (int index = 0; index * max_rtp_packet_size_ < size; index++)
			{
				int offset = (index == 0) ? 0 : (index * max_rtp_packet_size_);
				int payload_length = (offset + max_rtp_packet_size_ < size) ? max_rtp_packet_size_ : size - offset;

				// RFC3551 specifies that for audio the marker bit should always be 0 except for when returning
				// from silence suppression. For video the marker bit DOES get set to 1 for the last packet
				// in a frame.
		
				this->input_audio_packet((const uint8_t*)(frame + offset), payload_length, 0, audio_ts_);

				//logger.LogDebug($"send audio { audioRtpChannel.RTPLocalEndPoint}->{AudioDestinationEndPoint}.");

				payload_duration = (uint32_t)((payload_length * 1.0 / size) * duration);
				audio_ts_ += payload_duration;
			}


			return true;

		}

		bool connection::input_video_frame(const uint8_t* frame, uint32_t size, uint32_t duration)
		{
			std::lock_guard<std::recursive_mutex>lk(mutex_);
			int offset = 0;
			int nal_start = 0, nal_size = 0;
			bool islast = false;
			std::vector<std::string> nals;
			while (h264::find_next_nal(frame, size, offset, nal_start, nal_size, islast))
			{
				std::string nal;
				nal.assign((const char*)(frame + nal_start), nal_size);
				nals.push_back(nal);
			}

			size_t total_nal_size = 0;
			for (auto itr = nals.begin(); itr != nals.end(); itr++)
			{
				total_nal_size += itr->size();
			}

			if (total_nal_size <= max_rtp_packet_size_ && nals.size() > 1)
			{
				std::string nal = nals[0];
				uint8_t nal0 = nal[0];
				nal_header_t nal_header;
				h264::nal_header_set(&nal_header, nal0);
				nal_header_t fu_ind;
				fu_ind.f = nal_header.f;
				fu_ind.nri = nal_header.nri;
				fu_ind.t = 24;  //STAP-A

				uint8_t fu_ind_v = h264::nal_header_get(&fu_ind);

				
				std::string payload;
				payload.append(1, fu_ind_v);
				for (auto itr = nals.begin(); itr != nals.end(); itr++)
				{
					uint16_t len = (uint16_t)itr->size();
					len = sys::socket::hton16(len);
					payload.append((const char*)&len, 2);
					payload.append(*itr);
				}
				bool marker = false;
				if (nal_header.t == 7 || nal_header.t == 8)
				{
					marker = false;
				}
				else
				{
					marker = true;
				}
				this->input_video_packet((const uint8_t*)payload.data(), payload.size(), marker, video_ts_);
			}
			else
			{
				for (auto itr = nals.begin(); itr != nals.end(); itr++)
				{
					islast = itr == (nals.end() - 1);
					send_h264_nal(duration, (const uint8_t*)itr->data(), itr->size(), islast);
				}
			}
			return true;
		}



		void connection::send_h264_nal(uint32_t duration, const uint8_t* nal, uint32_t nal_size, bool islast)
		{
			if (nal_size <= max_rtp_packet_size_)
			{
				// Send as Single-Time Aggregation Packet (STAP-A).
				this->input_video_packet(nal, nal_size, islast, video_ts_);
			}
			else // Send as Fragmentation Unit A (FU-A):
			{
				uint8_t nal0 = nal[0];
				nal_header_t nal_header;
				h264::nal_header_set(&nal_header, nal0);
				int pos = 1; //skip nal0
				nal_size--;

				nal_header_t fu_ind;
				fu_ind.f = nal_header.f;
				fu_ind.nri = nal_header.nri;
				fu_ind.t = 28;  //FU-A
				uint8_t fu_ind_v = h264::nal_header_get(&fu_ind);


				for (int index = 0; index * max_rtp_packet_size_ < nal_size; index++)
				{
					int offset = index * max_rtp_packet_size_;
					int payload_length = ((index + 1) * max_rtp_packet_size_ < nal_size) ? max_rtp_packet_size_ : nal_size - index * max_rtp_packet_size_;

					bool is_first_packet = index == 0;
					bool is_final_packet = (index + 1) * max_rtp_packet_size_ >= nal_size;

					bool marker = (islast && is_final_packet) ? 1 : 0;
					fu_header_t fu;
					fu.t = nal_header.t;
					if (is_first_packet) {
						fu.s = 1;
						fu.e = 0;
						fu.r = 0;
					}
					else if (is_final_packet) {
						fu.s = 0;
						fu.e = 1;
						fu.r = 0;
					}
					else {
						fu.s = 0;
						fu.e = 0;
						fu.r = 0;
					}

					std::string s;
					s.reserve(payload_length + 2);
					s.append(1, fu_ind_v);
					s.append(1, h264::fu_header_get(&fu));
					s.append((const char*)nal + pos, payload_length);
					pos += payload_length;
					
					this->input_video_packet((const uint8_t*)s.data(), s.size(), marker, video_ts_);
				}
			}

			if (islast)
			{
				video_ts_ += duration;
			}
		}


		void connection::s_frame_event(void* ctx, const uint8_t* frame, size_t size, int64_t ts)
		{
			connection* p = (connection*)ctx;
			voip::call& call=(voip::call&)p->GetCall();

			auto ms=p->GetMediaStream(OpalMediaType::Video(), true);

			call.OnMediaFrameReceived(*ms, frame, size, ts);
		}


		bool connection::require_keyframe()
		{
			auto con = GetOtherPartyConnection();
			OpalRTPSession* sess = nullptr;
			auto* h323con = con.GetObjectAs<h323::connection>();
			if (h323con)
			{
				sess = dynamic_cast<OpalRTPSession*>(h323con->GetMediaSession(video_session_id_));
			}
			auto* sipcon = con.GetObjectAs<sip::connection>();
			if (sipcon)
			{
				sess = dynamic_cast<OpalRTPSession*>(sipcon->GetMediaSession(video_session_id_));
			}
			if (!sess)
			{
				return false;
			}

			sess->SendIntraFrameRequest(OPAL_OPT_VIDUP_METHOD_PLI | OPAL_OPT_VIDUP_METHOD_PREFER_PLI, video_sync_source_id_);
			return true;
		}
	}
}

