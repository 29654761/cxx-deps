/**
 * @file sender_video_h265.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */



#include "sender_video_h265.h"
#include "../h265/h265.h"

namespace litertp
{
    sender_video_h265::sender_video_h265(uint8_t pt, uint32_t ssrc, media_type_t mt, const sdp_format& fmt)
		:sender(pt,ssrc,mt,fmt)
	{
	}

    sender_video_h265::~sender_video_h265()
	{
	}


	bool sender_video_h265::send_frame(const uint8_t* frame, uint32_t size, uint32_t duration)
	{
        std::unique_lock<std::shared_mutex>lk(mutex_);
        size_t offset = 0;
        size_t nal_start = 0, nal_size = 0;
        bool islast = false;
        std::vector<std::string> nals;
        while (h265::find_next_nal(frame, size, offset, nal_start, nal_size, islast))
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
            h265::nal_header_t nal_header;
            h265::nal_header_set(&nal_header, (const uint8_t*)nal.data(),nal.size());
            h265::nal_header_t fu_ind;
            fu_ind.f = nal_header.f;
            fu_ind.layer = nal_header.layer;
            fu_ind.tid = nal_header.tid;
            fu_ind.type = 48;

            uint8_t fu_ind_v[2] = {};
            h265::nal_header_get(&fu_ind, fu_ind_v,2);

            packet_ptr pkt = std::make_shared<packet>((uint8_t)format_.pt, ssrc_, seq_, timestamp_);
            std::string payload;
            payload.append((const char*)fu_ind_v,2);
            for (auto itr = nals.begin(); itr != nals.end(); itr++)
            {
                uint16_t len = (uint16_t)itr->size();
                len = sys::socket::hton16(len);
                payload.append((const char*)&len, 2);
                payload.append(*itr);
            }

            if (nal_header.type == (uint8_t)h265::nal_type_t::pps || nal_header.type == (uint8_t)h265::nal_type_t::sps)
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

    void sender_video_h265::send_nal(uint32_t duration, const uint8_t* nal, uint32_t nal_size, bool islast)
    {
        if (nal_size <=MAX_RTP_PAYLOAD_SIZE)
        {
            // Send as Single-Time Aggregation Packet (STAP-A).
            packet_ptr pkt = std::make_shared<packet>((uint8_t)format_.pt, ssrc_, seq_, timestamp_);
            pkt->handle_->header->m = islast ? 1 : 0;
            pkt->set_payload(nal, nal_size);
            this->send_packet(pkt);
        }
        else // Send as Fragmentation Unit
        {
            h265::nal_header_t nal_header;
            h265::nal_header_set(&nal_header, nal,nal_size);
            uint32_t pos = 2; //skip nal header
            nal_size--;

            h265::nal_header_t fu_ind;
            fu_ind.f = nal_header.f;
            fu_ind.layer = nal_header.layer;
            fu_ind.tid = nal_header.tid;
            fu_ind.type = 49;
            uint8_t fu_ind_v[2] = {};
            h265::nal_header_get(&fu_ind, fu_ind_v,2);

            for (uint32_t index = 0; index * MAX_RTP_PAYLOAD_SIZE < nal_size; index++)
            {
                uint32_t offset = index * MAX_RTP_PAYLOAD_SIZE;
                uint32_t payload_length = ((index + 1) * MAX_RTP_PAYLOAD_SIZE < nal_size) ? MAX_RTP_PAYLOAD_SIZE : nal_size - index * MAX_RTP_PAYLOAD_SIZE;
                
                bool is_first_packet = index == 0;
                bool is_final_packet = (index + 1) * MAX_RTP_PAYLOAD_SIZE >= nal_size;

                packet_ptr pkt = std::make_shared<packet>((uint8_t)format_.pt, ssrc_, seq_, timestamp_);
                pkt->handle_->header->m= (islast && is_final_packet) ? 1 : 0;

                h265::fu_header_t fu;
                fu.t = nal_header.type;
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
                s.append((const char*)fu_ind_v,2);
                s.append(1, h265::fu_header_get(&fu));
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
