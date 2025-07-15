/**
 * @file receiver_rtx.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#include "receiver_rtx.h"

namespace rtpx
{
	receiver_rtx::receiver_rtx(asio::io_context& ioc, int32_t ssrc,media_type_t mt, const sdp_format& fmt, receiver_ptr apt, spdlogger_ptr log)
		:receiver(ioc,ssrc,mt,fmt,log)
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

		packet_ptr pkt2 = std::make_shared<packet>(apt_->format().pt, apt_->ssrc(), seq, pkt->header()->ts);
		pkt2->set_payload(pkt->payload() + 2, pkt->payload_size() - 2);

		if (log_)
		{
			log_->trace("Recv RTX rtx_ssrc={} rtp_ssrc={} pt={} apt={} seq={}",
				pkt->header()->ssrc,pkt->header()->pt,pkt2->header()->ssrc,pkt2->header()->pt,seq)->flush();
		}
		apt_->insert_packet(pkt2);
		return true;
	}
}