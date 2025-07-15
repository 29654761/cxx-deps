#include "call.h"
//#include <litertp/h264/h264.h>

namespace voip
{
	call::call(spdlogger_ptr log, const std::string& alias, direction_t direction,
		const std::string& nat_address, int port, litertp::port_range_ptr rtp_ports)
		:updated_at_(0)
		, triggered_connected_(false)
		, signal_close_(false)
	{
		alias_ = alias;
		direction_ = direction;
		nat_address_ = nat_address;
		local_port_ = port;
		rtp_ports_ = rtp_ports;
		log_ = log;
		rtp_.on_frame.add(s_litertp_on_frame, this);
		rtp_.on_received_require_keyframe.add(s_litertp_on_received_require_keyframe, this);
		update();
	}

	call::~call()
	{
		wait_for_stoped();
	}

	void call::wait_for_stoped()
	{
		signal_close_.wait();
	}

	bool call::send_video_frame(const uint8_t* frame, uint32_t size, uint32_t duration)
	{
		auto ms = rtp_.get_media_stream(media_type_video);
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


	void call::s_litertp_on_frame(void* ctx, uint32_t ssrc, uint16_t pt, int frequency, int channels, const av_frame_t* frame)
	{
		call* p = (call*)ctx;
		p->update();
		p->on_frame.invoke(ssrc, pt, frequency, channels, frame);
		
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
		auto sft = p->shared_from_this();
		p->on_received_require_keyframe.invoke(sft);
	}

	bool call::is_inactive()const
	{
		if (closed_)
			return true;
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
			auto sft = shared_from_this();
			on_connected.invoke(sft);
		}
	}

}
