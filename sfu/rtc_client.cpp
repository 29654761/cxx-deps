#include "rtc_client.h"
#include <sys2/util.h>

rtc_client::rtc_client(asio::io_service& ios)
	:ios_(ios), timer_(ios)
{
	connection_state_ = 2;
}

rtc_client::~rtc_client()
{
}

bool rtc_client::open()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (active_)
		return false;
	active_ = true;
	if (handle_)
		return false;

	handle_=rtc_create();
	if (!handle_)
		return false;

	rtc_set_connection_callback(handle_, s_rtc_connection_callback, this);
	rtc_set_user_event_callback(handle_, s_rtc_user_event_callback, this);
	rtc_set_track_event_callback(handle_, s_rtc_track_event_callback, this);
	rtc_set_track_sample_callback(handle_, s_rtc_track_sample_callback, this);
	rtc_set_layer_switched_callback(handle_, s_rtc_layer_switched_callback, this);
	rtc_set_connection_quality_callback(handle_, s_rtc_connection_quality_callback, this);
	rtc_set_active_speakers_callback(handle_, s_rtc_active_speakers_callback, this);
	rtc_set_custom_msg_callback(handle_, s_rtc_custom_msg_callback, this);
	rtc_set_keyframe_request_callback(handle_, s_rtc_keyframe_request_callback, this);

	timer_.expires_after(std::chrono::seconds(1));
	timer_.async_wait(std::bind(&rtc_client::handle_timer, shared_from_this(), std::placeholders::_1));
	return true;
}

void rtc_client::close()
{
	active_ = false;
	asio::error_code ec;
	timer_.cancel(ec);

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
		std::this_thread::sleep_for(std::chrono::seconds(2));// avoid crashing when rtc still callback
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
	int r= rtc_join_channel_sync(handle_, (char*)token,30000);
	//int r = rtc_join_channel(handle_, (char*)token);
	if (log_)
	{
		log_->debug("Join channel result={}", r)->flush();
	}
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

	if (log_)
	{
		log_->debug("Subscribe audio uid={}, trackid={}, result={}",uid,track_id, r)->flush();
	}

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
	if (log_)
	{
		log_->debug("Subscribe video uid={}, trackid={}, result={}", uid, track_id, r)->flush();
	}
	if (r != RTC_OK)
		return false;

	return true;
}

bool rtc_client::subscribe_by_desc(const char* uid, const char* tdesc)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!handle_)
		return false;

	rtc_user_info_t user = {};
	int r = rtc_get_user_info(handle_, uid, &user);
	if (r != RTC_OK)
	{
		if (log_)
		{
			log_->error("Subscribe by desc failed uid={}, tdesc={}, result=no user info", uid, tdesc)->flush();
		}
		return false;
	}
	for (int i = 0; i < user.stream_track_count; ++i)
	{
		if (strcmp(user.stream_tracks[i].desc, tdesc) == 0)
		{
			if (user.stream_tracks[i].kind == 0)
			{
				int r = rtc_subscribe_audio(handle_, uid, user.stream_tracks[i].track_id);
				if (log_)
				{
					if (r != RTC_OK)
					{
						log_->error("Subscribe audio failed chan={},uid={},name={}, tdesc={}, result={}",std::string(user.channel,128).c_str(), uid, std::string(user.name, 128).c_str(), tdesc, r)->flush();
					}
					else
					{
						log_->info("Subscribe audio ok chan={},uid={},name={},, tdesc={}, result={}", std::string(user.channel, 128).c_str(), uid, std::string(user.name, 128).c_str(), tdesc, r)->flush();
					}
				}
				rtc_free_user_info(&user);
				return r == RTC_OK;
			}
			else if (user.stream_tracks[i].kind == 1)
			{
				int r = rtc_subscribe_video(handle_, uid, user.stream_tracks[i].track_id);
				if (log_)
				{
					if (r != RTC_OK)
					{
						log_->error("Subscribe video failed chan={},uid={},name={}, tdesc={},tid={}, result={}", std::string(user.channel, 128).c_str(), uid , std::string(user.name, 128).c_str(), tdesc, std::string(user.stream_tracks[i].track_id,64), r)->flush();
					}
					else
					{
						log_->info("Subscribe video ok chan={},uid={},name={},, tdesc={},tid={}, result={}", std::string(user.channel, 128).c_str(), uid, std::string(user.name, 128).c_str(), tdesc, std::string(user.stream_tracks[i].track_id,64), r)->flush();
					}
				}
				rtc_free_user_info(&user);
				return r == RTC_OK;
			}
		}
	}

	rtc_free_user_info(&user);

	if (log_)
	{
		log_->error("Subscribe by desc failed chan={},uid={},name={}, tdesc={}, result={}", std::string(user.channel, 128).c_str(), uid, std::string(user.name, 128).c_str(), tdesc, r)->flush();
	}


	return false;
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

bool rtc_client::unsubscribe_by_desc(const char* uid, const char* tdesc)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!handle_)
		return false;

	rtc_user_info_t user = {};
	int r = rtc_get_user_info(handle_, uid, &user);
	if (r != RTC_OK)
		return false;

	for (int i = 0; i < user.stream_track_count; ++i)
	{
		if (strcmp(user.stream_tracks[i].desc, tdesc) == 0)
		{
			int r=rtc_unsubscribe(handle_, uid, user.stream_tracks[i].track_id);
			rtc_free_user_info(&user);
			return r == RTC_OK;
		}
	}

	rtc_free_user_info(&user);
	return false;
}

bool rtc_client::subscribe_all_items()
{
	std::vector<subscribe_item> items;
	all_subscribes(items);

	for (auto itr = items.begin(); itr != items.end(); itr++)
	{
		if (!itr->result)
		{
			bool result=subscribe_by_desc(itr->uid.c_str(), itr->tdesc.c_str());
			set_subscribe_result(itr->uid.c_str(), itr->tdesc.c_str(), result);
		}
	}

	return true;
}

void rtc_client::set_subscribe_result(const char* uid, const char* tdesc, bool result)
{
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto itr = subscribe_items_.begin(); itr != subscribe_items_.end(); itr++)
	{
		if (itr->uid == uid)
		{
			if (tdesc)
			{
				if (itr->tdesc == tdesc)
				{
					itr->result = result;
				}
			}
			else
			{
				itr->result = result;
			}
		}
	}
	
}

void rtc_client::set_all_subscribe_result(bool result)
{
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto itr = subscribe_items_.begin(); itr != subscribe_items_.end(); itr++)
	{
		itr->result = result;
	}
}

bool rtc_client::add_subscribe(const char* uid, const char* tdesc)
{
	std::lock_guard<std::mutex> lock(mutex_);
	auto itr=std::find_if(subscribe_items_.begin(), subscribe_items_.end(),
		[uid,tdesc](const subscribe_item& a) {
		return a.uid == uid && a.tdesc == tdesc;
	});
	if (itr == subscribe_items_.end())
	{
		subscribe_item item = {};
		item.uid = uid;
		item.tdesc = tdesc;
		item.result = false;
		subscribe_items_.push_back(item);
	}
	else
	{
		itr->result = false;
	}
	if (log_)
	{
		log_->debug("Add subscribe uid={}, tdesc={}", uid,tdesc)->flush();
	}
	return true;
}

bool rtc_client::remove_subscribe(const char* uid, const char* tdesc)
{
	subscribe_item item = {};
	{
		std::lock_guard<std::mutex> lock(mutex_);

		auto itr = std::find_if(subscribe_items_.begin(), subscribe_items_.end(),
			[uid, tdesc](const subscribe_item& a) {
			return a.uid == uid && a.tdesc == tdesc;
		});

		if (itr == subscribe_items_.end())
			return false;
		item = *itr;
		subscribe_items_.erase(itr);
	}

	unsubscribe_by_desc(item.uid.c_str(), item.tdesc.c_str());

	if (log_)
	{
		log_->debug("Remove subscribe uid={}, tdesc={}", uid, tdesc)->flush();
	}
	return true;
}

void rtc_client::clear_subscribes()
{
	std::vector<subscribe_item> items;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		items = subscribe_items_;
		subscribe_items_.clear();
	}

	for (auto itr = items.begin(); itr != items.end(); itr++)
	{
		if (itr->result)
		{
			unsubscribe_by_desc(itr->uid.c_str(), itr->tdesc.c_str());
		}
	}
}

void rtc_client::all_subscribes(std::vector<subscribe_item>& subs)const
{
	std::lock_guard<std::mutex> lock(mutex_);
	subs = subscribe_items_;
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
	int r = rtc_get_user_info(handle_, uid.c_str(),&info);
	if (r != RTC_OK)
		return false;

	user.from_struct(&info);
	rtc_free_user_info(&info);

	return true;
}

bool rtc_client::request_key_frame(const std::string& uid, const std::string& track_id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!handle_)
		return false;

	int r = rtc_request_key_frame(handle_,uid.c_str(),track_id.c_str());
	if (r != RTC_OK)
		return false;

	return true;
}

bool rtc_client::switch_layer(const std::string& pub_uid, const std::string& track_id, const std::string& target_track_id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!handle_)
		return false;

	int r = rtc_switch_layer(handle_, pub_uid.c_str(),track_id.c_str(),target_track_id.c_str());
	if (r != RTC_OK)
		return false;

	return true;
}

bool rtc_client::get_connection_quality(rtc_connection_quality_t* out)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!handle_)
		return false;

	int r = rtc_get_connection_quality(handle_,out);
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
	p->connection_state_ = state;

	p->ios_.post([p,state]() {
		if (state == 1)
		{
			p->set_all_subscribe_result(false);
		}
		p->connection_event.invoke(state);
	});

	p->signal_.notify();
}

void rtc_client::s_rtc_user_event_callback(
	void* context,
	const char* uid,
	int event_type  // 0=join, 1=leave
)
{
	rtc_client* p = (rtc_client*)context;

	if (event_type == 0)
	{
		p->ios_.post([p,uid]() {
			p->set_subscribe_result(uid, nullptr, false);
		});
	}

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
	if (event_type == 0)
	{
		p->ios_.post([p, uid, track_info]() {
			p->set_subscribe_result(uid, track_info->desc, false);
		});
		
	}
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
	p->track_sample_event.invoke(user_info, track_info, data, len, timestamp, duration);
}

void rtc_client::s_rtc_layer_switched_callback(void* context, const rtc_layer_switched_t* data)
{
	rtc_client* p = (rtc_client*)context;
	p->layer_switched_event.invoke(data);
}

void rtc_client::s_rtc_keyframe_request_callback(void* context, const char* track_id)
{
	rtc_client* p = (rtc_client*)context;
	p->keyframe_request_event.invoke(track_id);
}

void rtc_client::s_rtc_custom_msg_callback(void* context, const rtc_custom_msg_t* msg)
{
	rtc_client* p = (rtc_client*)context;
	p->custom_msg_event.invoke(msg);
}

void rtc_client::s_rtc_active_speakers_callback(void* context, int64_t ts, const rtc_active_speaker_t* speakers, int speakers_count)
{
	rtc_client* p = (rtc_client*)context;
	p->active_speakers_event.invoke(ts, speakers, speakers_count);
}

void rtc_client::s_rtc_connection_quality_callback(void* context, const rtc_connection_quality_t* q)
{
	rtc_client* p = (rtc_client*)context;
	p->connection_quality_event.invoke(q);
}

void rtc_client::handle_timer(const asio::error_code& ec)
{
	if (!active_ || ec)
		return;

	subscribe_all_items();

	timer_.expires_after(std::chrono::seconds(1));
	timer_.async_wait(std::bind(&rtc_client::handle_timer,shared_from_this(),std::placeholders::_1));
}