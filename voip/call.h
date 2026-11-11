#pragma once
#include <asio.hpp>
#include <deque>
#include <mutex>
#include <rtpx/rtp_session.h>
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
		typedef std::function<void(voip::call_ptr call, const std::string& local_alias, const std::string& remote_alias,const std::string& called_alias,
			const std::string& remote_addr, int remote_port,bool reg)> on_incoming_call_t;
		typedef std::function<void(call_ptr, bool, bool)> on_presentation_role_changed_t;
		// 收到对端电话按键（DTMF）。digit 取值 '0'-'9'、'*'、'#'、'A'-'D'；duration_ms 为按键时长(毫秒)，来源不提供时为 0。
		// SIP 的 RFC 4733 RTP 事件、SIP INFO，以及 H.323 的 UserInputIndication 都汇总到这里。
		// 注意：回调在 RTP 接收线程或信令线程上执行，不要在回调里做耗时操作。
		typedef std::function<void(call_ptr, char digit, int duration_ms)> on_dtmf_t;

		call(const std::string& local_alias,const std::string& remote_alias, direction_t direction,
			const std::string& nat_address, int port, rtpx::port_range_ptr rtp_ports,
			asio::io_context& ioc, spdlogger_ptr log);
		virtual ~call();
		void set_logger(spdlogger_ptr log) { log_ = log; }
		void set_url(const voip_uri& url) { url_ = url; }
		void set_auto_keyframe_interval(int interval) { auto_keyframe_interval_ = interval; }
		virtual std::string id()const = 0;
		virtual bool start(bool audio,bool video,bool use_rtp_address) = 0;
		virtual void stop(voip::call::reason_code_t reason) = 0;
		virtual bool require_keyframe() = 0;
		// 仅请求指定 mid 的视频流出关键帧（如 "extend_video" 辅流），不影响其他视频流；
		// 默认不支持（返回 false），由具备按流请求能力的子类实现
		virtual bool require_keyframe(const std::string& mid) { return false; }
		virtual bool answer() = 0;
		virtual bool refuse() = 0;
		virtual bool request_presentation_role() = 0;
		virtual void release_presentation_role() = 0;
		virtual bool has_presentation_role() = 0;
		virtual bool remote_addr(std::string& ip, int& port) = 0;

		void set_on_remote_ringing(on_ringing_t handler) { on_remote_ringing = handler; }
		void set_on_connected(on_connected_t handler) { on_connected = handler; }
		void set_on_open_media(on_open_media_t handler) { on_open_media = handler; }
		void set_on_close_media(on_close_media_t handler) { on_close_media = handler; }
		void set_on_received_require_keyframe(on_received_require_keyframe_t handler) { on_received_require_keyframe = handler; }
		void set_on_frame(on_frame_t handler) { on_frame = handler; }
		void set_on_hangup(on_hangup_t handler) { on_hangup = handler; }
		void set_on_presentation_role_changed(on_presentation_role_changed_t handler) { on_presentation_role_changed = handler; }
		void set_on_dtmf(on_dtmf_t handler) { on_dtmf = handler; }

		bool send_video_frame(const uint8_t* frame, uint32_t size, uint32_t duration,bool is_extend);
		bool send_audio_frame(const uint8_t* frame, uint32_t size, uint32_t duration);

		// 向对端发送按键（DTMF），立即返回，按键在后台按顺序逐个发出。
		// digits：只接受 0-9 * # A-D（小写 a-d 转大写），逗号 ',' 表示停顿 500ms（等对端语音提示），其它字符忽略。
		// duration_ms：每个键的按下时长，限制在 [40, 1000]，键与键之间另有 100ms 间隔。
		// 发送方式由子类按协商结果决定（见 dtmf_method）：SIP 优先 RFC 4733、否则 SIP INFO；H.323 走 H.245 UserInputIndication。
		// 返回 false 表示 digits 里没有可发送的按键、通话未接通或已结束
		bool send_dtmf(const std::string& digits, int duration_ms = 160);

		bool is_inactive()const;
		void update();
		void set_max_bitrate(int v) { max_bitrate_ = v; }

		call_type_t call_type()const { return call_type_; }

		void use_rtp_address(bool use) { use_rtp_address_ = use; }

		// RFC 4733 事件码转按键字符：0-9 -> '0'-'9'，10 -> '*'，11 -> '#'，12-15 -> 'A'-'D'；其他（如 16=Flash）返回 0
		static char dtmf_event_to_digit(uint8_t event);
	protected:
		void set_on_destroy(on_destroy_t handler) { on_destroy = handler; }
		void set_on_incoming_call(on_incoming_call_t handler) { on_incoming_call = handler; }

		void invoke_connected();
		void invoke_hangup(reason_code_t reason);
		// 统一的按键出口：过滤非法字符后回调 on_dtmf
		void invoke_dtmf(char digit, int duration_ms);

		// 发按键的方式
		enum class dtmf_method_t
		{
			none,		// 发不了（未协商 telephone-event 且子类没有信令方式）
			rfc4733,	// RTP telephone-event 事件包
			signaling,	// 信令：SIP INFO / H.245 UserInputIndication
		};
		// 选择发按键的方式。默认：对端 SDP 协商了 telephone-event 就走 RFC 4733，否则 none；子类可覆盖
		virtual dtmf_method_t dtmf_method();
		// 走信令发一个按键，由子类实现；默认不支持
		virtual bool send_dtmf_signaling(char digit, int duration_ms) { return false; }
		// 停止发按键：清空待发队列并取消定时器（通话结束时由子类 stop() 调用）
		void cancel_dtmf();
	private:
		// rtpx 回调桥接，转发到公开的 on_frame / on_received_require_keyframe / on_dtmf
		void on_rtpx_frame(const std::string& mid, const std::string& sid, const std::string& tid, const rtpx::sdp_format& fmt, const av_frame_t& frame);
		void on_rtpx_receive_require_keyframe(const std::string& mid);
		void on_rtpx_dtmf(const std::string& mid, const std::string& sid, const std::string& tid, uint8_t event, uint32_t duration_ms);
		// 从媒体的本地格式中挑一个用于发送的 payload type：
		// 音频优先 PCMA、其次 PCMU（与旧版 litertp 行为一致），并跳过 telephone-event/rtx
		bool pick_send_pt(rtpx::media_stream_ptr ms, uint8_t& pt);
		// 进程内注册一次默认能力集（PCMA/PCMU/telephone-event/H264/H265）。
		// offer 从能力集生成，answer 只保留对端 offer 与能力集的交集（见 rtpx::media_capabilities）
		static void register_default_capabilities();

		// 发按键的调度：取队列下一个键开始发送；队列空则结束
		void dtmf_next();
		// RFC 4733 一个键开始发送：确定音频流、PT 与起始时间戳。失败返回 false（调用方改走信令）
		bool dtmf_rfc4733_begin(char digit, int duration_ms);
		// RFC 4733 每 50ms 一拍：按键未结束发更新包，到时长后连发 3 个结束包并转下一个键
		void dtmf_rfc4733_tick();
		// delay_ms 后在 io 线程上执行 fn（持弱引用，通话销毁后不再执行）
		void dtmf_schedule(int delay_ms, void (call::*fn)());

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
		on_dtmf_t on_dtmf;

		int auto_keyframe_interval_ = 5;
		mutable std::recursive_mutex mutex_;
		int max_bitrate_ = 1920;
		voip_uri url_;
		std::string local_alias_;
		std::string remote_alias_;
		std::string nat_address_;
		int local_port_ = 0;

		direction_t direction_ = direction_t::incoming;
		rtpx::port_range_ptr rtp_ports_;
		rtpx::rtp_session_ptr rtp_;


		std::atomic<int64_t> updated_at_;
		spdlogger_ptr log_;

		call_type_t call_type_ = call_type_t::unknown;

		std::atomic<bool> triggered_connected_;

		bool use_rtp_address_ = false;

	private:
		// ---------- 发按键（send_dtmf）----------
		// 待发的一个按键；digit 为 ',' 时表示停顿
		struct dtmf_tone_t
		{
			char digit;
			int duration_ms;
		};
		std::mutex dtmf_mutex_;					// 保护 dtmf_queue_ / dtmf_running_ / dtmf_closed_ / dtmf_gen_ / dtmf_timer_
		std::deque<dtmf_tone_t> dtmf_queue_;	// 待发按键，按调用顺序
		bool dtmf_running_ = false;				// 是否已有发送链在跑（同一时刻只有一条定时器链，天然串行）
		bool dtmf_closed_ = false;				// cancel_dtmf 后置 true，不再接受新按键
		uint32_t dtmf_gen_ = 0;					// 发送链代际号，cancel_dtmf 时加一，让已出队的旧回调失效
		asio::steady_timer dtmf_timer_;

		// 正在发送的 RFC 4733 按键，只在定时器链上访问
		struct dtmf_rfc4733_t
		{
			rtpx::media_stream_ptr ms;	// 音频流
			uint8_t audio_pt = 0;		// 借用其发送器的 SSRC/序号
			uint8_t te_pt = 0;			// 对端 telephone-event 的负载类型
			int ts_per_ms = 8;			// 每毫秒的时间戳单位数（telephone-event 一般 8000Hz）
			uint8_t event = 0;			// 事件码
			uint32_t timestamp = 0;		// 本次按键起始时间戳
			int duration_ms = 0;		// 按下时长
			int elapsed_ms = 0;			// 已发送时长
		} te_;
		bool te_has_last_ = false;		// 是否发过按键
		uint32_t te_last_end_ts_ = 0;	// 上一个按键结束时的时间戳：音频没在发时时间戳不动，新按键起点不能与上一个重合，否则对端按时间戳去重会吞键
	};

	
}