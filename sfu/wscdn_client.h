#pragma once
#include <memory>
#include <string>
#include <mutex>
#include <asio/asio.hpp>
#include <spdlog/spdlogger.hpp>
#include <sys2/mutex_callback.hpp>
#include <rtpx/rtpx_core.h>
#include "sfu.h"

namespace sfu
{
	class wscdn_client;
	typedef std::shared_ptr<wscdn_client> wscdn_client_ptr;

	class wscdn_client:public std::enable_shared_from_this<wscdn_client>
	{
	public:
		wscdn_client(asio::io_service& ios, spdlogger_ptr log);
		~wscdn_client();

		std::string host()const;
		void set_host(const std::string& host);

		std::string stream()const;
		void set_stream(const std::string& stream);

		std::string key()const;
		void set_key(const std::string& key);

		virtual bool open(const std::string& host, const std::string& stream, const std::string& key);
		virtual void close();

		virtual bool send_frame(const std::string& mid, uint8_t pt, const uint8_t* frame, size_t frame_size, int64_t duration);
	
		void add_media(const media_stream_t& ms)
		{
			if(active_)
				return;
			medias_.push_back(ms);
		}

		void clear_medias()
		{
			if (active_)
				return;
			medias_.clear();
		}

	protected:
		void append_media_formats();
		std::string signature(const std::string& stream, int64_t ts)const;
		std::string make_url()const;

		bool connect();
		void disconnect();

		void handle_connect();
		void handle_disconnect();
		void handle_receive_require_keyframe(const std::string& mid);
		void handle_timer(const asio::error_code& ec);
		void handle_frame(const std::string& mid, const std::string& sid, const std::string& tid, const rtpx::sdp_format& fmt, const av_frame_t& frame);
	public:
		sys::mutex_callback<sfu_connected_t> on_connected;
		sys::mutex_callback<sfu_disconnected_t> on_disconnected;
		sys::mutex_callback<sfu_required_keyframe_t> on_required_keyframe;
		sys::mutex_callback<sfu_frame_t> on_frame;

	protected:

		mutable std::mutex mutex_;
		std::string host_;
		std::string stream_;
		std::string key_;

		std::atomic_bool active_;
		spdlogger_ptr log_;
		asio::io_service& ios_;

		std::vector<media_stream_t> medias_;

		rtpx::rtp_session_ptr sess_;
		asio::steady_timer timer_;
	};


};