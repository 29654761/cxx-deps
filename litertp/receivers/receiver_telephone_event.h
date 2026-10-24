/**
 * @file receiver_telephone_event.h
 * @brief RFC 4733 telephone-event (DTMF) receiver.
 * @author Shijie Zhou
 * @copyright 2026 Shijie Zhou
 */

#pragma once

#include "receiver.h"


namespace litertp
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
	// 本类按 RTP 时间戳去重，每次按键只通过 rtp_dtmf_event_ 回调一次。
	class receiver_telephone_event :public receiver
	{
	public:
		receiver_telephone_event(int32_t ssrc, media_type_t mt, const sdp_format& fmt);
		virtual ~receiver_telephone_event();

		// 不需要 NACK，不启动基类的 nack 线程
		virtual bool start();
		virtual bool insert_packet(packet_ptr pkt);
	private:
		// 把当前按键回调出去（duration 换算为毫秒）
		void report_current();
	private:
		bool has_current_ = false;       // 是否正在跟踪一个按键事件
		uint32_t current_ts_ = 0;        // 当前按键事件的 RTP 时间戳（同一次按键所有包相同）
		uint8_t current_event_ = 0;      // 当前按键的事件码（0-9、10=*、11=#、12-15=A-D、16=Flash）
		uint32_t current_duration_ = 0;  // 当前按键已知的最大持续时长（RTP 时间戳单位）
		bool current_reported_ = false;  // 当前按键是否已经回调过（结束包重复时用于去重）
	};


}
