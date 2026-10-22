#include "call.h"
//#include <litertp/h264/h264.h>

namespace voip
{
	call::call(const std::string& local_alias, const std::string& remote_alias, direction_t direction,
		const std::string& nat_address, int port, litertp::port_range_ptr rtp_ports)
		:updated_at_(0)
		, rtp_(false,true)
		, triggered_connected_(false)
	{
		local_alias_ = local_alias;
		remote_alias_ = remote_alias;
		direction_ = direction;
		nat_address_ = nat_address;
		local_port_ = port;
		rtp_ports_ = rtp_ports;
		rtp_.on_frame.add(s_litertp_on_frame, this);
		rtp_.on_received_require_keyframe.add(s_litertp_on_received_require_keyframe, this);
		rtp_.on_dtmf.add(s_litertp_on_dtmf, this);
		update();
	}

	call::~call()
	{
	}


	bool call::send_video_frame(const uint8_t* frame, uint32_t size, uint32_t duration, bool is_extend)
	{
		litertp::media_stream_ptr ms = nullptr;
		if (is_extend)
		{
			ms = rtp_.get_media_stream("extend_video");
		}
		else
		{
			ms = rtp_.get_media_stream("video");
		}
		if (!ms)
		{
			return false;
		}
		return ms->send_frame(frame, size, duration);
	}

	bool call::send_audio_frame(const uint8_t* frame, uint32_t size, uint32_t duration)
	{
		auto ms = rtp_.get_media_stream(media_type_audio);
		if (!ms)
		{
			return false;
		}
		return ms->send_frame(frame, size, duration);
	}


	void call::s_litertp_on_frame(void* ctx, const char* mid, uint32_t ssrc, uint16_t pt, int frequency, int channels, const av_frame_t* frame)
	{
		call* p = (call*)ctx;
		p->update();
		if (p->on_frame)
		{
			auto self = p->shared_from_this();
			bool extend = false;
			if (strncmp(mid, "extend_video",12) == 0)
			{
				extend = true;
			}
			p->on_frame(self,ssrc,pt,frequency,channels, frame, extend);
		}
		
		/*
		if (frame->mt == media_type_audio) {
			//printf("received audio\n");
		}
		else if (frame->mt == media_type_video) {
			//printf("received video\n");

			
			int nal_start=h264::find_nal_start(frame->data, frame->data_size);
			if (nal_start >= 0)
			{
				if (h264::is_key_frame(frame->data[nal_start]))
				{
					printf("received keyframe\n");
				}
			}
			
			int nal_start = h265::find_nal_start(frame->data,frame->data_size);
			if (nal_start >= 0)
			{
				if (h265::is_key_frame(frame->data+nal_start, frame->data_size-nal_start))
				{
					printf("received keyframe\n");
				}
			}
			
		}
		*/
	}

	void call::s_litertp_on_received_require_keyframe(void* ctx, uint32_t ssrc, int mode)
	{
		call* p = (call*)ctx;
		auto self = p->shared_from_this();
		if (p->on_received_require_keyframe)
		{
			p->on_received_require_keyframe(self);
		}
	}

	void call::s_litertp_on_dtmf(void* ctx, const char* mid, uint32_t ssrc, uint8_t event, uint32_t duration_ms)
	{
		call* p = (call*)ctx;
		char digit = dtmf_event_to_digit(event);
		if (digit == 0)
		{
			// Flash 等非按键事件不上报
			return;
		}
		p->invoke_dtmf(digit, (int)duration_ms);
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
