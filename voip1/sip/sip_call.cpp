#include "sip_call.h"
#include "endpoint.h"
#include <sys2/util.h>
#include <sys2/string_util.h>

namespace voip
{
	namespace sip
	{
		sip_call::sip_call(spdlogger_ptr log, endpoint& ep, sip_session_ptr sip, const std::string& alias, direction_t direction,
			const std::string& nat_address, int port, litertp::port_range_ptr rtp_ports)
			:call(log,alias,direction,nat_address,port,rtp_ports)
			,cseq_base_(1)
			, ep_(ep)
		{
			sip_ = sip;
			call_type_ = call_type_t::sip;
		}

		sip_call::~sip_call()
		{
			if (log_)
			{
				std::string url = url_.to_string();
				log_->info("SIP call freed. url={}", url)->flush();
			}
		}

		bool sip_call::add_audio_channel_g711()
		{
			uint32_t ssrc = sys::util::random_number<uint32_t>(100000000, 999999999);
			auto ms = rtp_.create_media_stream("audio", media_type_audio, ssrc, nat_address_.c_str(), rtp_ports_, false);
			if (!ms) {
				return false;
			}
			ms->use_rtp_address(true);
			ms->add_local_audio_track(codec_type_pcma, 8, 8000);
			return true;
		}

		bool sip_call::add_video_channel_h264()
		{
			uint32_t ssrc = sys::util::random_number<uint32_t>(100000000, 999999999);
			auto ms=rtp_.create_media_stream("video", media_type_video, ssrc, nat_address_.c_str(), rtp_ports_, false);
			if (!ms) {
				return false;
			}
			ms->use_rtp_address(true);
			ms->add_local_video_track(codec_type_h264,96,90000,true);
			litertp::fmtp fmtp;
			fmtp.set_packetization_mode(1);
			fmtp.set_profile_level_id(0x420028);
			fmtp.set_mbps(245760);
			fmtp.set_mfs(8192);
			fmtp.set_mbr(1920);
			fmtp.set_level_asymmetry_allowed(1);
			ms->set_local_fmtp(96, fmtp);

			return true;
		}

		bool sip_call::add_video_channel_h265()
		{
			uint32_t ssrc = sys::util::random_number<uint32_t>(100000000, 999999999);
			auto ms = rtp_.create_media_stream("video", media_type_video, ssrc, nat_address_.c_str(), rtp_ports_, false);
			if (!ms) {
				return false;
			}
			ms->use_rtp_address(true);
			ms->add_local_video_track(codec_type_h265, 96, 90000, true);
			litertp::fmtp fmtp;
			fmtp.set_profile_id(1);
			fmtp.set_level_id(0x0093);
			fmtp.set_mbps(245760);
			fmtp.set_mfs(8192);
			fmtp.set_mbr(1920);
			fmtp.set_level_asymmetry_allowed(1);
			ms->set_local_fmtp(96, fmtp);

			return true;
		}

		std::string sip_call::id()const
		{
			return call_id_;
		}

		bool sip_call::start(bool audio,bool video)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (active_)
				return true;
			active_ = true;

			if (audio)
			{
				if (!add_audio_channel_g711()) 
				{
					stop();
					return false;
				}
			}
			if (video)
			{
				if (!add_video_channel_h264())
				{
					stop();
					return false;
				}
			}
			if (direction_ == direction_t::outgoing)
			{
				if (!invite())
				{
					stop();
					return false;
				}
			}
			else if (direction_ == direction_t::incoming)
			{
				trying(invite_);
				ringing(invite_);

				sip_address contact = invite_.contact();
				sip_address from = invite_.from();

				std::string called_alias = url_.username;
				std::string remote_alias = from.display;
				if (remote_alias.empty())
				{
					remote_alias = from.url.username;
					if (remote_alias.empty())
					{
						remote_alias = contact.display;
						if (remote_alias.empty())
						{
							remote_alias = contact.url.username;
						}
					}
				}

				std::string remote_addr = contact.url.host;
				int remote_port = contact.url.port;
				if (remote_port == 0)
					remote_port = 5060;
				auto sft = shared_from_this();

				ep_.on_incoming_call.invoke(sft, called_alias, remote_alias, remote_addr, remote_port);
			}

			signal_.reset();
			timer_ = std::make_shared<std::thread>(&sip_call::run_timer, this);
			sys::util::set_thread_name("sip.call", timer_.get());
			return true;
		}

		void sip_call::stop()
		{
			closed_ = true;
			ep_.notify_clear_call();
		}

		void sip_call::internal_stop()
		{
			sip_session_ptr sip;
			{
				std::lock_guard<std::recursive_mutex> lk(mutex_);
				active_ = false;
				signal_.notify();
				if (timer_)
				{
					timer_->join();
					timer_.reset();
				}

				if (sip_)
				{
					if (got_ack)
					{
						for (int i = 0; i < 3; i++)
						{
							if (got_bye)
								break;
							bye();
							std::this_thread::sleep_for(std::chrono::seconds(1));
						}
					}
					sip = sip_;
					sip_.reset();
				}
			}

			if (!is_gk_client_)
			{
				if (sip)
				{
					sip->close();
				}
			}

			rtp_.stop();

			signal_close_.notify();
		}

		bool sip_call::require_keyframe()
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			rtp_.require_keyframe();
			return true;
		}

		bool sip_call::set_invite(const sip_message& invite)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			invite_ = invite;
			call_id_ = invite_.call_id();
			invite.from().get("tag", other_tag_);
			invite.to().get("tag", my_tag_);
			url_.parse(invite_.url());

			return true;
		}

		bool sip_call::answer()
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			litertp::sdp remote_sdp;
			if (!remote_sdp.parse(invite_.body))
			{
				if (sip_)
				{
					auto rsp = ep_.create_response(invite_, nullptr,true);
					rsp.set_status("400");
					rsp.set_message("BadRequest");
					sip_->send_message(rsp);
				}
				stop();
				return false;
			}
			rtp_.set_remote_sdp(remote_sdp,sdp_type_offer);
			auto sdp=rtp_.create_answer();
			std::string s = sdp.to_string();
			/*
			std::stringstream ss;
			ss << "v=0" << std::endl;
			ss << "o=- 1732102601 1 IN IP4 192.168.0.86" << std::endl;
			ss << "s=devgate/1.0.1" << std::endl;
			ss << "t=0 0" << std::endl;
			ss << "m=audio 50000 RTP/AVP 8" << std::endl;
			ss << "c=IN IP4 192.168.0.86" << std::endl;
			ss << "a=rtcp:50001 IN IP4 192.168.0.86" << std::endl;
			ss << "a=sendrecv" << std::endl;
			ss << "a=rtpmap:8 PCMA/8000" << std::endl;
			ss << "a=ssrc:2838114036 cname:mPdeQpJvHzQwoiOkXmHR" << std::endl;
			//ss << "a=ptime:20" << std::endl;
			//ss << "a=maxptime:240" << std::endl;
			ss << "m=video 50002 RTP/AVP 110" << std::endl;
			ss << "c=IN IP4 192.168.0.86" << std::endl;
			ss << "b=AS:1920" << std::endl;
			ss << "b=TIAS:1920000" << std::endl;
			ss << "a=sendrecv" << std::endl;
			ss << "a=rtpmap:110 H264/90000" << std::endl;
			ss << "a=fmtp:110 packetization-mode=1;max-br=1920;max-fs=8192;max-mbps=245760;profile-level-id=420028" << std::endl;
			ss << "a=rtcp:50003 IN IP4 192.168.0.86" << std::endl;
			ss << "a=label:11" << std::endl;
			ss << "a=ssrc:3952510209 cname:mPdeQpJvHzQwoiOkXmHR" << std::endl;
			ss << "a=rtcp-fb:* nack" << std::endl;
			ss << "a=rtcp-fb:* nack pli" << std::endl;
			ss << "a=rtcp-fb:* ccm fir" << std::endl;
			ss << "a=rtcp-fb:* ccm tmmbr" << std::endl;
			ss << "a=content:main" << std::endl;
			*/

			auto sft = shared_from_this();
			for (auto itr = sdp.medias.begin(); itr != sdp.medias.end(); itr++)
			{
				on_open_media.invoke(sft, itr->media_type);
			}

			if (!invite_rsp(invite_, s))
			{
				stop();
				return false;
			}

			rtp_.start();
			return true;
		}

		bool sip_call::refuse()
		{
			auto rsp = ep_.create_response(invite_, nullptr, true);
			rsp.set_status("603");
			rsp.set_message("Decline");
			sip_->send_message(rsp);
			stop();
			return true;
		}

		std::string sip_call::sess_id()const
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (!sip_)
				return "";
			return sip_->id();
		}

		void sip_call::on_trying(const sip_message& msg)
		{

		}

		void sip_call::on_ringing(const sip_message& msg)
		{
			on_remote_ringing.invoke(shared_from_this());
		}

		void sip_call::on_ack(const sip_message& msg)
		{
			got_ack = true;
			auto sft = shared_from_this();
			on_status.invoke(sft, status_code_t::ok);
		}

		void sip_call::on_info(const sip_message& msg)
		{
			if (sip_)
			{
				sip_address contact;
				make_contact(contact);
				auto rsp = ep_.create_response(msg, &contact,false);
				sip_->send_message(rsp);
			}
		}

		void sip_call::on_bye(const sip_message& msg)
		{
			got_bye = true;
			if (sip_)
			{
				auto rsp = ep_.create_response(msg, nullptr, false);
				sip_->send_message(rsp);
				auto sft = shared_from_this();
				on_status.invoke(sft, status_code_t::hangup);
			}
			stop();
		}

		void sip_call::on_decline(const sip_message& msg)
		{
			got_bye = true;
			if (sip_)
			{
				auto rsp = ep_.create_response(msg, nullptr, false);
				sip_->send_message(rsp);
				auto sft = shared_from_this();
				on_status.invoke(sft, status_code_t::busy);

				ack(msg);
			}

			stop();
		}

		void sip_call::on_options(const sip_message& msg)
		{
			if (sip_)
			{
				auto rsp = ep_.create_response(msg, nullptr, false);
				sip_->send_message(rsp);
			}
		}

		void sip_call::on_response(const sip_message& msg)
		{
			uint32_t cseq = 0;
			std::string method;
			msg.get_cseq(cseq, method);
			std::string status = msg.status();
			if (status != "200")
			{
				if (log_)
				{
					std::string smsg = msg.message();
					log_->error("Device response failed: status={},message={}", status, smsg)->flush();
				}
				auto sft = shared_from_this();
				on_status.invoke(sft,status_code_t::busy);
				stop();
				return;
			}

			if (strcasecmp(method.c_str(), "INVITE") == 0)
			{
				litertp::sdp sdp;
				if (sdp.parse(msg.body))
				{
					rtp_.set_remote_sdp(sdp, sdp_type_answer);
				}
				auto sft = shared_from_this();
				for (auto itr = sdp.medias.begin(); itr != sdp.medias.end(); itr++)
				{
					on_open_media.invoke(sft, itr->media_type);
				}

				msg.from().get("tag", my_tag_);
				msg.to().get("tag", other_tag_);
				ack(msg);

				rtp_.start();
			}
			else if (strcasecmp(method.c_str(), "BYE") == 0)
			{
				got_bye = true;
			}
		}

		bool sip_call::invite()
		{
			if (!sip_)
				return false;
			//url_.SetScheme
			uint32_t cseq = get_cseq();
			std::string branch = endpoint::create_branch();
			my_tag_ = endpoint::create_tag();
			sip_address from,to;
			from.url.username = alias_;
			from.url.host = nat_address_;
			from.url.port = local_port_;
			from.set("tag", my_tag_);
			to.url=url_;
			if (!other_tag_.empty()) {
				to.url.set("tag", other_tag_);
			}

			sip_address contact;
			make_contact(contact);

			std::vector<sip_via> vias;
			vias.emplace_back(sip_->is_tcp(), nat_address_, local_port_, "", 0, branch);

			sip_message req = ep_.create_request("INVITE", url_, cseq, from,to, &contact,vias, true);
			req.set_call_id(call_id_);
			req.set_content_type("application/sdp");

			litertp::sdp sdp = rtp_.create_offer();
			for (auto itr = sdp.medias.begin(); itr != sdp.medias.end(); itr++)
			{
				itr->protos.erase("UDP");
			}
			std::string s = sdp.to_string();

			req.set_body(s);
			return sip_->send_message(req);
		}

		bool sip_call::invite_rsp(const sip_message& invite,const std::string& sdp)
		{
			if (!sip_)
				return false;

			sip_address contact;
			make_contact(contact);
			sip_message req = ep_.create_response(invite, &contact,true);

			sip_address to = invite.to();
			to.set("tag", my_tag_);
			req.set_to(to);
			req.set_call_id(call_id_);
			req.set_status("200");
			req.set_message("OK");
			req.set_body(sdp);
			req.set_content_type("application/sdp");
			return sip_->send_message(req);
		}

		bool sip_call::bye()
		{
			if (!sip_)
				return false;

			uint32_t cseq = get_cseq();

			sip_address from, to;
			voip_uri url;
			if (direction_ == direction_t::incoming) 
			{
				url = invite_.contact().url;

				from.url = url_;
				if (!my_tag_.empty()) {
					from.set("tag", my_tag_);
				}

				to = invite_.from();
				if (!other_tag_.empty()) {
					to.set("tag", other_tag_);
				}
			}
			else
			{
				url = url_;

				from.url.username = alias_;
				from.url.host = nat_address_;
				from.url.port = local_port_;
				if (!my_tag_.empty()) {
					from.set("tag", my_tag_);
				}

				to.url = url_;
				if (!other_tag_.empty()) {
					to.set("tag", other_tag_);
				}
			}

			sip_address contact;
			make_contact(contact);

			std::vector<sip_via> vias;
			vias.emplace_back(sip_->is_tcp(), nat_address_, local_port_, "", 0, endpoint::create_branch());

			sip_message req = ep_.create_request("BYE", url, cseq, from, to,&contact, vias, false);
			req.set_call_id(call_id_);

			std::string scseq = std::to_string(cseq);

			return sip_->send_message(req);
		}

		bool sip_call::trying(const sip_message& invite)
		{
			if (!sip_)
				return false;
			sip_message req = ep_.create_response(invite, nullptr,false);
			req.set_call_id(call_id_);
			req.set_status("100");
			req.set_message("Trying");
			return sip_->send_message(req);
		}

		bool sip_call::ringing(const sip_message& invite)
		{
			if (!sip_)
				return false;

			sip_address contact;
			make_contact(contact);
			sip_message req = ep_.create_response(invite, &contact,true);
			my_tag_ = endpoint::create_branch();
			sip_address to = invite.to();
			to.set("tag", my_tag_);
			req.set_to(to);
			req.set_call_id(call_id_);
			req.set_status("180");
			req.set_message("Ringing");
			return sip_->send_message(req);
		}

		bool sip_call::ack(const sip_message& msg)
		{
			if (!sip_)
				return false;
			sip_address from, to;
			from.url.username = alias_;
			from.url.host = nat_address_;
			from.url.port = local_port_;
			if (!my_tag_.empty()) {
				from.set("tag", my_tag_);
			}
			to.url = url_;
			if (!other_tag_.empty()) {
				to.url.set("tag", other_tag_);
			}

			sip_address contact;
			make_contact(contact);

			uint32_t cseq = msg.get_cseq();
			std::vector<sip_via> vias;
			vias.emplace_back(sip_->is_tcp(), nat_address_, local_port_, "", 0, endpoint::create_branch());
			sip_message req = ep_.create_request("ACK", url_, cseq,  from,to,&contact,vias, false);
			req.set_call_id(call_id_);
			req.set_from(msg.from());
			req.set_to(msg.to());

			got_ack = true;
			auto sft = shared_from_this();
			on_status.invoke(sft, status_code_t::ok);
			invoke_connected();
			return sip_->send_message(req);
		}

		bool sip_call::options()
		{
			if (!sip_)
				return false;
			update();
			uint32_t cseq = get_cseq();

			sip_address from, to;
			voip_uri url;
			if (direction_ == direction_t::incoming)
			{
				url = invite_.contact().url;

				from.url = url_;
				if (!my_tag_.empty()) {
					from.set("tag", my_tag_);
				}

				to = invite_.from();
				if (!other_tag_.empty()) {
					to.set("tag", other_tag_);
				}
			}
			else
			{
				url = url_;

				from.url.username = alias_;
				from.url.host = nat_address_;
				from.url.port = local_port_;
				if (!my_tag_.empty()) {
					from.set("tag", my_tag_);
				}

				to.url = url_;
				if (!other_tag_.empty()) {
					to.set("tag", other_tag_);
				}
			}

			sip_address contact;
			make_contact(contact);

			std::vector<sip_via> vias;
			vias.emplace_back(sip_->is_tcp(), nat_address_, local_port_, "", 0, endpoint::create_branch());

			sip_message req = ep_.create_request("OPTIONS", url_, cseq, from, to, &contact,vias, true);
			req.set_call_id(call_id_);

			return sip_->send_message(req);
		}

		void sip_call::make_contact(sip_address& contact)
		{
			if (direction_ == direction_t::outgoing)
			{
				contact.url.username = alias_;
				contact.url.host = nat_address_;
				contact.url.port = local_port_;
				if (sip_)
				{
					std::string ip;
					sip_->get_local_address(ip, contact.url.port);
				}
			}
			else
			{
				contact.url = url_;
			}
			if (sip_ && sip_->is_tcp()) {
				contact.url.add("transport", "tcp");
			}
		}

		uint32_t sip_call::get_cseq()
		{
			uint32_t cseq = 0;
			while (cseq == 0) {
				cseq=cseq_base_.fetch_add(1);
			}
			return cseq;
		}

		void sip_call::make_call_id()
		{
			call_id_ = sys::util::random_string(32);
		}

		void sip_call::run_timer()
		{
			int heartbeat = 0;
			int keyframe = 3;

			while (active_)
			{
				signal_.wait(1000);
				if (!active_)
					break;
				
				heartbeat++;
				if (heartbeat >= 15)
				{
					if (got_ack) {
						options();
					}
					heartbeat = 0;
				}

				if (auto_keyframe_interval_ > 0)
				{
					keyframe++;
					if (keyframe >= auto_keyframe_interval_) {
						require_keyframe();
						keyframe = 0;
					}
				}
			}
		}
	}
}

