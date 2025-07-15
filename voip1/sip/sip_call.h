#pragma once
#include "../call.h"
#include <hypertext/sip/sip_session.h>

namespace voip
{
	namespace sip
	{
		class endpoint;

		class sip_call:public call
		{
		public:
			friend endpoint;
			sip_call(spdlogger_ptr log, endpoint& ep,sip_session_ptr sip,const std::string& alias, direction_t direction,
				const std::string& nat_address, int port, litertp::port_range_ptr rtp_ports);
			~sip_call();



			bool set_invite(const sip_message& invite);

			virtual std::string id()const;
			virtual bool start(bool audio, bool video);
			virtual void stop();
			virtual bool require_keyframe();
			virtual bool answer();
			virtual bool refuse();

			std::string sess_id()const;

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
		protected:
			virtual void internal_stop();
		private:
			bool add_audio_channel_g711();
			bool add_video_channel_h264();
			bool add_video_channel_h265();
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
			void run_timer();
		private:
			bool active_ = false;
			bool is_gk_client_ = false;
			std::shared_ptr<std::thread> timer_;
			sys::signal signal_;

			endpoint& ep_;
			sip_session_ptr sip_;
			std::atomic<uint32_t> cseq_base_;
			std::string call_id_;
			std::string my_tag_;
			std::string other_tag_;

			sip_message invite_;

			bool got_ack = false;
			bool got_bye = false;
		};


	}
}
