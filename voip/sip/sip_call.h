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
				const std::string& nat_address, int port, litertp::port_range_ptr rtp_ports);
			~sip_call();

			bool set_invite(const sip_message& invite);

			virtual std::string id()const;
			virtual bool start(bool audio, bool video);
			virtual void stop(voip::call::reason_code_t reason);
			virtual bool require_keyframe();
			virtual bool answer();
			virtual bool refuse();
			virtual bool request_presentation_role();
			virtual void release_presentation_role();
			virtual bool has_presentation_role();

			std::string con_id()const;

			void on_trying(const sip_message& msg);
			void on_ringing(const sip_message& msg);
			void on_ack(const sip_message& msg);
			void on_info(const sip_message& msg);
			void on_bye(const sip_message& msg);
			void on_decline(const sip_message& msg);
			void on_options(const sip_message& msg);
			void on_response(const sip_message& msg);

			void make_call_id();

			void set_gkclient(bool isgk) { is_gk_client_ = isgk; }

		private:
			bool add_audio_channel_g711();
			bool add_video_channel_h264();
			bool add_video_channel_h265();
			bool add_extend_video_channel_h264();
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
			sip_connection_ptr get_con()const;
			void clear_con();

			uint16_t make_bfcp_transaction_id();
		private:
			std::atomic<bool> active_;
			bool is_gk_client_ = false;
			endpoint_ptr ep_;
			asio::steady_timer timer_;
			sip_connection_ptr con_tmp_;
			sip_connection_ptr con_;
			std::atomic<uint32_t> cseq_base_;
			std::string call_id_;
			std::string my_tag_;
			std::string other_tag_;

			sip_message invite_;

			bool got_ack = false;
			bool got_bye = false;

			int heartbeat_ = 0;
			int keyframe_ = 3;

			std::atomic<uint16_t> bfcp_transaction_id_;

		};


	}
}
