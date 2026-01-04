#pragma once

#ifdef _MSC_VER
#define _CRT_USE_C_COMPLEX_H
#endif
#include <librtc.h>
#include <mutex>



class rtc_publisher :public std::enable_shared_from_this<rtc_publisher>
{
public:
	rtc_publisher();
	~rtc_publisher();

	const rtc_publish_options_t& options() const {
		return options_;
	}

	void* handle() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return handle_;
	}

	bool open(int codec);
	void close();
	
	bool publish(void* channel_handler, const rtc_publish_options_t& options);
	void unpublish();
	bool write_sample(uint8_t* data, int length, uint32_t samples);
private:
	rtc_publish_options_t options_;
	mutable std::mutex mutex_;
	void* handle_ = nullptr;
	void* channel_handler_ = nullptr;
};

typedef std::shared_ptr<rtc_publisher> rtc_publisher_ptr;

