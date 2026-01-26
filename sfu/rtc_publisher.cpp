#include "rtc_publisher.h"
#include <string.h>

rtc_publisher::rtc_publisher()
{
	memset(&options_, 0, sizeof(options_));
}

rtc_publisher::~rtc_publisher()
{
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
	int r=rtc_publish_local_track(channel_handler_, handle_, &options_);
	if(r!=RTC_OK)
	{
		channel_handler_ = nullptr;
		return false;
	}
	return true;
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
