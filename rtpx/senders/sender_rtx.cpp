/**
 * @file sender_rtx.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */


#include "sender_rtx.h"

namespace rtpx
{
	sender_rtx::sender_rtx(uint8_t rtx_pt, uint32_t rtx_ssrc, media_type_t mt, const sdp_format& fmt, spdlogger_ptr log)
		:sender(rtx_pt, rtx_ssrc,mt,fmt, log)
	{
	}

	sender_rtx::~sender_rtx()
	{
	}


	bool sender_rtx::send_frame(const uint8_t* frame, uint32_t size, uint32_t duration)
	{
		return false;
	}

	bool sender_rtx::send_packet(packet_ptr pkt)
	{
		std::unique_lock<std::shared_mutex>lk(mutex_);
		uint16_t ori_seq = pkt->header()->seq;
		packet_ptr pkt2 = std::make_shared<packet>(pt_, ssrc_, seq_, pkt->header()->ts);

		std::string payload;
		payload.reserve(pkt->payload_size() + 2);
		payload.push_back(static_cast<uint8_t>(ori_seq >> 8));
		payload.push_back(static_cast<uint8_t>(ori_seq));
		payload.append((const char*)pkt->payload(),pkt->payload_size());
		pkt2->set_payload((const uint8_t*)payload.data(),payload.size());
		this->sender_(pkt2);
		seq_ = pkt2->header()->seq + 1;

		if (log_)
		{
			log_->trace("Send RTX packet,rtp_ssrc={} rtx_ssrc={} pt={} apt={} seq={}",
				pkt->header()->ssrc,pkt2->header()->ssrc,pkt->header()->pt,pkt2->header()->pt,ori_seq)->flush();
		}
		return true;
	}




}
