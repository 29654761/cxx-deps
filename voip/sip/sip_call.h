#pragma once
#include "../call.h"
#include <sip/sip_connection.h>

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


			sip_connection_ptr get_con()const;

			
		private:
			// 从全局能力集（rtpx::media_capabilities::global()）创建媒体流并生成 send 格式，
			// offer 的 rtpmap 即能力集内容；应答方向的格式交集过滤由 rtpx 在 set_remote_sdp 内完成
			bool add_audio_channel();
			bool add_video_channel();
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


		};


	}
}
