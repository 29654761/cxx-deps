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

	struct subscribe_item
	{
		std::string uid;
		std::string tdesc;
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



	bool add_subscribe(const char* uid, const char* tdesc);
	bool remove_subscribe(const char* uid, const char* tdesc);
	void clear_subscribes();
	void all_subscribes(std::vector<subscribe_item>& subs)const;

	bool all_users(std::vector<rtc_user>& users);
	bool get_user(const std::string& uid, rtc_user& user);


	sys::mutex_callback<rtc_connection_callback> connection_event;
	sys::mutex_callback<rtc_user_event_callback> user_event;
	sys::mutex_callback<rtc_track_event_callback> track_event;
	sys::mutex_callback<rtc_track_sample_callback> track_sample;

private:
	bool subscribe_audio(const char* uid, const char* track_id);
	bool subscribe_video(const char* uid, const char* track_id);
	bool subscribe_by_desc(const char* uid, const char* tdesc);
	bool unsubscribe(const char* uid, const char* track_id);
	bool unsubscribe_by_desc(const char* uid, const char* tdesc);
	bool subscribe_all_items();
	//tdesc is null will set all tracks
	void set_subscribe_result(const char* uid, const char* tdesc, bool result);
	void set_all_subscribe_result(bool result);
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

