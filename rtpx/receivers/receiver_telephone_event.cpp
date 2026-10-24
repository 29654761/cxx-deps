/**
 * @file receiver_telephone_event.cpp
 * @brief RFC 4733 telephone-event (DTMF) receiver.
 * @author Shijie Zhou
 * @copyright 2026 Shijie Zhou
 */

#include "receiver_telephone_event.h"

#include "../rtpx_def.h"
#include "../util/sn.hpp"

namespace rtpx
{
	receiver_telephone_event::receiver_telephone_event(asio::io_context& ioc, int32_t ssrc, media_type_t mt, const sdp_format& fmt, spdlogger_ptr log)
		:receiver(ioc, ssrc, mt, fmt, log)
	{
		// 按键事件丢了也不重传（结束包本身就有冗余），关闭 NACK
		nack_max_count_ = 0;
	}

	receiver_telephone_event::~receiver_telephone_event()
	{
		stop();
	}

	bool receiver_telephone_event::start()
	{
		active_ = true;
		return true;
	}

	bool receiver_telephone_event::insert_packet(packet_ptr pkt)
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);

		// 对端换了 SSRC（重启/切换流），按键跟踪状态作废
		if (has_current_ && ssrc_ != pkt->header()->ssrc)
		{
			has_current_ = false;
		}

		// 复用基类做 SSRC 修正和收包统计
		if (!receiver::insert_packet(pkt))
		{
			return false;
		}
		// 基类会把包放进重组缓冲区，按键包不需要组帧，直接释放
		recv_packs_[pkt->header()->seq % PACKET_BUFFER_SIZE].reset();

		// RFC 4733 每个事件块 4 字节
		const uint8_t* payload = pkt->payload();
		if (!payload || pkt->payload_size() < 4)
		{
			return false;
		}

		uint8_t event = payload[0];
		bool end = (payload[1] & 0x80) != 0;
		uint32_t duration = ((uint32_t)payload[2] << 8) | payload[3];
		uint32_t ts = pkt->header()->ts;

		if (has_current_ && ts != current_ts_)
		{
			// 比当前事件还旧的乱序包，丢弃
			if (sn::ahead_of<uint32_t>(current_ts_, ts))
			{
				return true;
			}

			// 新的按键开始了，但上一次按键的结束包全丢了：补报上一次按键
			if (!current_reported_)
			{
				if (log_)
				{
					log_->debug("telephone-event end packet lost, event={} ts={}", (unsigned)current_event_, current_ts_)->flush();
				}
				report_current();
			}
			has_current_ = false;
		}

		if (!has_current_)
		{
			has_current_ = true;
			current_ts_ = ts;
			current_event_ = event;
			current_duration_ = 0;
			current_reported_ = false;
		}

		if (duration > current_duration_)
		{
			current_duration_ = duration;
		}

		// 收到结束包且尚未上报：上报一次；之后重复的结束包按时间戳去重直接忽略
		if (end && !current_reported_)
		{
			report_current();
		}

		return true;
	}

	void receiver_telephone_event::report_current()
	{
		current_reported_ = true;

		// duration 是 RTP 时间戳单位，按协商的采样率换算为毫秒（telephone-event 一般为 8000）
		int frequency = format_.frequency > 0 ? format_.frequency : 8000;
		uint32_t duration_ms = (uint32_t)((uint64_t)current_duration_ * 1000 / frequency);

		on_dtmf(ssrc_, format_, current_event_, duration_ms);
	}

}
