/**
 * @file sender_video_av1.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */



#include "sender_video_av1.h"
#include "../av1/obu.h"
#include <string.h>

namespace rtpx
{
    sender_video_av1::sender_video_av1(uint8_t pt, uint32_t ssrc, media_type_t mt, const sdp_format& fmt, spdlogger_ptr log)
		:sender(pt,ssrc,mt,fmt,log)
	{
	}

    sender_video_av1::~sender_video_av1()
	{
	}


	bool sender_video_av1::send_frame(const uint8_t* frame, uint32_t size, uint32_t duration)
	{
        std::unique_lock<std::shared_mutex>lk(mutex_);

        if (size + 1 <= MAX_RTP_PAYLOAD_SIZE)
        {
            obu::aggr_header_t aggr = {};
            aggr.w = 1;
            aggr.n = 1;
            size_t payload_size = size + 1;
            uint8_t* payload = new uint8_t[payload_size];
            payload[0] = obu::encode_aggr_header(&aggr);
            memcpy(payload + 1, frame, size);

            packet_ptr pkt = std::make_shared<packet>((uint8_t)format_.pt, ssrc_, seq_, timestamp_);
            pkt->mutable_header()->m = 1;
            pkt->set_payload(payload, payload_size);
            this->send_packet(pkt);
            delete[] payload;
        }
        else
        {
            uint32_t offset = 0;
            while (offset < size)
            {
                uint32_t chunk = std::min<uint32_t>(MAX_RTP_PAYLOAD_SIZE - 1, size - offset); // -1 for payload descriptor
                bool first = (offset == 0);
                bool last = (offset + chunk >= size);

                obu::aggr_header_t aggr = {};
                aggr.w = 1;
                if (first) {
                    aggr.n = 1;
                }
                else {
                    aggr.z = 1;
                }
                if (!last) {
                    aggr.y = 1;
                }

                size_t payload_size = chunk + 1;
                uint8_t* payload = new uint8_t[payload_size];

                payload[0] = obu::encode_aggr_header(&aggr);
                
                memcpy(payload + 1, frame+offset, chunk);

                packet_ptr pkt = std::make_shared<packet>((uint8_t)format_.pt, ssrc_, seq_, timestamp_);
                pkt->mutable_header()->m = last ? 1 : 0; // Set marker bit for the last packet in the frame.
                pkt->set_payload(payload, payload_size);
                this->send_packet(pkt);
                delete[] payload;

                offset += chunk;
            }
        }

        timestamp_ += duration;



        return true;
	}

}
