/**
 * @file transport.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#include "transport.h"
#include "../proto/rtcp_header.h"
#include "../log.h"

#include <sys2/util.h>

namespace litertp {

	transport::transport()
	{
		tie_breaker_ = sys::util::random_number<uint64_t>(0, 0xFFFFFFFFFFFFFFFF);
	}

	transport::~transport()
	{
	}


	bool transport::start()
	{
		if (active_) {
			return true;
		}
		active_ = true;

		return true;
	}

	void transport::stop()
	{
		active_ = false;
		sctp_.reset();
	}

	bool transport::test_rtcp_packet(const uint8_t* data, int size, int* pt)
	{
		rtcp_header hdr = { 0 };
		int ptv = rtcp_header_parse(&hdr, (const uint8_t*)data, size);
		if (pt)
		{
			*pt = ptv;
		}
		if (ptv < 0)
		{
			return false;
		}
		if (ptv == rtcp_packet_type::RTCP_APP ||
			ptv == rtcp_packet_type::RTCP_BYE ||
			ptv == rtcp_packet_type::RTCP_RR ||
			ptv == rtcp_packet_type::RTCP_SR ||
			ptv == rtcp_packet_type::RTCP_SDES ||
			ptv == rtcp_packet_type::RTCP_PSFB ||
			ptv == rtcp_packet_type::RTCP_RTPFB)
		{
			return true;
		}


		return false;
	}

	bool transport::receive_rtp_packet(const uint8_t* rtp_packet, int size)
	{
		auto pkt = std::make_shared<packet>();
		if (!pkt->parse(rtp_packet, size)) 
		{
			return false;
		}

		rtp_packet_event_.invoke(socket_, pkt, nullptr, 0);
		return true;
	}

	bool transport::receive_rtp_packet(const packet_ptr pkt)
	{
		rtp_packet_event_.invoke(socket_, pkt, nullptr, 0);
		return true;
	}

	bool transport::receive_rtcp_packet(const uint8_t* rtcp_packet, int size)
	{
		rtcp_header hdr = { 0 };
		int pt = rtcp_header_parse(&hdr, rtcp_packet, size);
		if (pt >= 0)
		{
			return false;
		}
		rtcp_packet_event_.invoke(socket_, (uint16_t)pt, rtcp_packet, size, nullptr, 0);
		return true;
	}

	void transport::init_sctp(int local_port)
	{
		if (!sctp_)
		{
			sctp_ = std::make_shared<sctp>();
			sctp_->on_sctp_send_data.add(s_send_sctp_packet, this);
			sctp_->open(local_port);
		}
	}

	void transport::s_send_sctp_packet(void* ctx, const std::vector<uint8_t>& data, const sockaddr* addr, socklen_t addr_size)
	{
		transport* p = static_cast<transport*>(ctx);
		if (p->sctp_)
		{
			p->send_raw_packet(data.data(), data.size(), addr, addr_size);
		}
		
	}

	void transport::on_dtls_handshake(srtp_role_t role)
	{
		if (sctp_)
		{
			sctp_->set_init_stream_id(sdp_type_ == sdp_type_offer ? 2 : 1);
		}
		if (role == srtp_role_client)
		{
			if (sctp_)
			{
				sctp_->connect(sctp_remote_port_);
			}
		}
		dtls_handshake_event_.invoke(srtp_role_);

		this->connected_event_.invoke(port_);
	}

	void transport::on_app_data(const uint8_t* data, int size, const sockaddr* addr, int addr_size)
	{
		if (sctp_)
		{
			sctp_->conn_input(data, size);
		}
		else
		{
			raw_packet_event_.invoke(socket_, data, size, (sockaddr*)&addr, addr_size);
		}
	}
}


