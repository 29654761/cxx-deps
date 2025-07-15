/**
 * @file sender_video_vp8.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */



#include "sender_video_vp8.h"
#include "../vpx/vpx_header.h"

#include <string.h>

namespace litertp
{
    sender_video_vp8::sender_video_vp8(uint8_t pt, uint32_t ssrc, media_type_t mt, const sdp_format& fmt)
		:sender(pt,ssrc,mt,fmt)
	{
	}

    sender_video_vp8::~sender_video_vp8()
	{
	}


	bool sender_video_vp8::send_frame(const uint8_t* frame, uint32_t size, uint32_t duration)
	{
        std::unique_lock<std::shared_mutex>lk(mutex_);

        for (size_t index = 0; index * MAX_RTP_PAYLOAD_SIZE < size; index++)
        {
            size_t offset = index * MAX_RTP_PAYLOAD_SIZE;
            size_t payload_length = (offset + MAX_RTP_PAYLOAD_SIZE < size) ? MAX_RTP_PAYLOAD_SIZE : size - offset;

            uint8_t vp8_header = (index == 0) ? 0x10 : 0x00;
            uint8_t* payload = new uint8_t[payload_length + 1];
            payload[0] = vp8_header;
            memcpy(payload + 1, frame + offset, payload_length);

            packet_ptr pkt = std::make_shared<packet>((uint8_t)format_.pt, ssrc_, seq_, timestamp_);
            pkt->handle_->header->m = ((offset + payload_length) >= size) ? 1 : 0; // Set marker bit for the last packet in the frame.
            pkt->set_payload(payload, payload_length + 1);
            this->send_packet(pkt);
            delete[] payload;
        }

        timestamp_ += duration;

        return true;
	}

}
