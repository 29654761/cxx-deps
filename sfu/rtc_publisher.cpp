#include "rtc_publisher.h"
#include <string.h>

rtc_publisher::rtc_publisher()
{
	memset(&options_, 0, sizeof(options_));
}

rtc_publisher::~rtc_publisher()
{
	// 销毁本地轨道会等在途的关键帧回调返回（rtc-go rtc_destroy_local_track），
	// 保证析构后不会再以 this 为 context 回调
	close();
}

bool rtc_publisher::open(int codec)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (handle_)
		return false;


	handle_ = rtc_create_local_track(codec);
	if (!handle_)
		return false;

	return true;
}

void rtc_publisher::close()
{
	void* h = nullptr;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (handle_)
		{
			h = handle_;
			handle_ = nullptr;
		}
	}

	if (h)
	{
		rtc_destroy_local_track(h);
	}
}

bool rtc_publisher::publish(void* channel_handler, const rtc_publish_options_t& options)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (channel_handler_||!channel_handler)
		return false;

	channel_handler_ = channel_handler;
	options_ = options;
	register_keyframe_callback();
	int r=rtc_publish_local_track(channel_handler_, handle_, &options_);
	if(r!=RTC_OK)
	{
		channel_handler_ = nullptr;
		return false;
	}
	return true;
}

bool rtc_publisher::publish_mcu(void* channel_handler, const rtc_publish_options_t& options,int kind)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (channel_handler_ || !channel_handler)
		return false;

	channel_handler_ = channel_handler;
	options_ = options;
	// desc 为空时按 kind 给默认值（原先不分 kind 一律写 mcu_video，音频合成流的 desc 也成了 mcu_video）
	if (options_.desc[0] == '\0')
	{
		const char* def = (kind == 0) ? "mcu_audio" : "mcu_video";
		strncpy(options_.desc, def, sizeof(options_.desc) - 1);
	}
	// 必须在发布前注册：rtc-go 的发布会改写轨道 id 并开始读 RTCP
	register_keyframe_callback();
	if (kind == 0)
	{
		int r = rtc_publish_mcu_audio_track(channel_handler_, handle_, &options_);
		if (r != RTC_OK)
		{
			channel_handler_ = nullptr;
			return false;
		}
		return true;
	}
	else if (kind == 1)
	{
		int r = rtc_publish_mcu_video_track(channel_handler_, handle_, &options_);
		if (r != RTC_OK)
		{
			channel_handler_ = nullptr;
			return false;
		}
		return true;
	}
	else
	{
		return false;
	}
}

void rtc_publisher::set_keyframe_request_callback(keyframe_request_handler handler)
{
	std::lock_guard<std::mutex> lock(keyframe_mutex_);
	keyframe_handler_ = std::move(handler);
}

void rtc_publisher::register_keyframe_callback()
{
	if (!handle_)
		return;

	bool has_handler = false;
	{
		std::lock_guard<std::mutex> lock(keyframe_mutex_);
		has_handler = (bool)keyframe_handler_;
	}
	if (has_handler)
	{
		rtc_set_keyframe_request_callback(handle_, s_rtc_keyframe_request_callback, this);
	}
}

void rtc_publisher::s_rtc_keyframe_request_callback(void* context, const char* track_id)
{
	rtc_publisher* p = (rtc_publisher*)context;
	keyframe_request_handler handler;
	{
		std::lock_guard<std::mutex> lock(p->keyframe_mutex_);
		handler = p->keyframe_handler_;
	}
	if (handler)
	{
		handler(track_id ? track_id : "");
	}
}

void rtc_publisher::unpublish()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (channel_handler_)
	{
		rtc_unpublish_local_track(channel_handler_, handle_);
		channel_handler_ = nullptr;
	}
}

bool rtc_publisher::write_sample(uint8_t* data, int length, uint32_t samples)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if(!handle_)
		return false;

	int r = rtc_write_sample(handle_, data, length, samples);
	if (r != RTC_OK)
		return false;
	return true;
}
