#include "rtc_client.h"

rtc_client::rtc_client()
{
	connection_state_ = 2;
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
	connection_state_ = 0;
	signal_.reset();
	int r=rtc_join_channel(handle_, (char*)token);
	if (r != RTC_OK)
	{
		connection_state_ = 2;
		signal_.reset();
		return false;
	}
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
	connection_state_ = 2;
	return true;
}

bool rtc_client::subscribe_audio(const char* uid, const char* track_id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!handle_)
		return false;

	int r = rtc_subscribe_audio(handle_,uid,track_id);
	if (r != RTC_OK)
		return false;

	return true;
}

bool rtc_client::subscribe_video(const char* uid, const char* track_id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!handle_)
		return false;

	int r = rtc_subscribe_video(handle_,uid, track_id);
	if (r != RTC_OK)
		return false;

	return true;
}

bool rtc_client::unsubscribe(const char* uid, const char* track_id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!handle_)
		return false;

	int r = rtc_unsubscribe(handle_,uid,track_id);
	if (r != RTC_OK)
		return false;

	return true;
}

bool rtc_client::all_users(std::vector<rtc_user>& users)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!handle_)
		return false;

	users.clear();
	rtc_user_info_t* infos = nullptr;
	int count = 0;
	int r=rtc_get_users_info(handle_, &infos, &count);
	if (r != RTC_OK || !infos || count == 0)
	{
		if (infos)
		{
			rtc_free_users_info(infos,count);
		}
		return false;
	}

	users.reserve(count);
	for (int i = 0; i < count; i++)
	{
		rtc_user user;
		user.from_struct(infos + i);
		users.push_back(user);
	}

	if (infos)
	{
		rtc_free_users_info(infos, count);
	}

	return true;
}

bool rtc_client::get_user(const std::string& uid, rtc_user& user)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!handle_)
		return false;

	rtc_user_info_t info = {};
	int r = rtc_get_user_info(handle_, uid.c_str(), &info);
	if (r != RTC_OK)
		return false;

	user.from_struct(&info);
	rtc_free_user_info(&info);

	return true;
}


void rtc_client::s_rtc_connection_callback(
	void* context,
	int state  // 0=connecting, 1=connected, 2=disconnected, 3=reconnecting
)
{
	rtc_client* p = (rtc_client*)context;
	p->connection_state_ = state;
	p->connection_event.invoke(state);

	p->signal_.notify();
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