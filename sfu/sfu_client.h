#pragma once
#include <memory>
#include <string>
#include <mutex>
#include <asio/asio.hpp>
#include <spdlog/spdlogger.hpp>
#include <sys2/mutex_callback.hpp>
#include <rtpx/rtpx_core.h>
#include <json/value.h>
#include <json/reader.h>
#include "sfu.h"

namespace sfu
{
	struct subscribe_item_t
	{
		std::string uid;
		std::string track_id;
		bool subscribe;
	};

	struct subscribe_user_track_t
	{
		std::string uid;
		std::string track_id;
		sfu_frame_t on_frame;
		void* ctx;
	};

	class sfu_client;
	typedef std::shared_ptr<sfu_client> sfu_client_ptr;

	class sfu_client :public std::enable_shared_from_this<sfu_client>
	{
	public:
		sfu_client(asio::io_service& ios, spdlogger_ptr log);
		~sfu_client();


		std::string channel() const;
		void set_channel(const std::string& channel);

		std::string url()const;
		void set_url(const std::string& url);

		virtual bool open(const std::string& url, const std::string& channel);
		virtual void close();
		virtual bool is_available() const { return true; }

		virtual bool send_frame(const std::string& mid, uint8_t pt, const uint8_t* frame, size_t frame_size, int64_t duration);
	
		void add_media(const media_stream_t& ms)
		{
			if (active_)
				return;
			medias_.push_back(ms);
		}
		void clear_medias()
		{
			if (active_)
				return;
			medias_.clear();
		}

		bool add_subscribe_track(const std::string& uid, const std::string& track_id, sfu_frame_t on_frame, void* ctx);
		bool remove_subscribe_track(const std::string& uid, const std::string& track_id, sfu_frame_t on_frame, void* ctx);
		void clear_subscribe_tracks();
	public:
		sys::mutex_callback<sfu_connected_t> on_connected;
		sys::mutex_callback<sfu_disconnected_t> on_disconnected;
		sys::mutex_callback<sfu_required_keyframe_t> on_required_keyframe;
	protected:
		bool connect();
		void disconnect();
		void append_media_formats(rtpx::rtp_session_ptr rtp);
		
		std::vector<subscribe_user_track_t> all_subscribe_tracks() const;

	protected:
		void handle_connect();
		void handle_disconnect();
		void handle_receive_require_keyframe(const std::string& mid);
		void handle_timer(const asio::error_code& ec);
		void handle_frame(const std::string& mid, const std::string& sid, const std::string& tid, const rtpx::sdp_format& fmt, const av_frame_t& frame);

	protected:
		bool sfu_request(const std::string& path,const Json::Value& jreq, Json::Value& jrsp);
		bool sfu_connect(const rtpx::sdp& offer, rtpx::sdp& publish_answer, rtpx::sdp& subscribe_offer);
		bool sfu_publish(const rtpx::sdp& offer, rtpx::sdp& answer);
		bool sfu_subscribe(const std::vector<subscribe_item_t>& items, rtpx::sdp& offer);
		bool sfu_answer_subscribe(const rtpx::sdp& subscribe_answer);

	protected:

		mutable std::mutex mutex_;
		std::string url_;
		std::string channel_;

		std::atomic_bool active_;
		spdlogger_ptr log_;
		asio::io_service& ios_;

		std::vector<media_stream_t> medias_;

		rtpx::rtp_session_ptr pub_rtp_;
		rtpx::rtp_session_ptr sub_rtp_;
		asio::steady_timer timer_;

		mutable std::shared_mutex subscribe_tracks_mutex_;
		std::vector<subscribe_user_track_t> subscribe_tracks_;
	};


}