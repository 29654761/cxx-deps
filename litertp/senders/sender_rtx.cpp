/**
 * @file sender_rtx.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */


#include "sender_rtx.h"

namespace litertp
{
	sender_rtx::sender_rtx(uint8_t rtx_pt, uint32_t rtx_ssrc, media_type_t mt, const sdp_format& fmt)
		:sender(rtx_pt, rtx_ssrc,mt,fmt)
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
		uint16_t ori_seq = pkt->handle_->header->seq;
		packet_ptr pkt2 = std::make_shared<packet>(pt_, ssrc_, seq_, pkt->handle_->header->ts);

		std::string payload;
		payload.reserve(pkt->payload_size() + 2);
		payload.push_back(static_cast<uint8_t>(ori_seq >> 8));
		payload.push_back(static_cast<uint8_t>(ori_seq));
		payload.append((const char*)pkt->payload(),pkt->payload_size());
		pkt2->set_payload((const uint8_t*)payload.data(),payload.size());
		send_rtp_packet_event_.invoke(pkt2);
		seq_ = pkt2->handle_->header->seq + 1;
		return true;
	}




}
