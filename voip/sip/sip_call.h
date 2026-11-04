#pragma once
#include "../call.h"
#include <sip/sip_connection.h>
#include "bfcp.h"

namespace voip
{
	namespace sip
	{
		class endpoint;
		typedef std::shared_ptr<endpoint> endpoint_ptr;

		class sip_call:public call
		{
		public:
			friend endpoint;
			sip_call(endpoint_ptr ep,sip_connection_ptr con, const std::string& local_alias, const std::string& remote_alias, direction_t direction,
				const std::string& nat_address, int port, rtpx::port_range_ptr rtp_ports);
			~sip_call();

			bool set_invite(const sip_message& invite);

			virtual std::string id()const;
			virtual bool start(bool audio, bool video, bool use_rtp_address);
			virtual void stop(voip::call::reason_code_t reason);
			virtual bool require_keyframe();
			virtual bool require_keyframe(const std::string& mid);
			virtual bool answer();
			virtual bool refuse();
			virtual bool request_presentation_role();
			virtual void release_presentation_role();
			virtual bool has_presentation_role();
			virtual bool remote_addr(std::string& ip, int& port);

			std::string con_id()const;

			void on_re_invite(const sip_message& msg);
			void on_trying(const sip_message& msg);
			void on_ringing(const sip_message& msg);
			void on_ack(const sip_message& msg);
			void on_info(const sip_message& msg);
			void on_bye(const sip_message& msg);
			void on_decline(const sip_message& msg);
			void on_options(const sip_message& msg);
			void on_response(const sip_message& msg);
			void on_update(const sip_message& msg);

			void make_call_id();

			void set_gkclient(bool isgk) { is_gk_client_ = isgk; }

			void set_video_mid(const std::string& mid) { video_mid_ = mid; }
			void set_audio_mid(const std::string& mid) { audio_mid_ = mid; }

			// 辅流（BFCP）支持开关。开启后主叫 offer 携带第二条 video（mid=extend_video）与
			// m=application UDP/BFCP；被叫按对端 offer 自适应。须在 start() 之前设置
			void set_content_support(bool support) { content_support_ = support; }


			sip_connection_ptr get_con()const;


		private:
			// 从全局能力集（rtpx::media_capabilities::global()）创建媒体流并生成 send 格式，
			// offer 的 rtpmap 即能力集内容；应答方向的格式交集过滤由 rtpx 在 set_remote_sdp 内完成
			bool add_audio_channel();
			bool add_video_channel();
			// 辅流视频流（mid=extend_video），格式取自能力集，带 a=content:slides 与
			// a=label:<mstrm>（RFC 4796/4574）；同时生成配套的 BFCP application 行声明
			// （m=application UDP/BFCP，a=floorid:2 mstrm:<label>），主叫 offer 时插入。
			// BFCP 授予后由 call::send_video_frame(is_extend=true) 发送辅流
			bool add_extend_video_channel();
		private:
			// 生成发给对端的 SDP 收尾处理：剥离 UDP 前缀、填充本端地址端口、补充带宽与方向信息
			void postprocess_local_sdp(rtpx::sdp& sdp);
		private:
			bool invite();
			bool invite_rsp(const sip_message& invite, const std::string& sdp);
			bool bye();
			bool trying(const sip_message& invite);
			bool ringing(const sip_message& invite);
			bool ack(const sip_message& msg);
			bool options();
		private:
			void make_contact(sip_address& contact);
			uint32_t get_cseq();
			void handle_timer(call_ptr call,const std::error_code& ec);
			void handle_sipcon_close(call_ptr call, sip_connection_ptr con);

			void set_con(sip_connection_ptr con);
			void clear_con();

			uint16_t make_bfcp_transaction_id();
		private:
			// ---------- BFCP 辅流（对应 h323_call 的 H.239） ----------
			// 绑定本地 BFCP 端口并初始化默认参数（主叫 offer 前调用）
			bool bind_bfcp_socket();
			// 从对端 offer/answer 的 BFCP 行取服务端地址与 confid/userid/floorid，完成协商
			bool start_bfcp(const rtpx::sdp& remote_sdp);
			void stop_bfcp();
			void handle_bfcp_message(const bfcp::message& msg, const asio::ip::udp::endpoint& ep);
			// 在 SDP 中查找 BFCP application 行
			const rtpx::sdp_media* find_bfcp_media(const rtpx::sdp& sdp)const;
			// offer/answer 中提取 BFCP 行的属性（a=confid/userid/floorid/floorctrl）
			void parse_bfcp_attributes(const rtpx::sdp_media& m);
			// 统一的辅流令牌状态回调出口（对齐 h323 的 on_h239_presentation_role_changed）
			void on_bfcp_presentation_role_changed(bool owned, bool active);
		private:
			std::atomic<bool> active_;
			bool is_gk_client_ = false;
			endpoint_ptr ep_;
			asio::steady_timer timer_;

			mutable std::mutex con_mutex_;
			sip_connection_ptr con_tmp_;
			sip_connection_ptr con_;

			std::atomic<uint32_t> cseq_base_;
			std::string call_id_;
			std::string my_tag_;
			std::string other_tag_;
			std::vector<std::string> record_route_;

			sip_message invite_;

			bool got_ack = false;
			bool got_bye = false;

			int heartbeat_ = 0;
			int keyframe_ = 3;

			std::atomic<uint16_t> bfcp_transaction_id_;

			std::string video_mid_;
			std::string audio_mid_;

			// ---------- BFCP 辅流状态（对齐 h323 的 h239 状态） ----------
			bool content_support_ = false;
			std::shared_ptr<bfcp_client> bfcp_;
			bool bfcp_negotiated_ = false;              // SDP 中 BFCP 协商成功
			bool bfcp_owned_ = false;                   // 持有辅流令牌（h239_token_owned_）
			bool bfcp_pending_ = false;                 // 我方请求进行中
			bool bfcp_remote_owned_ = false;            // 对端持有（我方作为 chair 授予）
			uint32_t bfcp_conference_id_ = 0;
			uint16_t bfcp_user_id_ = 0;
			uint16_t bfcp_remote_user_id_ = 0;
			uint16_t bfcp_floor_id_ = 0;
			uint16_t bfcp_mstrm_ = 12;                  // 媒体流编号，与 extend_video 行的 a=label 对应（RFC 4583）
			rtpx::sdp_media bfcp_media_;                // 主叫 offer 的 BFCP application 行声明（add_extend_video_channel 生成）
			uint16_t bfcp_floor_request_id_ = 0;        // 我方请求的 Floor Request ID（对端分配）
			uint16_t bfcp_remote_floor_request_id_ = 0; // 对端请求的 Floor Request ID（我方分配）
			uint16_t bfcp_pending_transaction_ = 0;
			uint16_t bfcp_local_port_ = 0;


		};


	}
}
