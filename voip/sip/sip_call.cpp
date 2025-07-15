#include "sip_call.h"
#include "endpoint.h"
#include <sys2/util.h>
#include <sys2/string_util.h>

namespace voip
{
	namespace sip
	{
		sip_call::sip_call(endpoint_ptr ep, sip_connection_ptr con, const std::string& local_alias, const std::string& remote_alias, direction_t direction,
			const std::string& nat_address, int port, litertp::port_range_ptr rtp_ports)
			:call(local_alias,remote_alias,direction,nat_address,port,rtp_ports)
			,cseq_base_(1)
			,ep_(ep)
			,timer_(ep_->io_context())
			, active_(false)
		{
			con_tmp_ = con;
			call_type_ = call_type_t::sip;
			bfcp_transaction_id_ = 1;
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

		bool sip_call::add_extend_video_channel_h264()
		{
			auto ms = rtp_.create_application_stream("app", "UDP/BFCP", rtp_ports_);
			if (!ms) {
				return false;
			}
			ms->add_local_attribute("floorctrl", "c-s");
			ms->add_local_attribute("userid", "1");
			ms->add_local_attribute("confid", "1");
			ms->add_local_attribute("floorid", "2 mstrm:2");
			ms->set_local_setup(sdp_setup_actpass);
			return true;
		}

		std::string sip_call::id()const
		{
			return call_id_;
		}

		bool sip_call::start(bool audio,bool video)
		{
			bool experted = false;
			if (!active_.compare_exchange_strong(experted, true))
				return true;


			set_con(con_tmp_);
			con_tmp_.reset();

			auto con = get_con();
			if (!con)
			{
				stop(call::reason_code_t::sip_connect_failed);
				return false;
			}
			auto self = shared_from_this();
			if (!con->start())
			{
				stop(call::reason_code_t::sip_connect_failed);
				return false;
			}

			if (audio)
			{
				if (!add_audio_channel_g711()) 
				{
					stop(call::reason_code_t::error);
					return false;
				}
			}
			if (video)
			{
				if (!add_video_channel_h264())
				{
					stop(call::reason_code_t::error);
					return false;
				}
			}



			if (direction_ == direction_t::outgoing)
			{
				if (!invite())
				{
					stop(call::reason_code_t::error);
					return false;
				}
			}
			else if (direction_ == direction_t::incoming)
			{
				trying(invite_);
				ringing(invite_);

				sip_address contact = invite_.contact();
				sip_address from = invite_.from();

				remote_alias_ = from.display;
				if (remote_alias_.empty())
				{
					remote_alias_ = from.url.username;
					if (remote_alias_.empty())
					{
						remote_alias_ = contact.display;
						if (remote_alias_.empty())
						{
							remote_alias_ = contact.url.username;
						}
					}
				}

				std::string remote_addr = contact.url.host;
				int remote_port = contact.url.port;
				if (remote_port == 0)
					remote_port = 5060;

				if (on_incoming_call)
				{
					on_incoming_call(self, local_alias_, remote_alias_, remote_addr, remote_port);
				}
			}


			timer_.expires_after(std::chrono::milliseconds(1000));
			timer_.async_wait(std::bind(&sip_call::handle_timer, this, self, std::placeholders::_1));
			

			return true;
		}

		void sip_call::stop(voip::call::reason_code_t reason)
		{
			bool experted = true;
			if (!active_.compare_exchange_strong(experted, false))
				return;

			rtp_.stop();
			std::error_code ec;
			timer_.cancel(ec);

			sip_connection_ptr con = get_con();
			if (con)
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
			}
			clear_con();
			if (!is_gk_client_)
			{
				if (con)
				{
					con->stop();
				}
			}
			if (on_destroy)
			{
				auto self = shared_from_this();
				on_destroy(self, reason);
			}
		}


		bool sip_call::require_keyframe()
		{
			rtp_.require_keyframe();
			return true;
		}

		bool sip_call::set_invite(const sip_message& invite)
		{
			invite_ = invite;
			call_id_ = invite_.call_id();
			invite.from().get("tag", other_tag_);
			invite.to().get("tag", my_tag_);
			url_.parse(invite_.url());

			return true;
		}

		bool sip_call::answer()
		{
			litertp::sdp remote_sdp;
			if (!remote_sdp.parse(invite_.body))
			{
				auto con = get_con();
				if (con)
				{
					auto rsp = ep_->create_response(invite_, nullptr,true);
					rsp.set_status("400");
					rsp.set_msg("BadRequest");
					con->send_message(rsp);
				}
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

			auto self = shared_from_this();
			for (auto itr = sdp.medias.begin(); itr != sdp.medias.end(); itr++)
			{
				if (on_open_media)
				{
					on_open_media(self, itr->media_type, itr->mid);
				}
			}

			if (!invite_rsp(invite_, s))
			{
				return false;
			}

			rtp_.start();
			return true;
		}

		bool sip_call::refuse()
		{
			auto rsp = ep_->create_response(invite_, nullptr, true);
			rsp.set_status("603");
			rsp.set_msg("Decline");
			auto con = get_con();
			if (!con)
				return false;

			con->send_message(rsp);
			return true;
		}

		bool sip_call::request_presentation_role()
		{
			return false;
		}
		void sip_call::release_presentation_role(){}
		bool sip_call::has_presentation_role() {
			return false;
		}

		std::string sip_call::con_id()const
		{
			auto con = get_con();
			if (!con)
				return "";
			return con->id();
		}

		void sip_call::on_trying(const sip_message& msg)
		{

		}

		void sip_call::on_ringing(const sip_message& msg)
		{
			sip_address remote_contact;
			if (msg.contact(remote_contact))
			{
				remote_alias_ = remote_contact.display;
				if (remote_alias_.empty())
				{
					remote_alias_ = remote_contact.url.username;
				}
			}

			if (on_remote_ringing)
			{
				auto self = shared_from_this();
				on_remote_ringing(self);
			}
		}

		void sip_call::on_ack(const sip_message& msg)
		{
			got_ack = true;
			auto self = shared_from_this();
			invoke_connected();
		}

		void sip_call::on_info(const sip_message& msg)
		{
			auto con = get_con();
			if (con)
			{
				sip_address contact;
				make_contact(contact);
				auto rsp = ep_->create_response(msg, &contact,false);
				con->send_message(rsp);
			}
		}

		void sip_call::on_bye(const sip_message& msg)
		{
			got_bye = true;
			auto con = get_con();
			if (con)
			{
				auto rsp = ep_->create_response(msg, nullptr, false);
				con->send_message(rsp);
				auto self = shared_from_this();
			}
			if (on_hangup)
			{
				auto self = shared_from_this();
				on_hangup(self, call::reason_code_t::hangup);
			}
		}

		void sip_call::on_decline(const sip_message& msg)
		{
			got_bye = true;
			auto con = get_con();
			if (con)
			{
				auto rsp = ep_->create_response(msg, nullptr, false);
				con->send_message(rsp);
				auto self = shared_from_this();
				ack(msg);
			}

			if (on_hangup)
			{
				auto self = shared_from_this();
				on_hangup(self, call::reason_code_t::busy);
			}
		}

		void sip_call::on_options(const sip_message& msg)
		{
			auto con = get_con();
			if (con)
			{
				auto rsp = ep_->create_response(msg, nullptr, false);
				con->send_message(rsp);
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
					std::string smsg = msg.msg();
					log_->error("Device response failed: status={},message={}", status, smsg)->flush();
				}
				if (on_hangup)
				{
					auto self = shared_from_this();
					on_hangup(self, call::reason_code_t::error);
				}
				return;
			}

			if (strcasecmp(method.c_str(), "INVITE") == 0)
			{
				sip_address remote_contact;
				if (msg.contact(remote_contact))
				{
					remote_alias_ = remote_contact.display;
					if (remote_alias_.empty())
					{
						remote_alias_ = remote_contact.url.username;
					}
				}


				litertp::sdp sdp;
				if (sdp.parse(msg.body))
				{
					rtp_.set_remote_sdp(sdp, sdp_type_answer);
				}
				auto self = shared_from_this();
				for (auto itr = sdp.medias.begin(); itr != sdp.medias.end(); itr++)
				{
					if (on_open_media)
					{
						on_open_media(self, itr->media_type, itr->mid);
					}
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
			auto con = get_con();
			if (!con)
				return false;
			//url_.SetScheme
			uint32_t cseq = get_cseq();
			std::string branch = endpoint::create_branch();
			my_tag_ = endpoint::create_tag();
			sip_address from,to;
			from.url.username = local_alias_;
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
			vias.emplace_back(con->is_tcp(), nat_address_, local_port_, "", 0, branch);

			sip_message req = ep_->create_request("INVITE", url_, cseq, from,to, &contact,vias, true);
			req.set_call_id(call_id_);
			req.set_content_type("application/sdp");

			litertp::sdp sdp = rtp_.create_offer();
			for (auto itr = sdp.medias.begin(); itr != sdp.medias.end(); itr++)
			{
				itr->protos.erase("UDP");
			}
			std::string s = sdp.to_string();

			req.set_body(s);
			return con->send_message(req);
		}

		bool sip_call::invite_rsp(const sip_message& invite,const std::string& sdp)
		{
			auto con = get_con();
			if (!con)
				return false;

			sip_address contact;
			make_contact(contact);
			sip_message req = ep_->create_response(invite, &contact,true);

			sip_address to = invite.to();
			to.set("tag", my_tag_);
			req.set_to(to);
			req.set_call_id(call_id_);
			req.set_status("200");
			req.set_msg("OK");
			req.set_body(sdp);
			req.set_content_type("application/sdp");
			return con->send_message(req);
		}

		bool sip_call::bye()
		{
			auto con = get_con();
			if (!con)
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

				from.url.username = local_alias_;
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
			vias.emplace_back(con->is_tcp(), nat_address_, local_port_, "", 0, endpoint::create_branch());

			sip_message req = ep_->create_request("BYE", url, cseq, from, to,&contact, vias, false);
			req.set_call_id(call_id_);

			std::string scseq = std::to_string(cseq);

			return con->send_message(req);
		}

		bool sip_call::trying(const sip_message& invite)
		{
			auto con = get_con();
			if (!con)
				return false;
			sip_message req = ep_->create_response(invite, nullptr,false);
			req.set_call_id(call_id_);
			req.set_status("100");
			req.set_msg("Trying");
			return con->send_message(req);
		}

		bool sip_call::ringing(const sip_message& invite)
		{
			auto con = get_con();
			if (!con)
				return false;

			sip_address contact;
			make_contact(contact);
			sip_message req = ep_->create_response(invite, &contact,true);
			my_tag_ = endpoint::create_branch();
			sip_address to = invite.to();
			to.set("tag", my_tag_);
			req.set_to(to);
			req.set_call_id(call_id_);
			req.set_status("180");
			req.set_msg("Ringing");
			return con->send_message(req);
		}

		bool sip_call::ack(const sip_message& msg)
		{
			auto con = get_con();
			if (!con)
				return false;
			sip_address from, to;
			from.url.username = local_alias_;
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
			vias.emplace_back(con->is_tcp(), nat_address_, local_port_, "", 0, endpoint::create_branch());
			sip_message req = ep_->create_request("ACK", url_, cseq,  from,to,&contact,vias, false);
			req.set_call_id(call_id_);
			req.set_from(msg.from());
			req.set_to(msg.to());

			got_ack = true;
			auto self = shared_from_this();

			invoke_connected();
			return con->send_message(req);
		}

		bool sip_call::options()
		{
			auto con = get_con();
			if (!con)
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

				from.url.username = local_alias_;
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
			vias.emplace_back(con->is_tcp(), nat_address_, local_port_, "", 0, endpoint::create_branch());

			sip_message req = ep_->create_request("OPTIONS", url_, cseq, from, to, &contact,vias, true);
			req.set_call_id(call_id_);

			return con->send_message(req);
		}

		void sip_call::make_contact(sip_address& contact)
		{
			auto con = get_con();
			if (direction_ == direction_t::outgoing)
			{
				contact.url.username = local_alias_;
				contact.url.host = nat_address_;
				contact.url.port = local_port_;
				if (con)
				{
					std::string ip;
					con->local_address(ip, contact.url.port);
				}
			}
			else
			{
				contact.url = url_;
				contact.display = local_alias_;
			}
			if (con && con->is_tcp()) {
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


		void sip_call::handle_timer(call_ptr call,const std::error_code& ec)
		{
			if (ec|| !active_)
				return;

			heartbeat_++;
			if (heartbeat_ >= 15)
			{
				if (got_ack) {
					options();
				}
				heartbeat_ = 0;
			}

			if (auto_keyframe_interval_ > 0)
			{
				keyframe_++;
				if (keyframe_ >= auto_keyframe_interval_) {
					require_keyframe();
					keyframe_ = 0;
				}
			}
			timer_.expires_after(std::chrono::milliseconds(1000));
			timer_.async_wait(std::bind(&sip_call::handle_timer, this, call, std::placeholders::_1));
		}

		void sip_call::handle_sipcon_close(call_ptr call, sip_connection_ptr con)
		{
			invoke_hangup(reason_code_t::hangup);
		}

		void sip_call::set_con(sip_connection_ptr con)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (con)
			{
				auto self = shared_from_this();
				con->set_close_handler(std::bind(&sip_call::handle_sipcon_close, this, self, std::placeholders::_1));
			}
			con_ = con;
		}

		sip_connection_ptr sip_call::get_con()const
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			return con_;
		}

		void sip_call::clear_con()
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (con_)
			{
				con_->set_close_handler(nullptr);
			}
			con_.reset();
		}

		uint16_t sip_call::make_bfcp_transaction_id()
		{
			uint16_t id=bfcp_transaction_id_.fetch_add(1);
			if(id==0)
				id= bfcp_transaction_id_.fetch_add(1);
			return id;
		}

	}
}

