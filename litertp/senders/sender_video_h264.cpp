/**
 * @file sender_video_h264.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */



#include "sender_video_h264.h"
#include "../h264/h264.h"

namespace litertp
{
	sender_video_h264::sender_video_h264(uint8_t pt, uint32_t ssrc, media_type_t mt, const sdp_format& fmt)
		:sender(pt,ssrc,mt,fmt)
	{
	}

	sender_video_h264::~sender_video_h264()
	{
	}

	bool sender_video_h264::send_frame(const uint8_t* frame, uint32_t size, uint32_t duration)
	{
        std::unique_lock<std::shared_mutex>lk(mutex_);
        size_t offset = 0;
        size_t nal_start = 0, nal_size = 0;
        bool islast = false;
        std::vector<std::string> nals;
        while (h264::find_next_nal(frame, size, offset, nal_start, nal_size, islast))
        {
            std::string nal;
            nal.assign((const char*)(frame + nal_start), nal_size);
            nals.push_back(nal);
            //send_nal(duration, frame + nal_start, nal_size, islast);
        }

        size_t total_nal_size = 0;
        for (auto itr = nals.begin(); itr != nals.end(); itr++)
        {
            total_nal_size += itr->size();
        }

        if (total_nal_size <= MAX_RTP_PAYLOAD_SIZE&&nals.size()>1)
        {
            std::string nal = nals[0];
            uint8_t nal0 = nal[0];
            h264::nal_header_t nal_header;
            h264::nal_header_set(&nal_header, nal0);
            h264::nal_header_t fu_ind;
            fu_ind.f = nal_header.f;
            fu_ind.nri = nal_header.nri;
            fu_ind.t = 24;  //STAP-A

            uint8_t fu_ind_v = h264::nal_header_get(&fu_ind);

            packet_ptr pkt = std::make_shared<packet>((uint8_t)format_.pt, ssrc_, seq_, timestamp_);
            std::string payload;
            payload.append(1, fu_ind_v);
            for (auto itr = nals.begin(); itr != nals.end(); itr++)
            {
                uint16_t len = (uint16_t)itr->size();
                len = sys::socket::hton16(len);
                payload.append((const char*)&len, 2);
                payload.append(*itr);
            }

            if (nal_header.t == 7 || nal_header.t == 8)
            {
                pkt->handle_->header->m = 0;
            }
            else
            {
                pkt->handle_->header->m = 1;
            }
            pkt->set_payload((const uint8_t*)payload.data(), payload.size());
            this->send_packet(pkt);
        }
        else
        {
            for (auto itr = nals.begin(); itr != nals.end(); itr++)
            {
                islast = itr == (nals.end() - 1);
                send_nal(duration, (const uint8_t*)itr->data(), (uint32_t)itr->size(),islast);
            }
        }
        return true;
	}
    

    void sender_video_h264::send_nal(uint32_t duration, const uint8_t* nal, uint32_t nal_size, bool islast)
    {
        if (nal_size <=MAX_RTP_PAYLOAD_SIZE)
        {
            // Send as Single-Time Aggregation Packet (STAP-A).
            packet_ptr pkt = std::make_shared<packet>((uint8_t)format_.pt, ssrc_, seq_, timestamp_);
            pkt->handle_->header->m = islast ? 1 : 0;
            pkt->set_payload(nal, nal_size);
            this->send_packet(pkt);
        }
        else // Send as Fragmentation Unit A (FU-A):
        {
            uint8_t nal0 = nal[0];
            h264::nal_header_t nal_header;
            h264::nal_header_set(&nal_header, nal0);
            uint32_t pos = 1; //skip nal0
            nal_size--;

            h264::nal_header_t fu_ind;
            fu_ind.f = nal_header.f;
            fu_ind.nri = nal_header.nri;
            fu_ind.t = 28;  //FU-A
            uint8_t fu_ind_v = h264::nal_header_get(&fu_ind);


            for (uint32_t index = 0; index * MAX_RTP_PAYLOAD_SIZE < nal_size; index++)
            {
                uint32_t offset = index * MAX_RTP_PAYLOAD_SIZE;
                uint32_t payload_length = ((index + 1) * MAX_RTP_PAYLOAD_SIZE < nal_size) ? MAX_RTP_PAYLOAD_SIZE : nal_size - index * MAX_RTP_PAYLOAD_SIZE;
                
                bool is_first_packet = index == 0;
                bool is_final_packet = (index + 1) * MAX_RTP_PAYLOAD_SIZE >= nal_size;

                packet_ptr pkt = std::make_shared<packet>((uint8_t)format_.pt, ssrc_, seq_, timestamp_);
                pkt->handle_->header->m= (islast && is_final_packet) ? 1 : 0;

                h264::fu_header_t fu;
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
                s.reserve(payload_length+2);
                s.append(1, fu_ind_v);
                s.append(1, h264::fu_header_get(&fu));
                s.append((const char*)nal + pos, payload_length);
                pos += payload_length;
                pkt->set_payload((const uint8_t*)s.data(), s.size());

                this->send_packet(pkt);

            }
        }

        if (islast)
        {
            timestamp_ += duration;
        }
    }

}
