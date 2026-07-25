#pragma once

#ifdef _MSC_VER
#define _CRT_USE_C_COMPLEX_H
#endif
#include <librtc.h>
#include <mutex>
#include <atomic>
#include <sys2/mutex_callback.hpp>
#include <sys2/signal.h>
#include <sfu/rtc_models.h>
#include <asio/asio.hpp>
#include <spdlog/spdlogger.hpp>

class rtc_client :public std::enable_shared_from_this<rtc_client>
{
public:
	enum class track_type_t
	{
		unknown=0,
		tid_audio=1,
		tid_video=2,
		tdesc=3,
	};

	struct subscribe_item
	{
		std::string uid;
		std::string track;
		track_type_t track_type = track_type_t::tdesc;
		bool result = false;
	};

	rtc_client(asio::io_service& ios);
	~rtc_client();

	void* handle() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return handle_;
	}

	int connection_state()const { return connection_state_; }

	void set_logger(spdlogger_ptr log) { log_ = log; }
	bool open();
	void close();
	bool auto_subscribe(bool audio, bool video);
	bool join_channel(const char* token);
	bool wait_join(int msec) {
		return signal_.wait(msec);
	}
	void wait_join()
	{
		signal_.wait();
	}
	bool leave_channel();



	bool add_subscribe(const char* uid, const char* track, track_type_t ttype);
	bool remove_subscribe(const char* uid, const char* tdesc, track_type_t ttype);
	void clear_subscribes();
	void all_subscribes(std::vector<subscribe_item>& subs)const;

	bool all_users(std::vector<rtc_user>& users);
	bool get_user(const std::string& uid, rtc_user& user);

	bool request_key_frame(const std::string& uid, const std::string& track_id);
	bool switch_layer(const std::string& pub_uid, const std::string& track_id, const std::string& target_track_id);
	bool get_connection_quality(rtc_connection_quality_t* out);

	sys::mutex_callback<rtc_connection_callback> connection_event;
	sys::mutex_callback<rtc_user_event_callback> user_event;
	sys::mutex_callback<rtc_track_event_callback> track_event;
	sys::mutex_callback<rtc_track_sample_callback> track_sample_event;
	sys::mutex_callback<rtc_keyframe_request_callback> keyframe_request_event;
	sys::mutex_callback<rtc_custom_msg_callback> custom_msg_event;
	sys::mutex_callback<rtc_active_speakers_callback> active_speakers_event;
	sys::mutex_callback<rtc_connection_quality_callback> connection_quality_event;
	sys::mutex_callback<rtc_layer_switched_callback> layer_switched_event;

private:
	bool subscribe_by_desc(const char* uid, const char* tdesc);
	bool unsubscribe_by_desc(const char* uid, const char* tdesc);
	bool subscribe_all_items();
	//tdesc is null will set all tracks
	void set_subscribe_result(const char* uid, const char* track, track_type_t track_type, bool result);
	void set_all_subscribe_result(bool result);

	bool subscribe_track(const subscribe_item& item);
	bool unsubscribe_track(const subscribe_item& item);
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

	static void s_rtc_layer_switched_callback(void* context, const rtc_layer_switched_t* data);

	static void s_rtc_keyframe_request_callback(void* context, const char* track_id);

	static void s_rtc_custom_msg_callback(void* context, const rtc_custom_msg_t* msg);

	static void s_rtc_active_speakers_callback(void* context, int64_t ts, const rtc_active_speaker_t* speakers, int speakers_count);

	static void s_rtc_connection_quality_callback(void* context, const rtc_connection_quality_t* q);

	void handle_timer(const asio::error_code& ec);
private:
	asio::io_service& ios_;
	spdlogger_ptr log_;
	bool active_ = false;
	asio::steady_timer timer_;
	mutable std::mutex mutex_;
	void* handle_ = nullptr;
	std::atomic<int> connection_state_;
	sys::signal signal_;

	std::vector<subscribe_item> subscribe_items_;
};

typedef std::shared_ptr<rtc_client> rtc_client_ptr;

