#include "endpoint.h"
#include <sys2/util.h>
#include <sys2/string_util.h>
#include <hypertext/sip/challenge.h>
#include <hypertext/sip/credentials.h>

namespace voip
{
	namespace sip
	{
		endpoint::endpoint()
		{
		}

		endpoint::~endpoint()
		{
			stop();
		}

		std::string endpoint::local_alias()const
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			return alias_;
		}

		void endpoint::set_local_alias(const std::string& alias)
		{
			std::unique_lock<std::shared_mutex> lk(mutex_);
			alias_ = alias;
		}

		void endpoint::set_realm(const std::string& realm)
		{
			std::unique_lock<std::shared_mutex> lk(mutex_);
			realm_ = realm;
		}

		std::string endpoint::realm()const
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			return realm_;
		}

		void endpoint::set_nat_address(const std::string& nat_address)
		{
			std::unique_lock<std::shared_mutex> lk(mutex_);
			nat_address_ = nat_address;
		}

		const std::string& endpoint::nat_address()const
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			return nat_address_;
		}


		void endpoint::set_rtp_ports(litertp::port_range_ptr ports)
		{
			std::unique_lock<std::shared_mutex> lk(mutex_);
			rtp_ports_ = ports;
		}

		litertp::port_range_ptr endpoint::rtp_ports()const
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			return rtp_ports_;
		}


		void endpoint::set_max_bitrate(int v)
		{
			std::unique_lock<std::shared_mutex> lk(mutex_);
			max_bitrate_ = v;
		}

		int endpoint::max_bitrate()const
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			return max_bitrate_;
		}


		bool endpoint::start(const std::string& addr, int port, int worknum)
		{
			if (active_)
				return false;
			active_ = true;
			signal_.reset();
			server_.session_created_event.add(s_session_created, this);
			server_.session_destroyed_event.add(s_session_destroyed, this);
			server_.session_message_event.add(s_session_message, this);
			server_.set_logger(log_);
			if (!server_.start(addr,port, worknum))
			{
				if (log_)
				{
					log_->error("SIP listen failed on port {}", port_)->flush();
				}
				stop();
				return false;
			}
			port_ = port;
			timer_ = std::make_shared<std::thread>(&endpoint::run_timer, this);

			if (log_)
			{
				log_->info("SIP listen on port {}", port_)->flush();
			}
			return true;
		}

		void endpoint::stop()
		{
			active_ = false;
			signal_.notify();
			stop_gk_client();
			server_.stop();
			if (timer_ != nullptr)
			{
				timer_->join();
				timer_.reset();
			}
			clear_reg_endpoints();
			clear_calls();
		}

		bool endpoint::start_gk_client(const std::string& alias, const std::string& username, const std::string& password,
			const std::string& addr, int port, bool is_tcp)
		{
			auto gkclient = get_gkclient();
			if (gkclient)
				return true;
			gkclient = std::make_shared<gk_client>(*this);
			gkclient->set_alias(alias,username,password);
			gkclient->set_logger(log_);
			gkclient->set_nat_address(nat_address());

			gkclient->on_status.add(s_on_gk_status, this);
			if (!gkclient->start(addr, port, is_tcp))
			{
				gkclient.reset();
				return false;
			}
			set_gkclient(gkclient);
			return true;
		}

		void endpoint::stop_gk_client()
		{
			auto gkclient = get_gkclient();
			if (gkclient)
			{
				gkclient->stop();
				gkclient.reset();
			}
			set_gkclient(nullptr);
		}


		bool endpoint::is_gk_started()const
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			return gkclient_ != nullptr;
		}

		bool endpoint::gk_status()const
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			if (!gkclient_)
				return false;
			return gkclient_->status();
		}

		call_ptr endpoint::make_call(const std::string& url, bool tcp)
		{
			voip_uri vurl(url);
			vurl.scheme = "sip";
			auto sess=server_.create_session(vurl, tcp);
			if (!sess)
				return nullptr;

			std::string alias = local_alias();
			std::string nat = nat_address();
			auto rtp = rtp_ports();
			auto call = std::make_shared<sip_call>(log_,*this, sess, alias, voip::call::direction_t::outgoing, nat, port_, rtp);
			call->set_url(vurl);
			call->set_max_bitrate(max_bitrate());
			call->make_call_id();
			set_call(call);
			return call;
		}

		call_ptr endpoint::make_reg_server_call(const std::string& alias)
		{
			reg_endpoint ep;
			if (!get_reg_endpoint(alias, ep))
				return nullptr;

			std::string lalias = local_alias();
			std::string nat = nat_address();
			auto rtp = rtp_ports();

			auto call = std::make_shared<sip_call>(log_,*this, ep.session, lalias, voip::call::direction_t::outgoing, nat, port_, rtp);
			voip_uri vurl;
			vurl.username = alias;
			ep.session->get_remote_address(vurl.host, vurl.port);
			call->set_url(vurl);
			call->set_max_bitrate(max_bitrate());
			call->make_call_id();
			call->set_gkclient(true);
			set_call(call);
			return call;
		}

		call_ptr endpoint::make_reg_client_call(const std::string& alias)
		{
			auto gkclient = get_gkclient();
			if (!gkclient)
				return nullptr;
			auto sess = gkclient->get_session();
			if (!sess)
				return nullptr;

			std::string lalias = local_alias();
			std::string nat = nat_address();
			auto rtp = rtp_ports();

			auto call = std::make_shared<sip_call>(log_, *this, sess, lalias, voip::call::direction_t::outgoing, nat, port_, rtp);
			voip_uri vurl;
			vurl.username = alias;
			sess->get_remote_address(vurl.host, vurl.port);
			call->set_url(vurl);
			call->set_max_bitrate(max_bitrate());
			call->make_call_id();
			call->set_gkclient(true);
			set_call(call);
			return call;
		}

		bool endpoint::set_reg_endpoint(const reg_endpoint& ep)
		{
			std::lock_guard<std::recursive_mutex> lk(reg_endpoints_mutex_);
			auto itr = reg_endpoints_.find(ep.alias);
			if (itr != reg_endpoints_.end())
			{
				itr->second.update();
				itr->second.session = ep.session;
				itr->second.contact = ep.contact;
				return true;
			}
			else
			{
				auto r = reg_endpoints_.insert(std::make_pair(ep.alias, ep));
				return r.second;
			}
		}


		bool endpoint::get_reg_endpoint(const std::string& name, reg_endpoint& item)
		{
			std::lock_guard<std::recursive_mutex> lk(reg_endpoints_mutex_);
			auto itr = reg_endpoints_.find(name);
			if (itr == reg_endpoints_.end())
			{
				return false;
			}
			item = itr->second;
			return true;
		}

		bool endpoint::remove_reg_endpoint(const std::string& name)
		{
			std::lock_guard<std::recursive_mutex> lk(reg_endpoints_mutex_);
			size_t c = reg_endpoints_.erase(name);
			return c > 0;
		}

		void endpoint::all_reg_endpoints(std::vector<reg_endpoint>& vec)
		{
			vec.clear();
			std::lock_guard<std::recursive_mutex> lk(reg_endpoints_mutex_);
			vec.reserve(reg_endpoints_.size());
			for (auto itr = reg_endpoints_.begin(); itr != reg_endpoints_.end(); itr++)
			{
				vec.push_back(itr->second);
			}
		}

		size_t endpoint::count_reg_endpoints()
		{
			std::lock_guard<std::recursive_mutex> lk(reg_endpoints_mutex_);
			return reg_endpoints_.size();
		}

		void endpoint::clear_reg_endpoints()
		{
			std::lock_guard<std::recursive_mutex> lk(reg_endpoints_mutex_);
			reg_endpoints_.clear();
		}

		bool endpoint::set_call(std::shared_ptr<sip_call> call)
		{
			std::lock_guard<std::recursive_mutex> lk(calls_mutex_);
			calls_[call->id()] = call;
			return true;
		}

		std::shared_ptr<sip_call> endpoint::get_call(const std::string& call_id)
		{
			std::lock_guard<std::recursive_mutex> lk(calls_mutex_);
			auto itr = calls_.find(call_id);
			if (itr == calls_.end())
				return nullptr;

			return itr->second;
		}

		std::shared_ptr<sip_call> endpoint::find_call_by_session(const std::string& sess_id)
		{
			std::shared_ptr<sip_call> call;
			std::lock_guard<std::recursive_mutex> lk(calls_mutex_);
			for (auto itr = calls_.begin(); itr != calls_.end(); itr++)
			{
				if (itr->second)
				{
					if (itr->second->sess_id() == sess_id)
					{
						call = itr->second;
						break;
					}
				}
			}

			return call;
		}

		bool endpoint::remove_call(std::shared_ptr<sip_call> call)
		{
			if (call)
			{
				call->on_destroy.invoke(call);
				call->internal_stop();
			}
			{
				std::lock_guard<std::recursive_mutex> lk(calls_mutex_);
				auto itr = calls_.find(call->id());
				if (itr == calls_.end())
					return false;
				calls_.erase(itr);
			}
			
			return true;
		}

		void endpoint::all_calls(std::vector<std::shared_ptr<sip_call>>& vec)
		{
			vec.clear();
			std::lock_guard<std::recursive_mutex> lk(calls_mutex_);
			vec.reserve(calls_.size());
			for (auto itr = calls_.begin(); itr != calls_.end(); itr++)
			{
				auto ptr=std::dynamic_pointer_cast<sip_call>(itr->second);
				if (ptr) {
					vec.push_back(ptr);
				}
			}
		}

		size_t endpoint::count_calls()
		{
			std::lock_guard<std::recursive_mutex> lk(calls_mutex_);
			return calls_.size();
		}

		void endpoint::clear_calls()
		{
			std::lock_guard<std::recursive_mutex> lk(calls_mutex_);
			calls_.clear();
		}

		sip_message endpoint::create_request(const std::string& method, const voip_uri& url,
			uint32_t cseq, const sip_address& from, const sip_address& to, const sip_address* contact,
			const std::vector<sip_via>& vias, bool allow)
		{
			
			sip_message req = sip_message::create_request(method, url, cseq, vias);

			req.set_from(from);
			req.set_to(to);

			if (contact) {
				req.set_contact(*contact);
			}
			req.set_max_forwards(70);
			if (allow) {
				req.set_allow("INVITE,ACK,OPTIONS,BYE,CANCEL,SUBSCRIBE,NOTIFY,REFER,MESSAGE,INFO,PING,PRACK");
			}
			return req;
		}

		sip_message endpoint::create_response(const sip_message& request, const sip_address* contact, bool allow)
		{
			sip_message rsp = sip_message::create_response(request);
			if (contact)
			{
				rsp.set_contact(*contact);
			}
			if (allow) {
				rsp.set_allow("INVITE,ACK,OPTIONS,BYE,CANCEL,SUBSCRIBE,NOTIFY,REFER,MESSAGE,INFO,PING,PRACK");
			}
			return rsp;
		}

		std::string endpoint::create_branch()
		{
			uint64_t n = sys::util::random_number<uint64_t>(1000000000, 9999999999);
			return "z9hG4bK6" + std::to_string(n);
		}
		std::string endpoint::create_tag()
		{
			uint64_t n = sys::util::random_number<uint64_t>(1000000000, 9999999999);
			return std::to_string(n);
		}

		void endpoint::set_gkclient(std::shared_ptr<gk_client> gkclient)
		{
			std::unique_lock<std::shared_mutex> lk(mutex_);
			gkclient_ = gkclient;
		}

		std::shared_ptr<gk_client> endpoint::get_gkclient()const
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			return gkclient_;
		}

		void endpoint::s_on_gk_status(void* ctx, bool status)
		{
			endpoint* p = (endpoint*)ctx;
			p->on_gk_status.invoke(status);
		}

		void endpoint::run_timer()
		{
			sys::util::set_thread_name("sipep.timer", nullptr);
			while (active_)
			{
				signal_.wait(30000);
				std::vector<reg_endpoint> items;
				all_reg_endpoints(items);
				for (auto itr = items.begin(); itr != items.end(); itr++)
				{
					if (itr->is_timeout())
					{
						remove_reg_endpoint(itr->alias);
					}
				}

				std::vector<std::shared_ptr<sip_call>> items2;
				all_calls(items2);
				for (auto itr = items2.begin(); itr != items2.end(); itr++)
				{
					if ((*itr)->is_inactive())
					{
						remove_call(*itr);
					}
				}
			}
		}

		void endpoint::s_session_created(void* ctx, sip_session_ptr session)
		{
			//endpoint* p = (endpoint*)ctx;
		}

		void endpoint::s_session_destroyed(void* ctx, sip_session_ptr session)
		{
			endpoint* p = (endpoint*)ctx;
			auto call=p->find_call_by_session(session->id());
			if (call)
			{
				call->on_status.invoke(call,call::status_code_t::net_error);
				call->stop();
			}
		}

		void endpoint::s_session_message(void* ctx, sip_session_ptr session, const sip_message& message)
		{
			endpoint* p = (endpoint*)ctx;
			if (message.match_method("REGISTER"))
			{
				p->on_register(session, message);
			}
			if (message.match_method("INVITE"))
			{
				p->on_invite(session, message);
			}
			else if (message.match_method("MESSAGE"))
			{
				p->on_message(session, message);
			}
			else if (message.match_method("ACK"))
			{
				p->on_ack(session, message);
			}
			else if (message.match_method("INFO"))
			{
				p->on_info(session, message);
			}
			else if (message.match_method("BYE"))
			{
				p->on_bye(session, message);
			}
			else if (message.match_method("OPTIONS"))
			{
				p->on_options(session, message);
			}
			else if (message.match_method("SIP/2.0"))
			{
				p->on_response(session, message);
			}
		}

		void endpoint::on_register(sip_session_ptr session, const sip_message& message)
		{
			sip_message rsp = create_response(message, nullptr,true);
			sip_address addr;
			if (!message.from(addr))
			{
				if (!message.contact(addr))
				{
					rsp.set_status("400");
					rsp.set_message("BadRequest");
					session->send_message(rsp);
					return;
				}
			}

			int expires = 0;
			if (message.expires(expires))
			{
				if (expires == 0) //offline
				{
					reg_endpoint ep;
					if (get_reg_endpoint(addr.url.username, ep))
					{
						on_unreg_endpoint.invoke(ep);
					}
					remove_reg_endpoint(addr.url.username);
					session->send_message(rsp);
					return;
				}
			}

			std::string auth = message.authenticate();
			if (auth.empty())
			{
				challenge chal;
				chal.realm = realm();
				chal.nonce = sys::util::random_string(16);
				chal.algorithm = "MD5";

				rsp.set_status("401");
				rsp.set_message("Unauthorised");
				rsp.set_www_authenticate(chal.to_string());
				session->send_message(rsp);
				return;
			}

			credentials cred;
			if (!cred.parse(auth))
			{
				on_reg_endpoint_failed.invoke(addr.url.username);

				rsp.set_status("401");
				rsp.set_message("Unauthorized");
				session->send_message(rsp);
				return;
			}

			reg_endpoint ep;
			if (!get_reg_endpoint(addr.url.username, ep))
			{
				bool ok = true;
				on_require_password.invoke(voip::call_type_t::sip,addr.url.username, ok, ep.password);
				if (!ok)
				{
					rsp.set_status("401");
					rsp.set_message("Unauthorized");
					session->send_message(rsp);
					return;
				}
			}
			if (!ep.password.empty())
			{
				std::string digest = cred.digest("REGISTER", ep.password, nullptr, "");
				if (cred.response != digest)
				{
					on_reg_endpoint_failed.invoke(addr.url.username);
					rsp.set_status("401");
					rsp.set_message("Unauthorized");
					session->send_message(rsp);
					return;
				}
			}
			ep.alias = addr.url.username;
			ep.contact = addr;
			ep.session = session;
			ep.update();
			set_reg_endpoint(ep);
			on_reg_endpoint.invoke(ep);

			rsp.set_expires(expires);
			session->send_message(rsp);
		}

		void endpoint::on_message(sip_session_ptr session, const sip_message& message)
		{
			sip_message rsp = create_response(message, nullptr,false);
			session->send_message(rsp);
		}

		void endpoint::on_invite(sip_session_ptr session, const sip_message& message)
		{
			std::string call_id = message.call_id();
			auto call=get_call(call_id);
			if (call)
			{
				sip_message rsp = create_response(message, nullptr,false);
				rsp.set_status("488");
				rsp.set_message("Not Accept Here");
				session->send_message(rsp);
				return;
			}

			std::string alias = local_alias();
			std::string nat = nat_address();
			auto rtp = rtp_ports();
			call=std::make_shared<sip_call>(log_,*this, session, alias, voip::call::direction_t::incoming, nat, port_, rtp);
			call->set_invite(message);
			call->set_max_bitrate(max_bitrate());

			auto gkc = get_gkclient();
			if (gkc)
			{
				auto gksess = gkc->get_session();
				if (gksess)
				{
					if (session->id() == gksess->id())
					{
						call->set_gkclient(true);
					}
				}
			}
			set_call(call);
			call->start(incoming_audio_, incoming_video_);
		}

		void endpoint::on_ack(sip_session_ptr session, const sip_message& message)
		{
			std::string call_id = message.call_id();
			auto call = get_call(call_id);
			if (call)
			{
				call->on_ack(message);
			}
		}

		void endpoint::on_info(sip_session_ptr session, const sip_message& message)
		{
			std::string call_id = message.call_id();
			auto call = get_call(call_id);
			if (call)
			{
				call->on_info(message);
			}
		}

		void endpoint::on_bye(sip_session_ptr session, const sip_message& message)
		{
			std::string call_id = message.call_id();
			auto call = get_call(call_id);
			if (call)
			{
				call->on_bye(message);
			}
		}

		void endpoint::on_options(sip_session_ptr session, const sip_message& message)
		{
			std::string call_id = message.call_id();
			auto call = get_call(call_id);
			if (call)
			{
				call->on_options(message);
			}
		}

		void endpoint::on_response(sip_session_ptr session, const sip_message& message)
		{
			std::string call_id = message.call_id();

			auto call = get_call(call_id);
			if (call)
			{
				std::string status = message.status();

				if (status == "100")
				{
					call->on_trying(message);
				}
				else if (status == "180")
				{
					call->on_ringing(message);
				}
				else if (status == "603")
				{
					call->on_decline(message);
				}
				else
				{
					call->on_response(message);
				}
			}
			else
			{
				std::string method;
				uint32_t cseq = 0;
				if (message.get_cseq(cseq, method))
				{
					if (strcasecmp(method.c_str(), "REGISTER") == 0)
					{
						auto gkclient = get_gkclient();
						if (gkclient)
						{
							gkclient->on_register_rsp(session, message);
						}
					}
				}
			}


		}
	}
}


