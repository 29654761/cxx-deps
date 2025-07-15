#include "endpoint.h"
#include "h323_util.h"
#include "gk_server.h"
#include <h460/h46018.h>
#include <sys2/util.h>

namespace voip
{
	namespace h323
	{

		endpoint::endpoint(asio::io_context& ioc)
			:ioc_(ioc)
			, acceptor_(ioc_)
			,timer_(ioc_)
		{
			media_crypto_suites_.AppendString(OpalMediaCryptoSuite::ClearText());

			OpalMediaCryptoSuiteFactory::KeyList_T all = OpalMediaCryptoSuiteFactory::GetKeyList();
			for (OpalMediaCryptoSuiteFactory::KeyList_T::iterator it = all.begin(); it != all.end(); ++it) {
				if (*it != OpalMediaCryptoSuite::ClearText() && OpalMediaCryptoSuiteFactory::CreateInstance(*it)->Supports("H.323"))
					media_crypto_suites_.AppendString(PCaselessString(*it));
			}

		}

		endpoint::~endpoint()
		{
		}

		PString endpoint::local_alias()const
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			return alias_;
		}

		void endpoint::set_local_alias(const PString& alias)
		{
			std::unique_lock<std::shared_mutex> lk(mutex_);
			alias_ = alias;
		}

		void endpoint::set_nat_address(const PString& nat_address)
		{
			std::unique_lock<std::shared_mutex> lk(mutex_);
			nat_address_ = nat_address;
		}

		PString endpoint::nat_address()const
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			return nat_address_;
		}

		void endpoint::set_h245_ports(litertp::port_range_ptr ports)
		{
			std::unique_lock<std::shared_mutex> lk(mutex_);
			h245_ports_ = ports;
		}

		litertp::port_range_ptr endpoint::h245_ports()const
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			return h245_ports_;
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


		bool endpoint::start(const std::string& addr, int port)
		{
			if (active_)
				return false;
			active_ = true;
			std::error_code ec;

			asio::ip::tcp::endpoint ep(asio::ip::address::from_string(addr, ec), port);
			if (ec) {
				stop();
				if (log_)
				{
					log_->error("H.323 start failed: create socket failed.")->flush();
				}
				return false;
			}

			acceptor_.open(ep.protocol(), ec);
			if (ec) {
				stop();
				if (log_)
				{
					log_->error("H.323 start failed: acceptor failed {}",ec.message())->flush();
				}
				return false;
			}
			acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
			
			acceptor_.bind(ep, ec);
			if (ec) {
				stop();
				if (log_)
				{
					log_->error("H.323 start failed: create socket failed.")->flush();
				}
				return false;
			}
			acceptor_.listen(asio::socket_base::max_listen_connections, ec);
			if (ec) {
				stop();
				if (log_)
				{
					log_->error("H.323 start failed: listen failed, port={}.", port)->flush();
				}
				return false;
			}

			port_ = port;

			auto self = shared_from_this();

			acceptor_.async_accept(std::bind(&endpoint::handle_accept, this, self, std::placeholders::_1, std::placeholders::_2));
			
			timer_.expires_after(std::chrono::seconds(30));
			timer_.async_wait(std::bind(&endpoint::handle_timer, this, self, std::placeholders::_1));

			if (log_)
			{
				log_->info("H.323 listen on port {}", port_)->flush();
			}
			return true;
		}

		void endpoint::stop()
		{
			active_ = false;
			std::error_code ec;
			stop_gk_client();
			acceptor_.cancel(ec);
			acceptor_.close(ec);
			clear_calls();
			timer_.cancel(ec);
		}

		bool endpoint::start_gk_client(const PString& ras_address, WORD ras_port, const PString& alias, const PString& username, const PString& password)
		{
			auto gkclient = get_gkclient();
			if (gkclient)
				return true;
			auto self = shared_from_this();
			gkclient = std::make_shared<gk_client>(self);
			gkclient->set_nat_address(nat_address());
			gkclient->set_alias(alias, username, password);
			gkclient->set_logger(log_);
			gkclient->set_incoming_call_event(std::bind(&endpoint::handle_gk_incoming_call, this, self, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
			gkclient->set_status_event(std::bind(&endpoint::handle_gk_status, this, self, std::placeholders::_1, std::placeholders::_2));
			if (!gkclient->start(ras_address, ras_port))
			{
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
			set_gkclient(gkclient);
		}

		bool endpoint::is_gk_started()const
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			return gkclient_!=nullptr;
		}

		bool endpoint::gk_status()const
		{
			std::shared_lock<std::shared_mutex> lk(mutex_);
			if (!gkclient_)
				return false;
			return gkclient_->status();
		}

		call_ptr endpoint::make_call(const std::string& url, const std::string& local_alias, const std::string& call_id, const std::string& conf_id, int call_ref)
		{
			voip_uri purl;
			purl.parse(url);
			purl.scheme = "h323";
			int port = 1720;
			if (purl.port!=0)
			{
				port = purl.port;
			}

			std::error_code ec;
			asio::ip::tcp::endpoint ep(asio::ip::address::from_string(purl.host, ec), port);
			if (ec)
			{
				if (log_)
				{
					log_->error("H.323 call error: create socket error. url={}", url)->flush();
				}
				return nullptr;
			}

			auto self = shared_from_this();

			PString alias = local_alias;
			if (alias.empty()) {
				alias = this->local_alias();
			}
			PString nat = nat_address();
			auto h245 = this->h245_ports();
			auto rtp = this->rtp_ports();
			auto callptr=std::make_shared<voip::h323::h323_call>(self,ep, alias,"", voip::call::direction_t::outgoing, nat, port, h245, rtp);
			callptr->set_on_incoming_call(std::bind(&endpoint::handle_incoming_call, this, self, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
			callptr->set_on_destroy(std::bind(&endpoint::handle_destroy_call,this,self,std::placeholders::_1,std::placeholders::_2));
			callptr->set_logger(log_);
			callptr->setup(purl,call_id,conf_id,call_ref);
			callptr->set_max_bitrate(max_bitrate());
			add_call(callptr);
			return callptr;
		}

		call_ptr endpoint::make_reg_server_call(const std::string& alias, gk_server_ptr gkserver)
		{
			if (!gkserver)
				return nullptr;

			std::string call_id = sys::util::uuid(sys::uuid_fmt_t::bar);
			std::string conf_id = sys::util::uuid(sys::uuid_fmt_t::bar);
			if (!echo_gks_call_.set(call_id, conf_id))
				return nullptr;
			if (!gkserver->call((const PString&)alias, nat_address(), port_, call_id, conf_id))
			{
				echo_gks_call_.remove(call_id);
				return nullptr;
			}
			call_ptr call;
			if (!echo_gks_call_.get(call_id, call, 5000))
				return nullptr;

			return call;
		}

		call_ptr endpoint::make_reg_client_call(const std::string& alias)
		{
			auto gkclient = get_gkclient();
			if (!gkclient)
				return nullptr;

			PGloballyUniqueID call_id = (PString)sys::util::uuid(sys::uuid_fmt_t::bar);
			PGloballyUniqueID conf_id = (PString)sys::util::uuid(sys::uuid_fmt_t::bar);
			int call_ref = Q931::GenerateCallReference();
			auto call = gkclient->make_call(alias, call_id, conf_id, call_ref, max_bitrate());
			if (!call)
				return nullptr;
			auto h323call = dynamic_pointer_cast<h323_call>(call);
			if (h323call)
			{
				h323call->set_gk_client(gkclient);
			}
			return call;
		}


		void endpoint::handle_timer(endpoint_ptr self, const std::error_code& ec)
		{
			if (ec||!active_)
				return;

			std::vector<std::shared_ptr<h323_call>> items;
			all_calls(items);
			for (auto itr = items.begin(); itr != items.end(); itr++)
			{
				if ((*itr)->is_inactive())
				{
					(*itr)->invoke_hangup(call::reason_code_t::timeout);
				}
			}

			timer_.expires_after(std::chrono::seconds(30));
			timer_.async_wait(std::bind(&endpoint::handle_timer, this, self, std::placeholders::_1));
		}

		void endpoint::handle_accept(endpoint_ptr self, const std::error_code& ec, asio::ip::tcp::socket socket)
		{
			if (!ec)
			{
				auto sktptr = std::make_shared<asio::ip::tcp::socket>(std::move(socket));

				PString alias = local_alias();
				PString nat = nat_address();
				auto h245 = h245_ports();
				auto rtp = rtp_ports();
				auto callptr = std::make_shared<voip::h323::h323_call>(self, sktptr, alias,"", voip::call::direction_t::incoming, nat, port_, h245, rtp);
				callptr->set_logger(log_);
				callptr->set_on_incoming_call(std::bind(&endpoint::handle_incoming_call, this, self, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
				callptr->set_on_destroy(std::bind(&endpoint::handle_destroy_call, this, self, std::placeholders::_1,std::placeholders::_2));
				callptr->set_max_bitrate(max_bitrate());
				callptr->set_on_facility_event(std::bind(&endpoint::handle_facility, this, self, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
				add_call(callptr);
				if (callptr->start(incoming_audio_, incoming_video_))
				{
					if (log_)
					{
						std::error_code ec;
						auto addr = sktptr->remote_endpoint(ec);
						std::string ip = addr.address().to_string(ec);
						int port = addr.port();
						log_->debug("H.323 accpeted. addr={}:{}", ip, port)->flush();
					}
				}
			}
			else if(ec==asio::error::operation_aborted||ec==asio::error::bad_descriptor)
			{
				return;
			}

			if (active_)
			{
				acceptor_.async_accept(std::bind(&endpoint::handle_accept, this, self, std::placeholders::_1, std::placeholders::_2));
			}
		}

		void endpoint::handle_destroy_call(endpoint_ptr self, call_ptr call, call::reason_code_t reason)
		{
			auto p=std::dynamic_pointer_cast<h323_call>(call);
			if (p)
			{
				remove_call(p,reason);
			}
		}

		void endpoint::handle_incoming_call(endpoint_ptr self, call_ptr call,const std::string& local_alias, const std::string& remote_alias,
			const std::string& remote_addr, int remote_port)
		{
			if (on_incoming_call)
			{
				on_incoming_call(call, local_alias, remote_alias, remote_addr, remote_port);
			}
		}


		void endpoint::handle_facility(endpoint_ptr self, call_ptr call, const PGloballyUniqueID& call_id, const PGloballyUniqueID& conf_id)
		{
			if (!echo_gks_call_.response((const std::string&)call_id.AsString(), call))
			{
			}
		}



		bool endpoint::add_call(std::shared_ptr<h323_call> call)
		{
			std::lock_guard<std::recursive_mutex> lk(calls_mutex_);
			for (auto itr = calls_.begin(); itr != calls_.end(); itr++)
			{
				if ((*itr) == call)
				{
					return false;
				}
			}
			calls_.push_back(call);
			return true;
		}

		std::shared_ptr<h323_call> endpoint::get_call(const std::string& call_id)
		{
			std::lock_guard<std::recursive_mutex> lk(calls_mutex_);

			for (auto itr = calls_.begin(); itr != calls_.end(); itr++)
			{
				if ((*itr)->id() == call_id)
				{
					return *itr;
				}
			}

			return nullptr;
		}


		bool endpoint::remove_call(std::shared_ptr<h323_call> call, call::reason_code_t reason)
		{
			bool ret = false;
			std::lock_guard<std::recursive_mutex> lk(calls_mutex_);
			for (auto itr = calls_.begin(); itr != calls_.end();)
			{
				if (*itr == call)
				{
					itr = calls_.erase(itr);
					ret = true;
					break;
				}
				else
				{
					itr++;
				}
			}
			return ret;
		}

		void endpoint::all_calls(std::vector<std::shared_ptr<h323_call>>& vec)
		{
			vec.clear();
			std::lock_guard<std::recursive_mutex> lk(calls_mutex_);
			vec.reserve(calls_.size());
			for (auto itr = calls_.begin(); itr != calls_.end(); itr++)
			{
				vec.push_back(*itr);
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

		void endpoint::handle_gk_status(endpoint_ptr self, gk_client_ptr gk, bool status)
		{
			if (on_gk_status)
			{
				on_gk_status(self,status);
			}
		}

		void endpoint::handle_gk_incoming_call(endpoint_ptr self, gk_client_ptr gk, const H225_ServiceControlIndication& body, H225_ServiceControlResponse_result::Choices& result)
		{
			H225_GloballyUniqueID callId, confId;
			bool hasCallid = false;
			
			if (body.HasOptionalField(H225_ServiceControlIndication::e_callSpecific))
			{
				callId = body.m_callSpecific.m_callIdentifier.m_guid;
				confId = body.m_callSpecific.m_conferenceID;
				hasCallid = true;
			}
			

			if (!body.HasOptionalField(H225_ServiceControlIndication::e_genericData)|| body.m_genericData.GetSize()==0)
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (log_)
				{
					log_->error("H.323 ServiceControlIndication error: need genericData.")->flush();
				}
				return;
			}

			const H225_GenericData& gen = (const H225_GenericData&)body.m_genericData[0];
			if (!gen.HasOptionalField(H225_GenericData::e_parameters)|| gen.m_parameters.GetSize()==0)
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (log_)
				{
					log_->error("H.323 ServiceControlIndication error: need genericData parameeters.")->flush();
				}
				return;
			}
			const H225_EnumeratedParameter& gen_pm = (const H225_EnumeratedParameter&)gen.m_parameters[0];
			if (gen_pm.m_id.GetTag() != H225_GenericIdentifier::e_standard)
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (log_)
				{
					log_->error("H.323 ServiceControlIndication error: Unknown genericData parameeters id.")->flush();
				}
				return;
			}
			if (((const PASN_Integer&)gen_pm.m_id) != 1)
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (log_)
				{
					log_->error("H.323 ServiceControlIndication error: unknown genericData parameeters id.")->flush();
				}
				return;
			}
			if (!gen_pm.HasOptionalField(H225_EnumeratedParameter::e_content))
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (log_)
				{
					log_->error("H.323 ServiceControlIndication error: need genericData parameeters content.")->flush();
				}
				return;
			}
			if (gen_pm.m_content.GetTag() != H225_Content::e_raw)
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (log_)
				{
					log_->error("H.323 ServiceControlIndication error: unknown genericData parameeters content.")->flush();
				}
				return;
			}
			H46018_IncomingCallIndication incall;
			if (!((const PASN_OctetString&)gen_pm.m_content).DecodeSubType(incall))
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (log_)
				{
					log_->error("H.323 ServiceControlIndication error: decode incomingCallIndication failed.")->flush();
				}
				return;
			}

			PIPSocket::Address sig_address;
			WORD sig_port = 0;
			if (!h323_util::h225_transport_address_get(incall.m_callSignallingAddress, sig_address, sig_port))
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (log_)
				{
					log_->error("H.323 ServiceControlIndication error: get incomingCallIndication signal address failed.")->flush();
				}
				return;
			}

			PString sigaddr = sig_address.AsString();

			if (!hasCallid){
				callId = incall.m_callID.m_guid;
				hasCallid = true;
			}

			std::error_code ec;
			asio::ip::tcp::endpoint addr(asio::ip::address::from_string((const std::string&)sigaddr, ec), sig_port);
			if (ec)
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (log_)
				{
					log_->error("H.323 call error: invalid signal addr. dst={}:{}", sigaddr.c_str(), sig_port)->flush();
				}
				return;
			}

			tcp_socket_ptr skt = std::make_shared<asio::ip::tcp::socket>(ioc_);
			skt->open(addr.protocol(), ec);
			if (ec)
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (log_)
				{
					log_->error("H.323 call error: open socket error. dst={}:{}", sigaddr.c_str(), sig_port)->flush();
				}
				return;
			}

			skt->connect(addr, ec);
			if (ec)
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (log_)
				{
					log_->error("H.323 call error: socket connect error. dst={}:{}", sigaddr.c_str(), sig_port)->flush();
				}
				return;
			}

			PString alias = gk->alias();
			PString nat = nat_address();
			auto h245 = h245_ports();
			auto rtp = rtp_ports();
			auto callptr = std::make_shared<voip::h323::h323_call>(self,skt, (const std::string&)alias,"", voip::call::direction_t::outgoing, (const std::string&)nat, port_, h245, rtp);
			callptr->set_max_bitrate(max_bitrate());
			callptr->set_logger(log_);
			callptr->set_on_incoming_call(std::bind(&endpoint::handle_incoming_call, this, self, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
			callptr->set_on_destroy(std::bind(&endpoint::handle_destroy_call, this, self, std::placeholders::_1,std::placeholders::_2));
			voip_uri url;
			url.scheme = "h323";
			url.host = (const std::string&)sigaddr;
			url.port = sig_port;
			auto gkclient = get_gkclient();
			callptr->set_gk_client(gkclient);
			callptr->facility(url, callId,confId);
			add_call(callptr);
			callptr->start(incoming_audio_, incoming_video_);
		}


	}
}


