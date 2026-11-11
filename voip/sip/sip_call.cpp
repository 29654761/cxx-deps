#include "sip_call.h"
#include "endpoint.h"
#include <sys2/util.h>
#include <sys2/string_util.h>

namespace voip
{
	namespace sip
	{
		sip_call::sip_call(endpoint_ptr ep, sip_connection_ptr con, const std::string& local_alias, const std::string& remote_alias, direction_t direction,
			const std::string& nat_address, int port, rtpx::port_range_ptr rtp_ports)
			:call(local_alias,remote_alias,direction,nat_address,port,rtp_ports,ep->io_context(),ep->logger())
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

		// 纯 SIP 的 SDP 通常没有 a=setup（DTLS 协商属性），rtpx 的校验依赖它。
		// 这里对缺失 setup 的媒体注入安全默认值：offer 注入 actpass，answer 注入 passive，
		// 与 voip 侧应答里抹掉 a=setup（stp/sdp_setup_none 不序列化）的行为配套。
		static void ensure_setup(rtpx::sdp& sdp, sdp_type_t type)
		{
			for (auto itr = sdp.medias.begin(); itr != sdp.medias.end(); itr++)
			{
				if (itr->setup != sdp_setup_none)
					continue;
				itr->setup = (type == sdp_type_offer) ? sdp_setup_actpass : sdp_setup_passive;
			}
		}

		bool sip_call::add_audio_channel()
		{
			std::string mid = audio_mid_;
			if (mid.empty())
				mid = "audio";
			auto ms = rtp_->create_media_stream(media_type_audio, mid, false, "", "", rtp_trans_mode_sendrecv, false);
			if (!ms) {
				return false;
			}
			ms->use_rtp_endpoint(use_rtp_address_);
			ms->use_rtcp_endpoint(use_rtp_address_);
			// 从全局能力集生成 send 格式（PCMA/PCMU/telephone-event）
			auto fmts = rtpx::media_capabilities::global().formats(media_type_audio);
			for (auto& fmt : fmts)
			{
				ms->add_local_format(fmt);
			}
			return fmts.size() > 0;
		}

		bool sip_call::add_video_channel()
		{
			std::string mid = video_mid_;
			if (mid.empty())
				mid = "video";
			auto ms = rtp_->create_media_stream(media_type_video, mid, false, "", "", rtp_trans_mode_sendrecv, false);
			if (!ms) {
				return false;
			}
			ms->use_rtp_endpoint(use_rtp_address_);
			ms->use_rtcp_endpoint(use_rtp_address_);
			// 从全局能力集生成 send 格式（H264/H265），带宽限制取当前配置
			auto fmts = rtpx::media_capabilities::global().formats(media_type_video);
			for (auto fmt : fmts)
			{
				fmt.fmtp.set_mbr(max_bitrate_);
				ms->add_local_format(fmt);
			}
			return fmts.size() > 0;
		}

		bool sip_call::add_extend_video_channel()
		{
			// BFCP 声明需要本地端口，先绑定；失败则不建辅流，避免 offer 出现无 BFCP 的孤行
			if (!bind_bfcp_socket())
			{
				return false;
			}

			// 辅流视频（对应 h323 的 extend_video 通道）：mid 固定为 extend_video，
			// call::send_video_frame(is_extend=true) 按此 mid 发送辅流
			auto ms = rtp_->create_media_stream(media_type_video, "extend_video", false, "", "", rtp_trans_mode_sendrecv, false);
			if (!ms) {
				// 端口范围不足以再建一对时释放 BFCP socket，避免孤儿绑定占用端口
				bfcp_->stop();
				rtp_->release_port(bfcp_->local_port());
				bfcp_.reset();
				return false;
			}
			ms->use_rtp_endpoint(use_rtp_address_);
			ms->use_rtcp_endpoint(use_rtp_address_);
			// RFC 4796/4574：标记本行为辅流，label 与 BFCP 行的 mstrm 对应
			ms->set_content("slides");
			ms->set_label(std::to_string(bfcp_mstrm_));
			auto fmts = rtpx::media_capabilities::global().formats(media_type_video);
			for (auto fmt : fmts)
			{
				// 辅流带宽为主视频的一半，避免挤占主流
				fmt.fmtp.set_mbr(max_bitrate_ / 2);
				ms->add_local_format(fmt);
			}

			// BFCP application 行声明（RFC 4583）：floorid:2 mstrm:<label> 与 extend_video 的 a=label 关联
			bfcp_media_ = rtpx::sdp_media();
			bfcp_media_.media_type = media_type_application;
			bfcp_media_.protos = { "UDP", "BFCP" };
			bfcp_media_.app_data_type = "*";
			bfcp_media_.mid = "bfcp";
			bfcp_media_.has_rtp_address = true;
			bfcp_media_.rtp_address = nat_address_;
			bfcp_media_.rtcp_address = nat_address_;
			bfcp_media_.rtp_port = bfcp_->local_port();
			bfcp_media_.rtcp_port = bfcp_->local_port();
			bfcp_media_.trans_mode = rtp_trans_mode_sendrecv;
			bfcp_media_.attrs.push_back(rtpx::sdp_pair("floorctrl", "c-s", ":"));
			bfcp_media_.attrs.push_back(rtpx::sdp_pair("userid", std::to_string(bfcp_user_id_), ":"));
			bfcp_media_.attrs.push_back(rtpx::sdp_pair("confid", std::to_string(bfcp_conference_id_), ":"));
			bfcp_media_.attrs.push_back(rtpx::sdp_pair("floorid",
				std::to_string(bfcp_floor_id_) + " mstrm:" + std::to_string(bfcp_mstrm_), ":"));

			if (fmts.size() == 0)
			{
				bfcp_->stop();
				rtp_->release_port(bfcp_->local_port());
				bfcp_.reset();
				return false;
			}
			return true;
		}

		void sip_call::postprocess_local_sdp(rtpx::sdp& sdp)
		{
			sdp.bundle = false;
			sdp.o = "srtc 2371158016 0 IN IP4 " + nat_address_;
			sdp.b.push_back("CT:" + std::to_string(max_bitrate_));
			for (auto itr = sdp.medias.begin(); itr != sdp.medias.end(); itr++)
			{
				if (itr->media_type == media_type_application)
				{
					// BFCP 行协商成功：改写为本端 BFCP 服务地址并补充属性（否则保持 RFC 3264 拒绝行）
					if (bfcp_negotiated_ && itr->app_data_type == "BFCP" && bfcp_)
					{
						itr->rtp_address = nat_address_;
						itr->rtcp_address = nat_address_;
						itr->rtp_port = bfcp_->local_port();
						itr->rtcp_port = bfcp_->local_port();
						itr->attrs.clear();
						itr->attrs.push_back(rtpx::sdp_pair("floorctrl", "c-s", ":"));
						itr->attrs.push_back(rtpx::sdp_pair("userid", std::to_string(bfcp_user_id_), ":"));
						itr->attrs.push_back(rtpx::sdp_pair("confid", std::to_string(bfcp_conference_id_), ":"));
						itr->attrs.push_back(rtpx::sdp_pair("floorid",
							std::to_string(bfcp_floor_id_) + " mstrm:" + std::to_string(bfcp_mstrm_), ":"));
					}
					continue;
				}
				itr->protos.erase("UDP");
				itr->rtcp_rsize = false;
				itr->setup = sdp_setup_none;
				if (itr->media_type == media_type_video)
				{
					itr->b.push_back("AS:" + std::to_string(max_bitrate_));
				}

				// rtpx 不填充本端地址，这里补上 SDP 协商必需的地址与端口
				auto ms = rtp_->get_media_stream_by_mid(itr->mid);
				if (ms)
				{
					itr->rtp_address = nat_address_;
					itr->rtcp_address = nat_address_;
					itr->rtp_port = ms->local_rtp_port();
					itr->rtcp_port = ms->is_rtcp_mux() ? ms->local_rtp_port() : ms->local_rtcp_port();
				}
			}
		}

		std::string sip_call::id()const
		{
			return call_id_;
		}

		bool sip_call::start(bool audio,bool video, bool use_rtp_address)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			bool experted = false;
			if (!active_.compare_exchange_strong(experted, true))
				return true;

			this->use_rtp_address(use_rtp_address);
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

			if (direction_ == direction_t::outgoing)
			{
				// 主叫方从能力集构建 offer；应答方的媒体流由 answer() 里的 set_remote_sdp(offer) 依据对端 offer 创建
				if (audio)
				{
					if (!add_audio_channel())
					{
						stop(call::reason_code_t::error);
						return false;
					}
				}
				if (video)
				{
					if (!add_video_channel())
					{
						stop(call::reason_code_t::error);
						return false;
					}
					// 辅流支持：offer 带第二条 video（mid=extend_video，a=content:slides）与 BFCP application 行
					if (content_support_ && !add_extend_video_channel())
					{
						content_support_ = false;
					}
				}

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
				
				std::string url = invite_.url();
				voip_uri called_uri(url);

				remote_alias_ = from.url.username;
				if (remote_alias_.empty())
				{
					remote_alias_ = contact.display;
				}

				std::string remote_addr = contact.url.host;
				int remote_port = contact.url.port;
				if (remote_port == 0)
					remote_port = 5060;

				if (on_incoming_call)
				{
					on_incoming_call(self, local_alias_, remote_alias_, called_uri.username, remote_addr, remote_port, is_gk_client_);
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

			cancel_dtmf();
			rtp_->stop();
			stop_bfcp();
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
			rtp_->require_keyframe();
			return true;
		}

		bool sip_call::require_keyframe(const std::string& mid)
		{
			// rtp_session 已支持按 mid 只对单条视频流发 PLI，避免波及其他视频流
			return rtp_->require_keyframe(mid);
		}

		bool sip_call::set_invite(const sip_message& invite)
		{
			invite_ = invite;
			call_id_ = invite_.call_id();
			invite.from().get("tag", other_tag_);
			invite.to().get("tag", my_tag_);
			url_.parse(invite_.url());

			invite_.headers.get("Record-Route", record_route_);
			return true;
		}

		bool sip_call::answer()
		{
			rtpx::sdp remote_sdp;
			if (!remote_sdp.parse(invite_.body))
			{
				auto con = get_con();
				if (con)
				{
					auto rsp = ep_->create_response(invite_, nullptr,true);
					rsp.set_status("400");
					rsp.set_msg("BadRequest");
					for(auto itr=record_route_.begin();itr!=record_route_.end();itr++)
					{
						rsp.headers.add("Record-Route", *itr);
					}
					con->send_message(rsp);
				}
				return false;
			}
			ensure_setup(remote_sdp, sdp_type_offer);
			if (!rtp_->set_remote_sdp(remote_sdp, sdp_type_offer))
			{
				if (log_)
				{
					log_->error("SIP answer set_remote_sdp failed")->flush();
				}
				return false;
			}

			// 对端 offer 携带 BFCP 行时启用辅流协商（自适应，无需本端开关）
			start_bfcp(remote_sdp);

			rtpx::sdp sdp;
			if (!rtp_->create_answer(sdp))
			{
				return false;
			}
			postprocess_local_sdp(sdp);

			auto self = shared_from_this();
			for (auto itr = sdp.medias.begin(); itr != sdp.medias.end(); itr++)
			{
				// 应用媒体（BFCP/H.224 等）已按 RFC 3264 拒绝，不向应用层回调
				if (itr->media_type == media_type_application)
				{
					continue;
				}
				if (on_open_media)
				{
					on_open_media(self, itr->media_type, itr->mid);
				}
			}

			std::string s = sdp.to_string();
			if (!invite_rsp(invite_, s))
			{
				return false;
			}

			rtp_->start();
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
			if (!bfcp_ || !bfcp_negotiated_ || bfcp_owned_ || bfcp_pending_)
			{
				return false;
			}
			uint16_t tid = make_bfcp_transaction_id();
			bfcp_pending_transaction_ = tid;
			bfcp_pending_ = true;
			// FloorRequest = 我方请求辅流令牌（对齐 h323 send_h239_presentation_request）
			bfcp_->send(bfcp::build_floor_request(bfcp_conference_id_, tid, bfcp_user_id_, bfcp_floor_id_, bfcp_user_id_));
			return true;
		}

		void sip_call::release_presentation_role()
		{
			if (!bfcp_ || !bfcp_negotiated_)
			{
				return;
			}
			if (!bfcp_owned_ && !bfcp_pending_)
			{
				return;
			}
			// FloorRelease 释放令牌/取消进行中的请求（RFC 4582 §10.2）
			if (bfcp_floor_request_id_ > 0)
			{
				uint16_t tid = make_bfcp_transaction_id();
				bfcp_->send(bfcp::build_floor_release(bfcp_conference_id_, tid, bfcp_user_id_, bfcp_floor_request_id_));
			}
			bfcp_owned_ = false;
			bfcp_pending_ = false;
			bfcp_pending_transaction_ = 0;
			// 对齐 h323 release 回调语义：(was_owner=true, active=false)
			on_bfcp_presentation_role_changed(true, false);
		}

		bool sip_call::has_presentation_role()
		{
			return bfcp_owned_;
		}

		void sip_call::on_bfcp_presentation_role_changed(bool owned, bool active)
		{
			if (on_presentation_role_changed)
			{
				auto self = shared_from_this();
				on_presentation_role_changed(self, owned, active);
			}
		}

		bool sip_call::remote_addr(std::string& ip, int& port)
		{
			auto con = this->get_con();
			if (!con)
				return false;
			return con->remote_address(ip, port);
		}

		std::string sip_call::con_id()const
		{
			auto con = get_con();
			if (!con)
				return "";
			return con->id();
		}

		void sip_call::on_re_invite(const sip_message& msg)
		{
			rtpx::sdp sdp;
			if (!sdp.parse(msg.body))
			{
				if (log_)
				{
					log_->error("Parse re-INVITE SDP failed")->flush();
				}
				return;
			}

			// 记录现有流的本地端口，重协商时保持端口不变（复用已绑定的 transport）
			std::map<std::string, std::pair<uint16_t, uint16_t>> prefer_ports;
			auto mss = rtp_->media_streams();
			for (auto itr = mss.begin(); itr != mss.end(); itr++)
			{
				prefer_ports[(*itr)->mid()] = std::make_pair((*itr)->local_rtp_port(), (*itr)->local_rtcp_port());

				if (on_close_media)
				{
					on_close_media(shared_from_this(), (*itr)->media_type(), (*itr)->mid());
				}
			}

			ensure_setup(sdp, sdp_type_offer);
			if (!rtp_->set_remote_sdp(sdp, sdp_type_offer, prefer_ports))
			{
				if (log_)
				{
					log_->error("re-INVITE SDP set_remote_sdp failed")->flush();
				}
				return;
			}

			// re-INVITE 携带/变更 BFCP 行时更新协商
			start_bfcp(sdp);

			rtpx::sdp answer;
			if (!rtp_->create_answer(answer))
			{
				return;
			}
			postprocess_local_sdp(answer);
			for (auto itr = answer.medias.begin(); itr != answer.medias.end(); itr++)
			{
				// 应用媒体（BFCP/H.224 等）已按 RFC 3264 拒绝，不向应用层回调
				if (itr->media_type == media_type_application)
				{
					continue;
				}
				if (on_open_media)
				{
					auto self = shared_from_this();
					on_open_media(self, itr->media_type, itr->mid);
				}
			}

			auto con = get_con();
			if (con)
			{
				auto rsp = ep_->create_response(msg, nullptr, false);
				for (auto itr = record_route_.begin(); itr != record_route_.end(); itr++)
				{
					rsp.headers.add("Record-Route", *itr);
				}
				rsp.set_content_type("application/sdp");
				rsp.set_body(answer.to_string());
				con->send_message(rsp);
			}
		}

		void sip_call::on_trying(const sip_message& msg)
		{

		}

		void sip_call::on_ringing(const sip_message& msg)
		{
			sip_address remote_contact;
			if (msg.contact(remote_contact))
			{
				remote_alias_ = remote_contact.url.username;
				if (remote_alias_.empty())
				{
					remote_alias_ = remote_contact.display;
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
			if (!got_ack)
			{
				got_ack = true;
				auto self = shared_from_this();
				invoke_connected();
			}
		}

		// 解析 SIP INFO 里的按键，支持两种常见格式：
		//   Content-Type: application/dtmf-relay   正文 "Signal=5\r\nDuration=160\r\n"（Duration 单位毫秒）
		//   Content-Type: application/dtmf         正文 "5"
		// Signal 一般是按键字符本身，个别设备用 RFC 4733 事件码（10=*、11=#、12-15=A-D），两种都兼容。
		static bool parse_info_dtmf(const std::string& content_type, const std::string& body, char& digit, int& duration_ms)
		{
			// 去掉 Content-Type 里的参数（如 ;charset=utf-8）
			std::string ct = content_type.substr(0, content_type.find(';'));
			sys::string_util::trim(ct);

			std::string signal;
			duration_ms = 0;
			if (sys::string_util::icasecompare(ct, "application/dtmf-relay"))
			{
				auto lines = sys::string_util::split(body, "\n");
				for (auto line : lines)
				{
					size_t pos = line.find('=');
					if (pos == std::string::npos)
						continue;
					std::string key = line.substr(0, pos);
					std::string val = line.substr(pos + 1);
					sys::string_util::trim(key);
					sys::string_util::trim(val);
					if (sys::string_util::icasecompare(key, "Signal"))
					{
						signal = val;
					}
					else if (sys::string_util::icasecompare(key, "Duration"))
					{
						duration_ms = atoi(val.c_str());
					}
				}
			}
			else if (sys::string_util::icasecompare(ct, "application/dtmf"))
			{
				signal = body;
				sys::string_util::trim(signal);
			}
			else
			{
				return false;
			}

			if (signal.size() == 1)
			{
				digit = signal[0];
				return true;
			}
			if (signal.size() == 2 && isdigit((unsigned char)signal[0]) && isdigit((unsigned char)signal[1]))
			{
				// 事件码形式 "10"~"15"
				digit = call::dtmf_event_to_digit((uint8_t)atoi(signal.c_str()));
				return digit != 0;
			}
			return false;
		}

		void sip_call::on_info(const sip_message& msg)
		{
			auto con = get_con();
			if (con)
			{
				sip_address contact;
				make_contact(contact);
				auto rsp = ep_->create_response(msg, &contact,false);
				for (auto itr = record_route_.begin(); itr != record_route_.end(); itr++)
				{
					rsp.headers.add("Record-Route", *itr);
				}
				con->send_message(rsp);
			}

			// 对端通过 SIP INFO 发送的按键（未协商 telephone-event 时常见）
			char digit = 0;
			int duration_ms = 0;
			if (parse_info_dtmf(msg.content_type(), msg.body, digit, duration_ms))
			{
				invoke_dtmf(digit, duration_ms);
			}
		}

		void sip_call::on_bye(const sip_message& msg)
		{
			got_bye = true;
			auto con = get_con();
			if (con)
			{
				auto rsp = ep_->create_response(msg, nullptr, false);
				for (auto itr = record_route_.begin(); itr != record_route_.end(); itr++)
				{
					rsp.headers.add("Record-Route", *itr);
				}
				con->send_message(rsp);
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
				for (auto itr = record_route_.begin(); itr != record_route_.end(); itr++)
				{
					rsp.headers.add("Record-Route", *itr);
				}
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
				for (auto itr = record_route_.begin(); itr != record_route_.end(); itr++)
				{
					rsp.headers.add("Record-Route", *itr);
				}
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
				
				uint32_t seq = 0;
				std::string method;
				msg.get_cseq(seq, method);
				// OPTIONS 心跳与 INFO 按键被拒（如 415/501 不支持）不影响通话，不挂断
				if (method != "OPTIONS" && method != "INFO")
				{
					if (on_hangup)
					{
						auto self = shared_from_this();
						on_hangup(self, call::reason_code_t::error);
					}
				}
				return;
			}

			if (strcasecmp(method.c_str(), "INVITE") == 0)
			{
				sip_address remote_contact;
				if (msg.contact(remote_contact))
				{
					remote_alias_ = remote_contact.url.username;
					if (remote_alias_.empty())
					{
						remote_alias_ = remote_contact.display;
					}
				}
				msg.headers.get("Record-Route", record_route_);

				rtpx::sdp sdp;
				if (sdp.parse(msg.body))
				{
					ensure_setup(sdp, sdp_type_answer);
					rtp_->set_remote_sdp(sdp, sdp_type_answer);
					// 对端 answer 的 BFCP 行携带其服务端地址，完成辅流协商
					start_bfcp(sdp);
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

				rtp_->start();
			}
			else if (strcasecmp(method.c_str(), "BYE") == 0)
			{
				got_bye = true;
			}
		}

		void sip_call::on_update(const sip_message& msg)
		{
			auto con = get_con();
			if (con)
			{
				sip_address contact;
				make_contact(contact);
				auto rsp = ep_->create_response(msg, &contact, false);
				for (auto itr = record_route_.begin(); itr != record_route_.end(); itr++)
				{
					rsp.headers.add("Record-Route", *itr);
				}
				con->send_message(rsp);
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

			// offer 的 rtpmap 来自能力集（含 telephone-event），对端应答后按交集协商
			rtpx::sdp sdp;
			if (!rtp_->create_offer(sdp))
			{
				return false;
			}
			postprocess_local_sdp(sdp);
			if (content_support_ && bfcp_media_.media_type == media_type_application)
			{
				// 辅流：追加 add_extend_video_channel 生成的 BFCP application 行
				sdp.medias.push_back(bfcp_media_);
			}
			std::string s = sdp.to_string();
			//std::stringstream ss;
			//ss << "v=0\r\n";
			//ss << "o=- 20005 20005 IN IP4 125.118.73.10\r\n";
			//ss << "s=SDP data\r\n";
			//ss << "c=IN IP4 125.118.73.10\r\n";
			//ss << "b=AS:2048\r\n";
			//ss << "t=0 0\r\n";
			//ss << "m=audio "<< sdp.medias[0].rtp_port << " RTP/AVP 8\r\n";
			//ss << "a=rtpmap:8 PCMA/8000\r\n";
			//ss << "a=sendrecv\r\n";
			//ss << "m=video " << sdp.medias[1].rtp_port << " RTP/AVP 96\r\n";
			//ss << "b=TIAS:2048000\r\n";
			//ss << "a=rtpmap:96 H264/90000\r\n";
			//ss << "a=fmtp:96 profile-level-id=428029; max-mbps=245760; max-fs=8192; packetization-mode=1\r\n";
			//ss << "a=rtcp-fb:* ccm fir\r\n";
			//ss << "a=sendrecv\r\n";
			//std::string s = ss.str();
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
			sip_message rsp = ep_->create_response(invite, &contact,true);

			sip_address to = invite.to();
			to.set("tag", my_tag_);
			rsp.set_to(to);
			rsp.set_call_id(call_id_);
			rsp.set_status("200");
			rsp.set_msg("OK");
			rsp.set_body(sdp);
			rsp.set_content_type("application/sdp");
			for (auto itr = record_route_.begin(); itr != record_route_.end(); itr++)
			{
				rsp.headers.add("Record-Route", *itr);
			}
			return con->send_message(rsp);
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
			for (auto itr = record_route_.begin(); itr != record_route_.end(); itr++)
			{
				req.headers.add("Route", *itr);
			}
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
			sip_message rsp = ep_->create_response(invite, &contact,true);
			my_tag_ = endpoint::create_branch();
			sip_address to = invite.to();
			to.set("tag", my_tag_);
			rsp.set_to(to);
			rsp.set_call_id(call_id_);
			rsp.set_status("180");
			rsp.set_msg("Ringing");
			for (auto itr = record_route_.begin(); itr != record_route_.end(); itr++)
			{
				rsp.headers.add("Record-Route", *itr);
			}
			return con->send_message(rsp);
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
			for (auto itr = record_route_.begin(); itr != record_route_.end(); itr++)
			{
				req.headers.add("Route", *itr);
			}

			got_ack = true;
			auto self = shared_from_this();

			if (msg.status() == "200") 
			{
				invoke_connected();
			}
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

		call::dtmf_method_t sip_call::dtmf_method()
		{
			// 对端 SDP 协商了 telephone-event 走 RFC 4733，否则退回 SIP INFO
			auto method = call::dtmf_method();
			return method == dtmf_method_t::rfc4733 ? method : dtmf_method_t::signaling;
		}

		bool sip_call::send_dtmf_signaling(char digit, int duration_ms)
		{
			// 与 parse_info_dtmf 对应：application/dtmf-relay，"Signal=5\r\nDuration=160\r\n"
			std::string body = std::string("Signal=") + digit + "\r\nDuration=" + std::to_string(duration_ms) + "\r\n";
			return info("application/dtmf-relay", body);
		}

		bool sip_call::info(const std::string& content_type, const std::string& body)
		{
			auto con = get_con();
			if (!con)
				return false;

			uint32_t cseq = get_cseq();

			// 对话内请求，From/To/请求地址与 bye() 一致
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

			sip_message req = ep_->create_request("INFO", url, cseq, from, to, &contact, vias, false);
			req.set_call_id(call_id_);
			for (auto itr = record_route_.begin(); itr != record_route_.end(); itr++)
			{
				req.headers.add("Route", *itr);
			}
			req.set_content_type(content_type);
			req.set_body(body);
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
			std::lock_guard<std::mutex> lk(con_mutex_);
			if (con)
			{
				auto self = shared_from_this();
				con->set_close_handler(std::bind(&sip_call::handle_sipcon_close, this, self, std::placeholders::_1));
			}
			con_ = con;
		}

		sip_connection_ptr sip_call::get_con()const
		{
			std::lock_guard<std::mutex> lk(con_mutex_);
			return con_;
		}

		void sip_call::clear_con()
		{
			std::lock_guard<std::mutex> lk(con_mutex_);
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

		const rtpx::sdp_media* sip_call::find_bfcp_media(const rtpx::sdp& sdp)const
		{
			for (auto& m : sdp.medias)
			{
				if (m.media_type == media_type_application && m.app_data_type == "BFCP")
				{
					return &m;
				}
			}
			return nullptr;
		}

		void sip_call::parse_bfcp_attributes(const rtpx::sdp_media& m)
		{
			for (auto& attr : m.attrs)
			{
				if (attr.key == "confid")
				{
					bfcp_conference_id_ = (uint32_t)strtoul(attr.val.c_str(), nullptr, 10);
				}
				else if (attr.key == "userid")
				{
					bfcp_remote_user_id_ = (uint16_t)strtoul(attr.val.c_str(), nullptr, 10);
				}
				else if (attr.key == "floorid")
				{
					// a=floorid:<floor-id> [mstrm:<stream-id>]，mstrm 与媒体行的 a=label 对应
					std::vector<std::string> ss = sys::string_util::split(attr.val, " ");
					if (!ss.empty())
					{
						bfcp_floor_id_ = (uint16_t)strtoul(ss[0].c_str(), nullptr, 10);
					}
					for (size_t i = 1; i < ss.size(); i++)
					{
						if (ss[i].find("mstrm:") == 0)
						{
							bfcp_mstrm_ = (uint16_t)strtoul(ss[i].substr(6).c_str(), nullptr, 10);
						}
					}
				}
			}
		}

		// 绑定本地 BFCP 端口并初始化默认参数（confid/userid/floorid）
		bool sip_call::bind_bfcp_socket()
		{
			if (bfcp_ && bfcp_->is_active())
			{
				return true;
			}
			if (bfcp_conference_id_ == 0)
			{
				bfcp_conference_id_ = sys::util::random_number<uint32_t>(1, 0xFFFFFFFE);
			}
			if (bfcp_user_id_ == 0)
			{
				bfcp_user_id_ = 1;
			}
			if (bfcp_floor_id_ == 0)
			{
				bfcp_floor_id_ = 2;
			}

			bfcp_ = std::make_shared<bfcp_client>(ep_->io_context());
			bfcp_->set_message_handler(std::bind(&sip_call::handle_bfcp_message, this,
				std::placeholders::_1, std::placeholders::_2));

			// 仅从配置的端口范围绑定（便于防火墙按范围放行）；未配置范围时直接失败，
			// 避免随机端口被防火墙拦截导致难以排查
			if (!rtp_ports_)
			{
				if (log_)
				{
					log_->error("BFCP: rtp port range not configured, cannot bind BFCP socket")->flush();
				}
				bfcp_.reset();
				return false;
			}
			// 从本会话端口池取空闲端口并预留：与媒体流同一游标、同一范围，绝不落到媒体端口上
			// （此前用 endpoint 共享的 port_range，其游标无人推进，BFCP 永远拿到首个媒体端口 51000）
			bool started = false;
			for (int i = 0; i < 16; i++)
			{
				uint16_t port = rtp_->allocate_udp_port();
				if (port == 0)
				{
					break;
				}
				if (bfcp_->start(log_, port))
				{
					started = true;
					break;
				}
				rtp_->release_port(port);
			}
			if (!started)
			{
				if (log_)
				{
					log_->error("BFCP: bind local port failed, rtp ports {}-{} exhausted", rtp_ports_->min_port(), rtp_ports_->max_port())->flush();
				}
				bfcp_.reset();
				return false;
			}
			bfcp_local_port_ = bfcp_->local_port();
			return true;
		}

		bool sip_call::start_bfcp(const rtpx::sdp& remote_sdp)
		{
			const rtpx::sdp_media* m = find_bfcp_media(remote_sdp);
			if (!m || m->rtp_port <= 0 || m->rtp_address.empty() || m->rtp_address == "0.0.0.0")
			{
				return false;
			}

			parse_bfcp_attributes(*m);

			// 缺省值兜底
			if (bfcp_conference_id_ == 0)
			{
				bfcp_conference_id_ = 1;
			}
			if (bfcp_remote_user_id_ == 0)
			{
				bfcp_remote_user_id_ = 1;
			}
			if (bfcp_floor_id_ == 0)
			{
				bfcp_floor_id_ = 1;
			}
			// 点对点场景：本端 user id 取与对端不同的值（对端未先绑定本端 socket 时）
			if (bfcp_user_id_ == 0)
			{
				bfcp_user_id_ = (bfcp_remote_user_id_ == 1) ? 2 : 1;
			}

			if (!bind_bfcp_socket())
			{
				return false;
			}

			bfcp_->set_remote(m->rtp_address, (uint16_t)m->rtp_port);
			bfcp_negotiated_ = true;

			if (log_)
			{
				log_->info("BFCP negotiated: local port={}, remote={}:{}, confid={}, userid={}, floorid={}",
					bfcp_local_port_, m->rtp_address, m->rtp_port, bfcp_conference_id_, bfcp_user_id_, bfcp_floor_id_)->flush();
			}
			return true;
		}

		void sip_call::stop_bfcp()
		{
			if (bfcp_)
			{
				bfcp_->stop();
				if (rtp_)
				{
					rtp_->release_port(bfcp_->local_port());
				}
			}
			bfcp_negotiated_ = false;
			bfcp_owned_ = false;
			bfcp_pending_ = false;
			bfcp_remote_owned_ = false;
		}

		void sip_call::handle_bfcp_message(const bfcp::message& msg, const asio::ip::udp::endpoint& ep)
		{
			switch (msg.primitive)
			{
			case bfcp::primitive::floor_request:
			{
				// 对端请求辅流令牌，我方作为 chair 决策（RFC 4582 §13.1）
				uint16_t floor_id = msg.floor_ids.empty() ? bfcp_floor_id_ : msg.floor_ids[0];
				if (bfcp_owned_)
				{
					// 我方持有令牌：拒绝（对齐 h323 持有时 ack=false，不回调）
					if (log_)
					{
						log_->info("BFCP: deny floor request from user={}, owned locally", msg.user_id)->flush();
					}
					bfcp_->send(bfcp::build_floor_request_status(bfcp_conference_id_, msg.transaction_id, msg.user_id,
						bfcp_remote_floor_request_id_, floor_id, bfcp::st_denied));
					return;
				}

				// 我方未持有：授予
				if (bfcp_remote_floor_request_id_ == 0)
				{
					bfcp_remote_floor_request_id_ = (uint16_t)(sys::util::random_number<uint16_t>(1, 0xFFFE));
				}
				bfcp_remote_user_id_ = msg.user_id;
				bfcp_remote_owned_ = true;
				bfcp_->send(bfcp::build_floor_request_status(bfcp_conference_id_, msg.transaction_id, msg.user_id,
					bfcp_remote_floor_request_id_, floor_id, bfcp::st_granted));
				if (log_)
				{
					log_->info("BFCP: grant floor to user={}, floor_request_id={}", msg.user_id, bfcp_remote_floor_request_id_)->flush();
				}
				// 对齐 h323 授予对端后的回调 (owned=false, active=true)
				on_bfcp_presentation_role_changed(false, true);
				break;
			}
			case bfcp::primitive::floor_release:
			{
				// 对端释放（RFC 4582 §13.4）：回 Released 状态
				bfcp_->send(bfcp::build_floor_request_status(bfcp_conference_id_, msg.transaction_id, msg.user_id,
					msg.floor_request_id, bfcp_floor_id_, bfcp::st_released));
				if (bfcp_remote_owned_)
				{
					bfcp_remote_owned_ = false;
					bfcp_remote_floor_request_id_ = 0;
					on_bfcp_presentation_role_changed(false, false);
				}
				break;
			}
			case bfcp::primitive::floor_request_status:
			{
				// 对端对我方请求/释放的响应
				uint8_t status = msg.overall_status != 0 ? msg.overall_status : msg.status;
				bool is_response = (msg.transaction_id == bfcp_pending_transaction_ && bfcp_pending_transaction_ != 0);
				bool is_notify = (msg.transaction_id == 0);

				if (bfcp_pending_ && (is_response || is_notify))
				{
					bfcp_floor_request_id_ = msg.floor_request_id;
					if (status == bfcp::st_granted)
					{
						// 令牌授予成功（对齐 h323 presentationTokenResponse(ack) + 通道打开）
						bfcp_owned_ = true;
						bfcp_pending_ = false;
						bfcp_pending_transaction_ = 0;
						if (log_)
						{
							log_->info("BFCP: floor granted, floor_request_id={}", bfcp_floor_request_id_)->flush();
						}
						on_bfcp_presentation_role_changed(true, true);
					}
					else if (status == bfcp::st_denied)
					{
						bfcp_owned_ = false;
						bfcp_pending_ = false;
						bfcp_pending_transaction_ = 0;
						if (log_)
						{
							log_->info("BFCP: floor denied")->flush();
						}
						on_bfcp_presentation_role_changed(false, false);
					}
					else if (status == bfcp::st_released || status == bfcp::st_cancelled)
					{
						// FloorRelease 的响应，本地状态已在发送时清理
						bfcp_pending_ = false;
						bfcp_pending_transaction_ = 0;
						bfcp_floor_request_id_ = 0;
					}
					else if (log_)
					{
						log_->debug("BFCP: floor request status={}, floor_request_id={}", (int)status, msg.floor_request_id)->flush();
					}
				}
				else if (is_notify && status == bfcp::st_revoked)
				{
					// 对端作为 chair 收回我方令牌（server-initiated）
					if (bfcp_owned_)
					{
						bfcp_owned_ = false;
						bfcp_floor_request_id_ = 0;
						on_bfcp_presentation_role_changed(false, false);
					}
				}
				else if (is_notify && status == bfcp::st_granted && !bfcp_owned_ && !bfcp_remote_owned_
					&& msg.beneficiary_id > 0 && msg.beneficiary_id == bfcp_remote_user_id_)
				{
					// 非应答的授予通知：部分对端 chair 本地抓流时只发 FloorRequestStatus 通知
					// （无对应请求事务），持有者须明确是对端用户才认定为对端持有
					bfcp_remote_owned_ = true;
					if (msg.floor_request_id > 0)
					{
						bfcp_remote_floor_request_id_ = msg.floor_request_id;
					}
					if (log_)
					{
						log_->info("BFCP: remote owns floor (notify), user={}", msg.beneficiary_id)->flush();
					}
					on_bfcp_presentation_role_changed(false, true);
				}
				break;
			}
			case bfcp::primitive::floor_status:
			{
				// 对端作为 floor control server 时，本地抓取/释放辅流不经过 FloorRequest，
				// 仅以 FloorStatus（RFC 4582 §9.10）推送楼层状态，需据此同步"对端持有"状态
				uint8_t status = msg.overall_status != 0 ? msg.overall_status : msg.status;
				// 持有者优先取 BENEFICIARY-ID；缺失时视为对端（我方请求的授予走 FloorRequestStatus 应答）
				bool held_by_remote = (msg.beneficiary_id == 0 || msg.beneficiary_id != bfcp_user_id_);
				if (status == bfcp::st_granted && held_by_remote && !bfcp_owned_ && !bfcp_remote_owned_)
				{
					bfcp_remote_owned_ = true;
					if (msg.beneficiary_id > 0)
					{
						bfcp_remote_user_id_ = msg.beneficiary_id;
					}
					if (msg.floor_request_id > 0)
					{
						bfcp_remote_floor_request_id_ = msg.floor_request_id;
					}
					if (log_)
					{
						log_->info("BFCP: floor status granted to remote, user={}, floor_request_id={}",
							bfcp_remote_user_id_, bfcp_remote_floor_request_id_)->flush();
					}
					// 对齐 h323 对端获取令牌后的回调 (owned=false, active=true)
					on_bfcp_presentation_role_changed(false, true);
				}
				else if (bfcp_remote_owned_ && (status == bfcp::st_released || status == bfcp::st_revoked
					|| status == bfcp::st_denied || status == bfcp::st_cancelled))
				{
					bfcp_remote_owned_ = false;
					bfcp_remote_floor_request_id_ = 0;
					if (log_)
					{
						log_->info("BFCP: floor status released by remote, status={}", (int)status)->flush();
					}
					// 对齐 h323 对端释放令牌后的回调 (owned=false, active=false)
					on_bfcp_presentation_role_changed(false, false);
				}
				break;
			}
			case bfcp::primitive::hello_ack:
			case bfcp::primitive::hello:
			case bfcp::primitive::chair_action_ack:
			case bfcp::primitive::error:
			default:
				// Hello 已由 bfcp_client 自动应答；其余暂不处理
				break;
			}
		}

	}
}

