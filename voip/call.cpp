#include "call.h"
#include <mutex>

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
		}
		else
		{
			ms = rtp_->get_media_stream_by_mid("video");
			if (!ms)
			{
				// 应答方的流 mid 来自对端 offer，不一定叫 "video"，按媒体类型兜底
				for (auto& m : rtp_->media_streams())
				{
					if (m->media_type() == media_type_video)
					{
						ms = m;
						break;
					}
				}
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
			bool extend = (mid == "extend_video");
			uint32_t ssrc = 0;
			auto ms = rtp_->get_media_stream_by_mid(mid);
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
