#include "gk_client.h"
#include "endpoint.h"
#include <sys2/util.h>

namespace voip
{
	namespace sip
	{
		gk_client::gk_client(endpoint_ptr ep)
			: ep_(ep)
			, timer_(ep_->io_context())
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
			auto self = shared_from_this();
			timer_.expires_after(std::chrono::milliseconds(0));
			timer_.async_wait(std::bind(&gk_client::handle_timer, this, self, std::placeholders::_1));
			return true;
		}

		void gk_client::stop()
		{
			active_ = false;
			std::error_code ec;
			timer_.cancel(ec);

			send_unregister();

			auto con = get_con();
			if (con)
			{
				con->stop();
			}
			set_con(nullptr);
		}

		uint32_t gk_client::get_cseq()
		{
			uint32_t cseq = 0;
			while (cseq == 0) {
				cseq = cseq_base_.fetch_add(1);
			}
			return cseq;
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
				if (status_handler_)
				{
					auto self = shared_from_this();
					status_handler_(self, is_registered_);
				}
			}
		}


		void gk_client::set_credentials(const credentials& cred)
		{
			std::lock_guard<std::mutex> lk(mutex_);
			cred_ = cred;
			has_cred_ = true;
		}

		bool gk_client::get_credentials(credentials& cred)
		{
			std::lock_guard<std::mutex> lk(mutex_);
			cred = cred_;
			return has_cred_;
		}

		void gk_client::clear_credentials()
		{
			std::lock_guard<std::mutex> lk(mutex_);
			has_cred_ = false;
		}

		void gk_client::set_con(sip_connection_ptr con)
		{
			std::lock_guard<std::mutex> lk(mutex_);
			con_ = con;
		}
		sip_connection_ptr gk_client::get_con()const
		{
			std::lock_guard<std::mutex> lk(mutex_);
			return con_;
		}

		void gk_client::handle_timer(gk_client_ptr gk, const std::error_code& ec)
		{
			if (ec||!active_)
				return;

			time_t now = time(nullptr);
			if (now - updated_at_ >= time_to_live_ + 10)
			{
				set_status(false);
			}

			auto con = get_con();
			if (!con || con->is_timeout())
			{
				con = ep_->server_->create_connection(url_, is_tcp_);
				if (con)
				{
					if (con->start()) 
					{
						my_tag_ = "";
						set_con(con);
					}
					else
					{
						con.reset();
					}
				}
				
			}

			if (con)
			{
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

			timer_.expires_after(std::chrono::milliseconds(interval_));
			timer_.async_wait(std::bind(&gk_client::handle_timer, this, gk, std::placeholders::_1));
		}

		bool gk_client::send_register(const credentials* cred)
		{
			auto con = get_con();
			if (!con)
				return false;
			std::string ip;
			int port = 0;
			if (!con->local_address(ip, port))
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
			sip_message req=ep_->create_request("REGISTER", url_ ,cseq, from, to, &contact, vias, true);
			req.set_expires(time_to_live_);
			if (cred)
			{
				req.set_authenticate(cred->to_string());
			}

			return con->send_message(req);
		}

		bool gk_client::send_unregister()
		{
			auto con = get_con();
			if (!con)
				return false;
			std::string ip;
			int port = 0;
			if (!con->local_address(ip, port))
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

			sip_message req = ep_->create_request("REGISTER", url_, cseq,from, to, &contact,vias, true);
			req.set_expires(0);

			credentials cred;
			if (get_credentials(cred))
			{
				req.set_authenticate(cred.to_string());
			}
			return con->send_message(req);
		}


		void gk_client::on_register_rsp(sip_connection_ptr con, const sip_message& msg)
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
					
					auto self = shared_from_this();
					timer_.expires_after(std::chrono::milliseconds(0));
					timer_.async_wait(std::bind(&gk_client::handle_timer, this, self, std::placeholders::_1));
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


