#include "rtc_client.h"

rtc_client::rtc_client()
{
}

rtc_client::~rtc_client()
{
}

bool rtc_client::open()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (handle_)
		return false;

	handle_=rtc_create();
	if (!handle_)
		return false;

	rtc_set_connection_callback(handle_, s_rtc_connection_callback, this);
	rtc_set_user_event_callback(handle_, s_rtc_user_event_callback, this);
	rtc_set_track_event_callback(handle_, s_rtc_track_event_callback, this);
	rtc_set_track_sample_callback(handle_, s_rtc_track_sample_callback, this);
	return true;
}

void rtc_client::close()
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
		rtc_destroy(h);
	}
}

bool rtc_client::auto_subscribe(bool audio, bool video)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!handle_)
		return false;

	rtc_set_auto_subscribe(handle_, audio ? 1 : 0, video ? 1 : 0);
	return true;
}

bool rtc_client::join_channel(const char* token)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!handle_)
		return false;

	int r=rtc_join_channel(handle_, (char*)token);
	if (r != RTC_OK)
		return false;

	return true;
}

bool rtc_client::leave_channel()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!handle_)
		return false;

	int r = rtc_leave_channel(handle_);
	if (r != RTC_OK)
		return false;

	return true;
}

bool rtc_client::subscribe_audio(const char* uid, const char* track_id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!handle_)
		return false;

	int r = rtc_subscribe_audio(handle_,(char*)uid,(char*)track_id);
	if (r != RTC_OK)
		return false;

	return true;
}

bool rtc_client::subscribe_video(const char* uid, const char* track_id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!handle_)
		return false;

	int r = rtc_subscribe_video(handle_, (char*)uid, (char*)track_id);
	if (r != RTC_OK)
		return false;

	return true;
}

bool rtc_client::unsubscribe(const char* uid, const char* track_id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!handle_)
		return false;

	int r = rtc_unsubscribe(handle_, (char*)uid, (char*)track_id);
	if (r != RTC_OK)
		return false;

	return true;
}



void rtc_client::s_rtc_connection_callback(
	void* context,
	int state  // 0=connecting, 1=connected, 2=disconnected, 3=reconnecting
)
{
	rtc_client* p = (rtc_client*)context;
	p->connection_event.invoke(state);
}

void rtc_client::s_rtc_user_event_callback(
	void* context,
	const char* uid,
	int event_type  // 0=join, 1=leave
)
{
	rtc_client* p = (rtc_client*)context;
	p->user_event.invoke(uid, event_type);
}

void rtc_client::s_rtc_track_event_callback(
	void* context,
	const char* uid,
	rtc_track_info_t* track_info,
	int event_type  // 0=add, 1=update, 2=remove
)
{
	rtc_client* p = (rtc_client*)context;
	p->track_event.invoke(uid, track_info, event_type);
}

void rtc_client::s_rtc_track_sample_callback(
	void* context,
	rtc_user_info_t* user_info,
	rtc_track_info_t* track_info,
	uint8_t* data,
	int len,
	int64_t timestamp,
	int64_t duration
)
{
	rtc_client* p = (rtc_client*)context;
	p->track_sample.invoke(user_info, track_info, data, len, timestamp, duration);
}