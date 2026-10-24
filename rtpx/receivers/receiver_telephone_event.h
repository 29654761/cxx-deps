/**
 * @file receiver_telephone_event.h
 * @brief RFC 4733 telephone-event (DTMF) receiver.
 * @author Shijie Zhou
 * @copyright 2026 Shijie Zhou
 */

#pragma once

#include "receiver.h"


namespace rtpx
{
	// 接收 RFC 4733 telephone-event 载荷（电话按键 DTMF）。
	// 载荷格式（4 字节）:
	//   0                   1                   2                   3
	//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//  |     event     |E|R| volume    |          duration             |
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	// 同一次按键的所有包 RTP 时间戳相同；按住期间持续发更新包（duration 递增），
	// 松开后发 E=1 的结束包，且结束包通常重复发 3 次。
	// 本类按 RTP 时间戳去重，每次按键只通过 on_dtmf 回调一次。
	class receiver_telephone_event : public receiver
	{
	public:
		receiver_telephone_event(asio::io_context& ioc, int32_t ssrc, media_type_t mt, const sdp_format& fmt, spdlogger_ptr log);
		virtual ~receiver_telephone_event();

		// 不需要 NACK，不启动基类的 timer
		virtual bool start();
		virtual bool insert_packet(packet_ptr pkt);
	private:
		void report_current();
	private:
		bool has_current_ = false;
		uint32_t current_ts_ = 0;
		uint8_t current_event_ = 0;
		uint32_t current_duration_ = 0;
		bool current_reported_ = false;
	};

}
