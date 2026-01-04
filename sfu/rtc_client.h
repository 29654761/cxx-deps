#pragma once

#ifdef _MSC_VER
#define _CRT_USE_C_COMPLEX_H
#endif
#include <librtc.h>
#include <mutex>
#include <sys2/mutex_callback.hpp>



class rtc_client :public std::enable_shared_from_this<rtc_client>
{
public:
	rtc_client();
	~rtc_client();

	void* handle() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return handle_;
	}

	bool open();
	void close();
	bool auto_subscribe(bool audio, bool video);
	bool join_channel(const char* token);
	bool leave_channel();
	bool subscribe_audio(const char* uid, const char* track_id);
	bool subscribe_video(const char* uid, const char* track_id);
	bool unsubscribe(const char* uid, const char* track_id);

	sys::mutex_callback<rtc_connection_callback> connection_event;
	sys::mutex_callback<rtc_user_event_callback> user_event;
	sys::mutex_callback<rtc_track_event_callback> track_event;
	sys::mutex_callback<rtc_track_sample_callback> track_sample;
private:
	static void s_rtc_connection_callback(
		void* context,
		int state  // 0=connecting, 1=connected, 2=disconnected, 3=reconnecting
		);

	static void s_rtc_user_event_callback(
		void* context,
		const char* uid,
		int event_type  // 0=join, 1=leave
		);

	static void s_rtc_track_event_callback(
		void* context,
		const char* uid,
		rtc_track_info_t* track_info,
		int event_type  // 0=add, 1=update, 2=remove
		);

	static void s_rtc_track_sample_callback(
		void* context,
		rtc_user_info_t* user_info,
		rtc_track_info_t* track_info,
		uint8_t* data,
		int len,
		int64_t timestamp,
		int64_t duration
		);
private:
	mutable std::mutex mutex_;
	void* handle_ = nullptr;
};

typedef std::shared_ptr<rtc_client> rtc_client_ptr;

