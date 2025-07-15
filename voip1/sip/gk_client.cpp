#include "gk_client.h"
#include "endpoint.h"
#include <sys2/util.h>

namespace voip
{
	namespace sip
	{
		gk_client::gk_client(endpoint& ep)
			: ep_(ep)
			, updated_at_(0)
			, interval_(10000)
			, cseq_base_(1)
		{
		}

		gk_client::~gk_client()
		{
		}

		bool gk_client::start(const std::string& addr, int port, bool is_tcp,int interval)
		{
			if (active_)
				return false;
			active_ = true;

			time_to_live_ = interval;
			is_tcp_ = is_tcp;
			url_.username = alias_;
			url_.host = addr;
			url_.port = port;
			if (is_tcp_)
			{
				url_.add("transport", "tcp");
			}
	
			signal_.notify();
			timer_ = std::make_shared<std::thread>(&gk_client::run_timer, this);
			sys::util::set_thread_name("sipgkc.timer", timer_.get());
			return true;
		}

		void gk_client::stop()
		{
			active_ = false;
			signal_.notify();
			if (timer_)
			{
				timer_->join();
				timer_.reset();
			}
		}

		uint32_t gk_client::get_cseq()
		{
			uint32_t cseq = 0;
			while (cseq == 0) {
				cseq = cseq_base_.fetch_add(1);
			}
			return cseq;
		}

		sip_session_ptr gk_client::get_session()const
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			return sip_;
		}

		void gk_client::set_session(sip_session_ptr sess)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			sip_ = sess;
		}

		void gk_client::set_status(bool status)
		{
			if (is_registered_ != status)
			{
				is_registered_ = status;
				if (status)
				{
					interval_ = time_to_live_ * 1000;
				}
				else
				{
					interval_ = 10000;
					clear_credentials();
				}
				on_status.invoke(is_registered_);
			}
		}

		void gk_client::run_timer()
		{
			while (active_)
			{
				signal_.wait(interval_);
				if (!active_)
					break;
				time_t now = time(nullptr);
				if (now - updated_at_ >= time_to_live_ + 10)
				{
					set_status(false);
				}

				auto sess = get_session();
				if (!sess || sess->is_timeout())
				{
					auto sip = ep_.server_.create_session(url_,is_tcp_);
					if (!sip)
					{
						continue;
					}
					my_tag_ = "";
					set_session(sip);
					//sip_->
				}


				credentials cred;
				if (get_credentials(cred))
				{
					send_register(&cred);
				}
				else
				{
					send_register(nullptr);
				}
			}

			send_unregister();
		}

		void gk_client::set_credentials(const credentials& cred)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			cred_ = cred;
			has_cred_ = true;
		}

		bool gk_client::get_credentials(credentials& cred)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			cred = cred_;
			return has_cred_;
		}

		void gk_client::clear_credentials()
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			has_cred_ = false;
		}

		bool gk_client::send_register(const credentials* cred)
		{
			auto sess = get_session();
			if (!sess)
				return false;
			std::string ip;
			int port = 0;
			if (!sess->get_local_address(ip, port))
				return false;

			sip_address from;
			from.url.username = alias_;
			from.url.host = nat_address_;
			from.url.port = port;

			if (!cred)
			{
				my_tag_ = "";
			}
			if (my_tag_.empty())
			{
				my_tag_ = endpoint::create_tag();
			}
			from.add("tag", my_tag_);

			sip_address to;
			to.url= url_;

			sip_address contact;
			contact.url = from.url;
			uint32_t cseq = get_cseq();

			std::vector<sip_via> vias;
			vias.emplace_back(true, nat_address_, port, "", 0, endpoint::create_branch());
			sip_message req=ep_.create_request("REGISTER", url_ ,cseq, from, to, &contact, vias, true);
			req.set_expires(time_to_live_);
			if (cred)
			{
				req.set_authenticate(cred->to_string());
			}

			return sess->send_message(req);
		}

		bool gk_client::send_unregister()
		{
			auto sess = get_session();
			if (!sess)
				return false;
			std::string ip;
			int port = 0;
			if (!sess->get_local_address(ip, port))
				return false;

			sip_address from;
			from.url.username = alias_;
			from.url.host = nat_address_;
			from.url.port = port;
			if (!my_tag_.empty())
			{
				from.add("tag", my_tag_);
			}

			sip_address to;
			to.url = url_;

			sip_address contact;
			contact.url = from.url;

			uint32_t cseq = get_cseq();

			std::vector<sip_via> vias;
			vias.emplace_back(true, nat_address_, port, "", 0, endpoint::create_branch());

			sip_message req = ep_.create_request("REGISTER", url_, cseq,from, to, &contact,vias, true);
			req.set_expires(0);

			credentials cred;
			if (get_credentials(cred))
			{
				req.set_authenticate(cred.to_string());
			}
			return sess->send_message(req);
		}


		void gk_client::on_register_rsp(sip_session_ptr sess, const sip_message& msg)
		{
			const std::string& status = msg.status();
			if (status == "401")
			{
				std::string auth=msg.www_authenticate();
				challenge chal;
				if (chal.parse(auth))
				{
					credentials cred;
					cred.algorithm = chal.algorithm;
					cred.nonce = chal.nonce;
					cred.realm = chal.realm;
					cred.username = username_;
					cred.uri = url_.to_string();
					cred.response = cred.digest("REGISTER", password_, nullptr, "");
					set_credentials(cred);
					signal_.notify();
				}
				set_status(false);
				return;
			}
			else if (status == "200")
			{
				updated_at_ = time(nullptr);
				set_status(true);
			}
			else
			{
				set_status(false);
			}
		}
	}
}


