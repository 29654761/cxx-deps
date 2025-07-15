#pragma once

#include "rtsp_request.h"
#include "rtsp_response.h"

#include <spdlog/spdlogger.hpp>
#include <asio.hpp>
#include <hypertext/uri.h>
#include <memory>
#include <atomic>
#include <functional>
#include <sys2/ring_buffer.hpp>
#include <sys2/asio_sending_queue.hpp>

class rtsp_connection;

typedef std::shared_ptr<rtsp_connection> rtsp_connection_ptr;

class rtsp_connection:public std::enable_shared_from_this<rtsp_connection>
{
public:
	typedef std::shared_ptr<asio::ip::tcp::socket> tcp_socket_ptr;
	typedef std::function<void(const std::string& id)> on_destroy_t;

	rtsp_connection(tcp_socket_ptr socket);
	virtual ~rtsp_connection();

	const std::string& id()const;

	virtual bool start();
	virtual void close();
	bool is_timeout()const;

	bool send_response(rtsp_response& response);

	virtual bool send_video_frame(const uint8_t* frame, uint32_t size, uint32_t duration) = 0;
	virtual bool send_audio_frame(const uint8_t* frame, uint32_t size, uint32_t duration) = 0;

	hypertext::uri url();
	void set_url(const std::string& url);

	void set_logger(spdlogger_ptr log) { log_ = log; }
	void set_on_destroy(on_destroy_t handler) { on_destroy = handler; }
protected:
	virtual void on_message(rtsp_request& request);
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
	void handle_read(rtsp_connection_ptr con, const std::error_code& ec, std::size_t bytes);

protected:
	mutable std::mutex mutex_;
	spdlogger_ptr log_;
	bool active_ = false;
	std::string id_;
	tcp_socket_ptr socket_;
	std::atomic<time_t> updated_time_;
	int cseq = 0;
	std::string session_id_;


	int rtp_channel_ = 0;
	int rtcp_channel_ = 1;

	hypertext::uri url_;

	std::array<char, 10240> recv_buffer_;
	sys::ring_buffer<char> recv_ring_buffer_;

	asio::sending_queue_ptr sending_queue_;
	on_destroy_t on_destroy;

};


