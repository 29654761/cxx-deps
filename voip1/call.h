#pragma once
#include <litertp/rtp_session.h>
#include <hypertext/sip/voip_uri.h>
#include <spdlog/spdlogger.hpp>
#include <sys2/mutex_callback.hpp>

namespace voip
{
	enum class call_type_t
	{
		unknown,
		sip,
		h323,
	};

	class call;
	typedef std::shared_ptr<call> call_ptr;

	class call:public std::enable_shared_from_this<call>
	{
	public:

		enum class status_code_t
		{
			ok,
			error,
			net_error,
			hangup,
			busy,
			h225_connect_failed,
			h225_connect_failed_rsp,
			h245_connect_failed,
			h245_listen_failed,
			h245_terminal_capability_set_failed,
			h245_terminal_capability_set_reject,
			h245_master_slave_determination_reject,
			h245_master_slave_determination_reject_rsp,
			h245_open_logical_channel_reject,
			h245_open_logical_channel_reject_rsp,
			h245_open_logical_channel_ack_failed,
			sip_connect_failed,

		};

		enum class direction_t
		{
			outgoing,
			incoming,
		};

		typedef void (*on_status_t)(void* ctx, call_ptr call,status_code_t code);
		typedef void (*on_ringing_t)(void* ctx, call_ptr call);
		typedef void (*on_open_media_t)(void* ctx, call_ptr call, media_type_t mt);
		typedef void (*on_close_media_t)(void* ctx, call_ptr call, media_type_t mt);
		typedef void (*on_received_require_keyframe_t)(void* ctx, call_ptr call);
		typedef void (*on_destroy_t)(void* ctx, call_ptr call);
		typedef void (*on_connected_t)(void* ctx, call_ptr call);

		call(spdlogger_ptr log,const std::string& alias, direction_t direction,
			const std::string& nat_address, int port,litertp::port_range_ptr rtp_ports);
		virtual ~call();
		void set_url(const voip_uri& url) { url_ = url; }
		void set_auto_keyframe_interval(int interval) { auto_keyframe_interval_ = interval; }
		virtual std::string id()const = 0;
		virtual bool start(bool audio,bool video) = 0;
		virtual void stop() = 0;
		virtual bool require_keyframe() = 0;
		virtual bool answer() = 0;
		virtual bool refuse() = 0;

		void wait_for_stoped();
		
		bool send_video_frame(const uint8_t* frame, uint32_t size, uint32_t duration);
		bool send_audio_frame(const uint8_t* frame, uint32_t size, uint32_t duration);

		bool is_inactive()const;
		void update();
		void set_max_bitrate(int v) { max_bitrate_ = v; }

		call_type_t call_type()const { return call_type_; }
	protected:
		virtual void internal_stop() = 0;
		void invoke_connected();
	private:
		static void s_litertp_on_frame(void* ctx, uint32_t ssrc, uint16_t pt, int frequency, int channels, const av_frame_t* frame);
		static void s_litertp_on_received_require_keyframe(void* ctx, uint32_t ssrc, int mode);

	public:
		sys::mutex_callback<on_status_t> on_status;
		sys::mutex_callback<on_ringing_t> on_remote_ringing;
		sys::mutex_callback<on_open_media_t> on_open_media;
		sys::mutex_callback<on_close_media_t> on_close_media;
		sys::mutex_callback<on_received_require_keyframe_t> on_received_require_keyframe;
		sys::mutex_callback<litertp_on_frame_t> on_frame;
		sys::mutex_callback<on_destroy_t> on_destroy;
		sys::mutex_callback<on_connected_t> on_connected;
	protected:
		bool closed_ = false;
		sys::signal signal_close_;
		int auto_keyframe_interval_ = 5;
		mutable std::recursive_mutex mutex_;
		int max_bitrate_ = 1920;
		voip_uri url_;
		std::string alias_;
		std::string nat_address_;
		int local_port_ = 0;

		direction_t direction_ = direction_t::incoming;
		litertp::port_range_ptr rtp_ports_;
		litertp::rtp_session rtp_;


		std::atomic<int64_t> updated_at_;
		spdlogger_ptr log_;

		call_type_t call_type_ = call_type_t::unknown;

		std::atomic<bool> triggered_connected_;
	};

	
}