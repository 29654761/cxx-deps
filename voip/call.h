#pragma once
#include <litertp/rtp_session.h>
#include <sip/voip_uri.h>
#include <spdlog/spdlogger.hpp>

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

		enum class reason_code_t
		{
			ok,
			error,
			timeout,
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

		typedef std::function<void(call_ptr)> on_ringing_t;
		typedef std::function<void(call_ptr, media_type_t,const std::string&)> on_open_media_t;
		typedef std::function<void(call_ptr, media_type_t, const std::string&)> on_close_media_t;
		typedef std::function<void(call_ptr)> on_received_require_keyframe_t;
		typedef std::function<void(call_ptr, uint32_t, uint16_t, int, int ,const av_frame_t*,bool is_extend)> on_frame_t;

		typedef std::function<void(call_ptr, reason_code_t)> on_destroy_t;
		typedef std::function<void(call_ptr, reason_code_t)> on_hangup_t;
		typedef std::function<void(call_ptr, const std::string&,const std::string&)> on_connected_t;
		typedef std::function<void(voip::call_ptr call, const std::string& called_alias, const std::string& remote_alias,
			const std::string& remote_addr, int remote_port)> on_incoming_call_t;
		typedef std::function<void(call_ptr, bool, bool)> on_presentation_role_changed_t;

		call(const std::string& local_alias,const std::string& remote_alias, direction_t direction,
			const std::string& nat_address, int port,litertp::port_range_ptr rtp_ports);
		virtual ~call();
		void set_logger(spdlogger_ptr log) { log_ = log; }
		void set_url(const voip_uri& url) { url_ = url; }
		void set_auto_keyframe_interval(int interval) { auto_keyframe_interval_ = interval; }
		virtual std::string id()const = 0;
		virtual bool start(bool audio,bool video) = 0;
		virtual void stop(voip::call::reason_code_t reason) = 0;
		virtual bool require_keyframe() = 0;
		virtual bool answer() = 0;
		virtual bool refuse() = 0;
		virtual bool request_presentation_role() = 0;
		virtual void release_presentation_role() = 0;
		virtual bool has_presentation_role() = 0;

		void set_on_remote_ringing(on_ringing_t handler) { on_remote_ringing = handler; }
		void set_on_connected(on_connected_t handler) { on_connected = handler; }
		void set_on_open_media(on_open_media_t handler) { on_open_media = handler; }
		void set_on_close_media(on_close_media_t handler) { on_close_media = handler; }
		void set_on_received_require_keyframe(on_received_require_keyframe_t handler) { on_received_require_keyframe = handler; }
		void set_on_frame(on_frame_t handler) { on_frame = handler; }
		void set_on_hangup(on_hangup_t handler) { on_hangup = handler; }
		void set_on_presentation_role_changed(on_presentation_role_changed_t handler) { on_presentation_role_changed = handler; }

		bool send_video_frame(const uint8_t* frame, uint32_t size, uint32_t duration,bool is_extend);
		bool send_audio_frame(const uint8_t* frame, uint32_t size, uint32_t duration);

		bool is_inactive()const;
		void update();
		void set_max_bitrate(int v) { max_bitrate_ = v; }

		call_type_t call_type()const { return call_type_; }
	protected:
		void set_on_destroy(on_destroy_t handler) { on_destroy = handler; }
		void set_on_incoming_call(on_incoming_call_t handler) { on_incoming_call = handler; }

		void invoke_connected();
		void invoke_hangup(reason_code_t reason);
	private:
		static void s_litertp_on_frame(void* ctx, const char* mid, uint32_t ssrc, uint16_t pt, int frequency, int channels, const av_frame_t* frame);
		static void s_litertp_on_received_require_keyframe(void* ctx, uint32_t ssrc, int mode);

	protected:
		on_ringing_t on_remote_ringing;
		on_open_media_t on_open_media;
		on_close_media_t on_close_media;
		on_received_require_keyframe_t on_received_require_keyframe;
		on_frame_t on_frame;
		on_destroy_t on_destroy;
		on_hangup_t on_hangup;
		on_connected_t on_connected;
		on_incoming_call_t on_incoming_call;
		on_presentation_role_changed_t on_presentation_role_changed;

		int auto_keyframe_interval_ = 5;
		mutable std::recursive_mutex mutex_;
		int max_bitrate_ = 1920;
		voip_uri url_;
		std::string local_alias_;
		std::string remote_alias_;
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