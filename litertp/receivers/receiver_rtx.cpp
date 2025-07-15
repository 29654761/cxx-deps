/**
 * @file receiver_rtx.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#include "receiver_rtx.h"

namespace litertp
{
	receiver_rtx::receiver_rtx(int32_t ssrc,media_type_t mt, const sdp_format& fmt, receiver_ptr apt)
		:receiver(ssrc,mt,fmt)
	{
		apt_ = apt;
	}

	receiver_rtx::~receiver_rtx()
	{
	}

	bool receiver_rtx::start()
	{
		if (active_)
			return true;
		active_ = true;
		return true;
	}

	void receiver_rtx::stop()
	{
		active_ = false;
	}

	bool receiver_rtx::insert_packet(packet_ptr pkt)
	{
		if (pkt->payload_size() < 2)
			return false;

		uint16_t seq = 0;
		seq = pkt->payload()[0]<<8| pkt->payload()[1];

		packet_ptr pkt2 = std::make_shared<packet>(apt_->format().pt, ssrc_, seq, pkt->handle_->header->ts);
		pkt2->set_payload(pkt->payload() + 2, pkt->payload_size() - 2);

		apt_->insert_packet(pkt2);
		return true;
	}
}