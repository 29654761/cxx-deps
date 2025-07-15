#pragma once

#include "rtsp_request.h"
#include "rtsp_response.h"

#include <spdlog/spdlogger.hpp>
#include <sys2/socket.h>
#include <sys2/blocking_queue.hpp>
#include <sys2/mutex_callback.hpp>
#include <memory>
#include <atomic>


class rtsp_server;


class rtsp_session:public std::enable_shared_from_this<rtsp_session>
{
public:
	rtsp_session(rtsp_server* server,spdlogger_ptr log,std::shared_ptr<sys::socket> socket);
	virtual ~rtsp_session();

	const std::string& id()const;

	virtual bool start();
	virtual void close();
	bool is_timeout()const;

	bool send_response(rtsp_response& response);

	virtual bool send_video_frame(const uint8_t* frame, uint32_t size, uint32_t duration) = 0;
	virtual bool send_audio_frame(const uint8_t* frame, uint32_t size, uint32_t duration) = 0;

	sys::uri url();
	void set_url(const std::string& url);

protected:
	virtual void on_message_options(const rtsp_request& request);
	virtual void on_message_describe(const rtsp_request& request);
	virtual void on_message_setup(const rtsp_request& request);
	virtual void on_message_announce(const rtsp_request& request);
	virtual void on_message_play(const rtsp_request& request);
	virtual void on_message_pause(const rtsp_request& request);
	virtual void on_message_record(const rtsp_request& request);
	virtual void on_message_teardown(const rtsp_request& request);

protected:
	void print_message(const rtsp_request& request,const rtsp_response& response);
private:

	void run();
	void run_sender();
	void on_message(rtsp_request& request);
	bool process_media();

	bool read_line(std::string& s);
	bool read_size(std::string& buffer, int size);
	bool read_size(char* buffer, int size);

protected:
	mutable std::recursive_mutex mutex_;
	spdlogger_ptr log_;
	rtsp_server* server_ = nullptr;
	bool active_ = false;
	std::string id_;
	std::shared_ptr<sys::socket> socket_;
	std::atomic<time_t> updated_time_;
	std::thread* worker_ = nullptr;
	std::thread* worker_sender_ = nullptr;
	int cseq = 0;
	std::string session_id_;


	int rtp_channel_ = 0;
	int rtcp_channel_ = 1;

	sys::uri url_;

	sys::blocking_queue<std::string> send_buf_;
};

typedef std::shared_ptr<rtsp_session> rtsp_session_ptr;

