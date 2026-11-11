#include "call.h"
#include <mutex>
#include <algorithm>

namespace voip
{
	static std::once_flag s_capabilities_once;

	void call::register_default_capabilities()
	{
		std::call_once(s_capabilities_once, []()
		{
			auto& caps = rtpx::media_capabilities::global();
			caps.clear();

			const std::set<std::string> rtcp_fb = { "goog-remb", "ccm fir", "nack", "nack pli" };

			// 音频：G.711 与 RFC 4733 按键事件
			{
				rtpx::sdp_format fmt(0, codec_type_pcmu, 8000);
				fmt.rtcp_fb = rtcp_fb;
				caps.add_format(media_type_audio, fmt);
			}
			{
				rtpx::sdp_format fmt(8, codec_type_pcma, 8000);
				fmt.rtcp_fb = rtcp_fb;
				caps.add_format(media_type_audio, fmt);
			}
			{
				rtpx::sdp_format fmt(101, codec_type_telephone_event, 8000);
				caps.add_format(media_type_audio, fmt);
			}

			// 视频：H264（baseline/main 420028、high 640028）与 H265
			{
				rtpx::sdp_format fmt(96, codec_type_h264, 90000);
				fmt.fmtp.set_packetization_mode(1);
				fmt.fmtp.set_profile_level_id(0x420028);
				fmt.fmtp.set_mbps(245760);
				fmt.fmtp.set_mfs(8192);
				fmt.fmtp.set_mbr(1920);
				fmt.fmtp.set_level_asymmetry_allowed(1);
				fmt.rtcp_fb = rtcp_fb;
				caps.add_format(media_type_video, fmt);
			}
			{
				rtpx::sdp_format fmt(98, codec_type_h264, 90000);
				fmt.fmtp.set_packetization_mode(1);
				fmt.fmtp.set_profile_level_id(0x640028);
				fmt.fmtp.set_mbps(245760);
				fmt.fmtp.set_mfs(8192);
				fmt.fmtp.set_mbr(1920);
				fmt.fmtp.set_level_asymmetry_allowed(1);
				fmt.rtcp_fb = rtcp_fb;
				caps.add_format(media_type_video, fmt);
			}
			{
				rtpx::sdp_format fmt(100, codec_type_h265, 90000);
				fmt.fmtp.set_profile_id(1);
				fmt.fmtp.set_level_id(0x0093);
				fmt.fmtp.set_mbps(245760);
				fmt.fmtp.set_mfs(8192);
				fmt.fmtp.set_mbr(1920);
				fmt.fmtp.set_level_asymmetry_allowed(1);
				fmt.rtcp_fb = rtcp_fb;
				caps.add_format(media_type_video, fmt);
			}
		});
	}

	call::call(const std::string& local_alias, const std::string& remote_alias, direction_t direction,
		const std::string& nat_address, int port, rtpx::port_range_ptr rtp_ports,
		asio::io_context& ioc, spdlogger_ptr log)
		:updated_at_(0)
		, triggered_connected_(false)
		, dtmf_timer_(ioc)
	{
		register_default_capabilities();

		local_alias_ = local_alias;
		remote_alias_ = remote_alias;
		direction_ = direction;
		nat_address_ = nat_address;
		local_port_ = port;
		rtp_ports_ = rtp_ports;
		log_ = log;

		rtp_ = std::make_shared<rtpx::rtp_session>(ioc, log, true, false);
		if (rtp_ports_)
		{
			rtp_->set_port_range(rtp_ports_->min_port(), rtp_ports_->max_port());
		}
		rtp_->on_frame = std::bind(&call::on_rtpx_frame, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
		rtp_->on_receive_require_keyframe = std::bind(&call::on_rtpx_receive_require_keyframe, this, std::placeholders::_1);
		rtp_->on_dtmf = std::bind(&call::on_rtpx_dtmf, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
		update();
	}

	call::~call()
	{
	}


	bool call::pick_send_pt(rtpx::media_stream_ptr ms, uint8_t& pt)
	{
		auto formats = ms->local_formats();
		if (ms->media_type() == media_type_audio)
		{
			// 与旧版本行为保持一致：G.711 优先 PCMA，其次 PCMU
			for (auto& fmt : formats)
			{
				if (fmt.codec == codec_type_pcma)
				{
					pt = fmt.pt;
					return true;
				}
			}
			for (auto& fmt : formats)
			{
				if (fmt.codec == codec_type_pcmu)
				{
					pt = fmt.pt;
					return true;
				}
			}
		}
		for (auto& fmt : formats)
		{
			if (fmt.codec == codec_type_rtx || fmt.codec == codec_type_telephone_event)
			{
				continue;
			}
			pt = fmt.pt;
			return true;
		}
		return false;
	}

	bool call::send_video_frame(const uint8_t* frame, uint32_t size, uint32_t duration, bool is_extend)
	{
		rtpx::media_stream_ptr ms = nullptr;
		if (is_extend)
		{
			ms = rtp_->get_media_stream_by_mid("extend_video");
			if (!ms)
			{
				// 应答方的流 mid 来自对端 offer，辅流不一定叫 "extend_video"，按 RFC 4796 content 标记兜底
				for (auto& m : rtp_->media_streams())
				{
					if (m->media_type() == media_type_video && m->content() == "slides")
					{
						ms = m;
						break;
					}
				}
			}
		}
		else
		{
			ms = rtp_->get_media_stream_by_mid("video");
			if (!ms)
			{
				// 应答方的流 mid 来自对端 offer，不一定叫 "video"，按媒体类型兜底
				for (auto& m : rtp_->media_streams())
				{
					if (m->media_type() == media_type_video && m->content() != "slides")
					{
						ms = m;
						break;
					}
				}
			}
		}
		if (!ms)
		{
			if (log_)
			{
				log_->warn("send_video_frame failed: media stream not found, is_extend={}", is_extend)->flush();
			}
			return false;
		}
		uint8_t pt = 0;
		if (!pick_send_pt(ms, pt))
		{
			return false;
		}
		return ms->send_frame(pt, frame, size, duration);
	}

	bool call::send_audio_frame(const uint8_t* frame, uint32_t size, uint32_t duration)
	{
		rtpx::media_stream_ptr ms;
		for (auto& m : rtp_->media_streams())
		{
			if (m->media_type() == media_type_audio)
			{
				ms = m;
				break;
			}
		}
		if (!ms)
		{
			return false;
		}
		uint8_t pt = 0;
		if (!pick_send_pt(ms, pt))
		{
			return false;
		}
		return ms->send_frame(pt, frame, size, duration);
	}


	void call::on_rtpx_frame(const std::string& mid, const std::string& sid, const std::string& tid, const rtpx::sdp_format& fmt, const av_frame_t& frame)
	{
		this->update();
		if (on_frame)
		{
			auto self = shared_from_this();
			uint32_t ssrc = 0;
			auto ms = rtp_->get_media_stream_by_mid(mid);
			// 应答方流的 mid 来自对端 offer，辅流不一定叫 "extend_video"，按 content 标记兜底
			bool extend = (mid == "extend_video") || (ms && ms->content() == "slides");
			if (ms)
			{
				ssrc = ms->remote_rtp_ssrc();
			}
			on_frame(self, ssrc, fmt.pt, fmt.frequency, fmt.channels, &frame, extend);
		}
	}

	void call::on_rtpx_receive_require_keyframe(const std::string& mid)
	{
		auto self = shared_from_this();
		if (on_received_require_keyframe)
		{
			on_received_require_keyframe(self);
		}
	}

	void call::on_rtpx_dtmf(const std::string& mid, const std::string& sid, const std::string& tid, uint8_t event, uint32_t duration_ms)
	{
		char digit = dtmf_event_to_digit(event);
		if (digit == 0)
		{
			// Flash 等非按键事件不上报
			return;
		}
		invoke_dtmf(digit, (int)duration_ms);
	}

	char call::dtmf_event_to_digit(uint8_t event)
	{
		static const char digits[] = "0123456789*#ABCD";
		if (event < 16)
		{
			return digits[event];
		}
		return 0;
	}

	void call::invoke_dtmf(char digit, int duration_ms)
	{
		// 统一转成大写，只放行 0-9 * # A-D
		if (digit >= 'a' && digit <= 'd')
		{
			digit = digit - 'a' + 'A';
		}
		bool valid = (digit >= '0' && digit <= '9') || digit == '*' || digit == '#' || (digit >= 'A' && digit <= 'D');
		if (!valid)
		{
			return;
		}
		if (duration_ms < 0)
		{
			duration_ms = 0;
		}

		if (log_)
		{
			log_->info("Call {} received dtmf: digit={} duration={}ms", id(), digit, duration_ms)->flush();
		}

		if (on_dtmf)
		{
			auto self = shared_from_this();
			on_dtmf(self, digit, duration_ms);
		}
	}

	// ---------------- 发按键 ----------------

	// 按键时长与间隔（毫秒）
	static const int DTMF_MIN_DURATION_MS = 40;
	static const int DTMF_MAX_DURATION_MS = 1000;
	static const int DTMF_GAP_MS = 100;			// 键与键之间的静默
	static const int DTMF_PAUSE_MS = 500;		// ',' 的停顿
	static const int DTMF_RFC4733_INTERVAL_MS = 50;	// RFC 4733 更新包间隔

	// 按键字符转 RFC 4733 事件码，与 dtmf_event_to_digit 互逆
	static uint8_t dtmf_digit_to_event(char digit)
	{
		if (digit >= '0' && digit <= '9') return (uint8_t)(digit - '0');
		if (digit == '*') return 10;
		if (digit == '#') return 11;
		return (uint8_t)(12 + (digit - 'A'));	// A-D
	}

	bool call::send_dtmf(const std::string& digits, int duration_ms)
	{
		duration_ms = std::max(DTMF_MIN_DURATION_MS, std::min(DTMF_MAX_DURATION_MS, duration_ms));

		std::vector<dtmf_tone_t> tones;
		for (char c : digits)
		{
			if (c >= 'a' && c <= 'd')
			{
				c = c - 'a' + 'A';
			}
			bool valid = (c >= '0' && c <= '9') || c == '*' || c == '#' || (c >= 'A' && c <= 'D') || c == ',';
			if (valid)
			{
				tones.push_back({ c, duration_ms });
			}
		}
		if (tones.empty())
		{
			return false;
		}

		// 未接通（SIP 未完成 ACK、H.323 H.245 未建立）时对话/信令通道还没有，发不出去
		if (!triggered_connected_)
		{
			return false;
		}

		bool start = false;
		{
			std::lock_guard<std::mutex> lk(dtmf_mutex_);
			if (dtmf_closed_)
			{
				return false;	// 通话已结束
			}
			dtmf_queue_.insert(dtmf_queue_.end(), tones.begin(), tones.end());
			if (!dtmf_running_)
			{
				dtmf_running_ = true;
				start = true;
			}
		}
		if (log_)
		{
			// 按键可能是会议密码，日志只记个数不记内容
			log_->info("Call {} send dtmf queued: count={} duration={}ms", id(), tones.size(), duration_ms)->flush();
		}
		if (start)
		{
			dtmf_schedule(0, &call::dtmf_next);
		}
		return true;
	}

	void call::cancel_dtmf()
	{
		std::lock_guard<std::mutex> lk(dtmf_mutex_);
		dtmf_queue_.clear();
		dtmf_running_ = false;
		dtmf_closed_ = true;	// 不再复位：通话结束后 send_dtmf 一律拒绝，也就不会出现新旧两条链并存
		dtmf_gen_++;
		std::error_code ec;
		dtmf_timer_.cancel(ec);
	}

	void call::dtmf_schedule(int delay_ms, void (call::*fn)())
	{
		std::weak_ptr<call> wself = shared_from_this();
		// 定时器不是线程安全的，与 cancel_dtmf 同锁操作；已取消的链不再续
		std::lock_guard<std::mutex> lk(dtmf_mutex_);
		if (!dtmf_running_)
			return;
		uint32_t gen = dtmf_gen_;
		dtmf_timer_.expires_after(std::chrono::milliseconds(delay_ms));
		dtmf_timer_.async_wait([wself, fn, gen](const std::error_code& ec) {
			if (ec)
				return;
			auto self = wself.lock();
			if (!self)
				return;
			{
				// 取消后又有新的 send_dtmf 起了新链：旧链已经出队的回调到此为止
				std::lock_guard<std::mutex> lk(self->dtmf_mutex_);
				if (gen != self->dtmf_gen_ || !self->dtmf_running_)
					return;
			}
			((*self).*fn)();
		});
	}

	call::dtmf_method_t call::dtmf_method()
	{
		for (auto& m : rtp_->media_streams())
		{
			if (m->media_type() != media_type_audio)
				continue;
			for (auto& fmt : m->remote_formats())
			{
				if (fmt.codec == codec_type_telephone_event)
				{
					return dtmf_method_t::rfc4733;
				}
			}
		}
		return dtmf_method_t::none;
	}

	void call::dtmf_next()
	{
		dtmf_tone_t tone;
		{
			std::lock_guard<std::mutex> lk(dtmf_mutex_);
			if (!dtmf_running_)
			{
				return;	// 已被 cancel_dtmf
			}
			if (dtmf_queue_.empty())
			{
				dtmf_running_ = false;
				return;
			}
			tone = dtmf_queue_.front();
			dtmf_queue_.pop_front();
		}

		if (tone.digit == ',')
		{
			dtmf_schedule(DTMF_PAUSE_MS, &call::dtmf_next);
			return;
		}

		dtmf_method_t method = dtmf_method();
		if (method == dtmf_method_t::rfc4733)
		{
			if (dtmf_rfc4733_begin(tone.digit, tone.duration_ms))
			{
				dtmf_schedule(DTMF_RFC4733_INTERVAL_MS, &call::dtmf_rfc4733_tick);
				return;
			}
			method = dtmf_method_t::signaling;	// 媒体还没就绪等原因发不了 RTP 事件，退回信令
		}

		bool ok = false;
		if (method == dtmf_method_t::signaling)
		{
			ok = send_dtmf_signaling(tone.digit, tone.duration_ms);
		}
		if (!ok && log_)
		{
			log_->warn("Call {} send dtmf failed: method={}", id(), (int)method)->flush();
		}
		// 信令方式一条消息发完，按时长占位后再发下一个，保持与 RTP 方式一致的节奏
		dtmf_schedule(tone.duration_ms + DTMF_GAP_MS, &call::dtmf_next);
	}

	bool call::dtmf_rfc4733_begin(char digit, int duration_ms)
	{
		rtpx::media_stream_ptr ms;
		for (auto& m : rtp_->media_streams())
		{
			if (m->media_type() == media_type_audio)
			{
				ms = m;
				break;
			}
		}
		if (!ms)
		{
			return false;
		}

		uint8_t audio_pt = 0;
		if (!pick_send_pt(ms, audio_pt))
		{
			return false;
		}

		// 按键事件的时间戳沿用音频发送器的时钟，所以只能选与音频同采样率的 telephone-event
		int audio_frequency = 8000;
		for (auto& fmt : ms->local_formats())
		{
			if (fmt.pt == audio_pt)
			{
				audio_frequency = fmt.frequency;
				break;
			}
		}
		// 负载类型以对端 SDP 为准（对端可能用 96/100 等动态 PT）
		bool found = false;
		for (auto& fmt : ms->remote_formats())
		{
			if (fmt.codec == codec_type_telephone_event && fmt.frequency == audio_frequency)
			{
				te_.te_pt = fmt.pt;
				te_.ts_per_ms = audio_frequency / 1000;
				found = true;
				break;
			}
		}
		if (!found || te_.ts_per_ms <= 0)
		{
			return false;
		}

		uint32_t ts = 0;
		if (!ms->sender_rtp_timestamp(audio_pt, ts))
		{
			return false;
		}
		// 音频没在发时时间戳不前进，新按键起点要排在上一个按键之后（按回绕比较）
		if (te_has_last_ && (int32_t)(ts - te_last_end_ts_) <= 0)
		{
			ts = te_last_end_ts_ + (uint32_t)(DTMF_GAP_MS * te_.ts_per_ms);
		}

		te_.ms = ms;
		te_.audio_pt = audio_pt;
		te_.event = dtmf_digit_to_event(digit);
		te_.timestamp = ts;
		te_.duration_ms = duration_ms;
		te_.elapsed_ms = 0;
		return true;
	}

	void call::dtmf_rfc4733_tick()
	{
		te_.elapsed_ms += DTMF_RFC4733_INTERVAL_MS;
		bool end = te_.elapsed_ms >= te_.duration_ms;
		int elapsed = std::min(te_.elapsed_ms, te_.duration_ms);
		uint16_t duration = (uint16_t)std::min(0xFFFF, elapsed * te_.ts_per_ms);
		bool marker = (te_.elapsed_ms == DTMF_RFC4733_INTERVAL_MS);	// 一次按键的第一个包

		if (!end)
		{
			te_.ms->send_telephone_event(te_.audio_pt, te_.te_pt, te_.timestamp, te_.event, duration, false, marker);
			dtmf_schedule(DTMF_RFC4733_INTERVAL_MS, &call::dtmf_rfc4733_tick);
			return;
		}

		// RFC 4733 2.5.1.4：结束包重发 3 次防丢，时间戳与时长相同
		bool ok = true;
		for (int i = 0; i < 3; i++)
		{
			ok = te_.ms->send_telephone_event(te_.audio_pt, te_.te_pt, te_.timestamp, te_.event, duration, true, marker && i == 0) && ok;
		}
		if (!ok && log_)
		{
			log_->warn("Call {} send rfc4733 dtmf failed", id())->flush();
		}
		te_has_last_ = true;
		te_last_end_ts_ = te_.timestamp + duration;
		te_.ms.reset();
		dtmf_schedule(DTMF_GAP_MS, &call::dtmf_next);
	}

	bool call::is_inactive()const
	{
		time_t now=time(nullptr);
		time_t ts=now - updated_at_;
		return (ts >= 90);
	}

	void call::update()
	{
		updated_at_ = time(nullptr);
	}

	void call::invoke_connected()
	{
		bool expected = false;
		if (triggered_connected_.compare_exchange_strong(expected, true))
		{
			auto self = shared_from_this();
			if (on_connected)
			{
				on_connected(self,local_alias_,remote_alias_);
			}
		}
	}

	void call::invoke_hangup(reason_code_t reason)
	{
		if (on_hangup)
		{
			auto self = shared_from_this();
			on_hangup(self, reason);
		}
	}
}
