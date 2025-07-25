/**
 * @file sender_audio_aac.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */



#include "sender_audio_aac.h"

namespace litertp
{
	sender_audio_aac::sender_audio_aac(uint8_t pt, uint32_t ssrc, media_type_t mt, const sdp_format& fmt)
		:sender(pt,ssrc,mt,fmt)
	{
	}

	sender_audio_aac::~sender_audio_aac()
	{
	}


	bool sender_audio_aac::send_frame(const uint8_t* frame, uint32_t size, uint32_t duration)
	{
		std::unique_lock<std::shared_mutex>lk(mutex_);

		if (format_.codec == codec_type_mpeg4_generic)
		{
			return send_frame_rfc3640(frame,size,duration);
		}
		else if (format_.codec == codec_type_mp4a_latm)
		{
			return send_frame_rfc3016(frame, size, duration);
		}
		else 
		{
			return false;
		}
	}

	bool sender_audio_aac::send_frame_rfc3640(const uint8_t* frame, uint16_t size, uint32_t duration)
	{
		packet_ptr pkt = std::make_shared<packet>((uint8_t)format_.pt, ssrc_, seq_, timestamp_);
		pkt->handle_->header->m = 1;

		std::string data;
		data.reserve(MAX_RTP_PAYLOAD_SIZE);

		// one au header
		data.append(1,(char)0x00);
		data.append(1,(char)0x10);

		// 13bits au data size
		uint16_t c = size << 3;
		data.append(1,(char)((c & 0xFF00) >> 8));
		data.append(1, (char)(c & 0x00F8));

		// au data
		data.append((const char*)frame, size);

		pkt->set_payload((const uint8_t*)data.data(), data.size());


		timestamp_ += duration;

		return send_packet(pkt);

	}

	bool sender_audio_aac::send_frame_rfc3016(const uint8_t* frame, uint16_t size, uint32_t duration)
	{
		packet_ptr pkt = std::make_shared<packet>((uint8_t)format_.pt, ssrc_, seq_, timestamp_);

		pkt->handle_->header->m = 1;

		std::string data;
		data.reserve(MAX_RTP_PAYLOAD_SIZE);

		//au size
		int c=size / 0xFF;
		for (int i = 0; i < c; i++)
		{
			data.append(1, (char)0xFF);
		}
		data.append(1, (char)(size % 0xFF));

		// au data
		data.append((const char*)frame, size);

		pkt->set_payload((const uint8_t*)data.data(), data.size());
		
		timestamp_ += duration;

		return send_packet(pkt);
	}


}
