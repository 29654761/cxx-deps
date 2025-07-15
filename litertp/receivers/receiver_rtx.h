/**
 * @file receiver_rtx.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#pragma once

#include "../avtypes.h"
#include "../packet.h"
#include "../proto/rtp_source.h"
#include "../proto/rtcp_sr.h"
#include "../proto/rtcp_rr.h"
#include "../sdp/sdp_format.h"
#include "receiver.h"

#include <sys2/callback.hpp>
#include <sys2/signal.h>
#include <shared_mutex>
#include <atomic>
#include <array>
#include <thread>
#include <chrono>


namespace litertp
{

	class receiver_rtx:public receiver
	{
	public:
		receiver_rtx(int32_t ssrc,media_type_t mt,const sdp_format& fmt,receiver_ptr apt);
		virtual ~receiver_rtx();


		virtual bool start();
		virtual void stop();
		virtual bool insert_packet(packet_ptr pkt);

	private:
		receiver_ptr apt_;
	};

}