#include "endpoint.h"
#include "h323_util.h"
#include "gk_server.h"
#include <h460/h46018.h>
#include <sys2/util.h>

namespace voip
{
	namespace h323
	{

		endpoint::endpoint()
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
			stop();
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


		bool endpoint::start(const std::string& addr, int port, int worknum)
		{
			if (active_)
				return false;
			active_ = true;

			if (!ios_.open(worknum,"h323"))
			{
				stop();
				if (log_)
				{
					log_->error("H.323 start failed: io service open failed")->flush();
				}
				return false;
			}

			SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (!sys::socket::ready(s)) {
				stop();
				if (log_)
				{
					log_->error("H.323 start failed: create socket failed.")->flush();
				}
				return false;
			}
			sys::socket::set_reuseaddr(s, true);
			sockaddr_in addrin = {};
			sys::socket::ep2addr(AF_INET, addr.c_str(), port, (sockaddr*)&addrin);
			int r = ::bind(s, (const sockaddr*)&addrin, sizeof(addrin));
			if (r < 0) {
				stop();
				sys::socket::close(s);
				if (log_)
				{
					log_->error("H.323 start failed: bind port failed, port={}.", port)->flush();
				}
				return false;
			}
			r=::listen(s, 0x7FFFFFFF);
			if (r < 0) {
				stop();
				sys::socket::close(s);
				if (log_)
				{
					log_->error("H.323 start failed: listen failed, port={}.", port)->flush();
				}
				return false;
			}

			port_ = port;
			skt_ = ios_.bind(s);
			skt_->accept_event.add(on_accept_event,this);
			skt_->post_accept();
			signal_.reset();
			timer_ = std::make_shared<std::thread>(&endpoint::run_timer, this);
			sys::util::set_thread_name("h323ep.timer", timer_.get());
			if (log_)
			{
				log_->info("H.323 listen on port {}", port_)->flush();
			}
			return true;
		}

		void endpoint::stop()
		{
			active_ = false;
			signal_.notify();
			stop_gk_client();
			if (skt_) {
				skt_->close();
			}
			clear_calls();
			ios_.close();

			if (timer_ != nullptr)
			{
				timer_->join();
				timer_.reset();
			}
		}

		bool endpoint::start_gk_client(const PString& ras_address, WORD ras_port, const PString& alias, const PString& username, const PString& password)
		{
			auto gkclient = get_gkclient();
			if (gkclient)
				return true;

			gkclient = std::make_shared<gk_client>(*this,ios_);
			gkclient->set_nat_address(nat_address());
			gkclient->set_alias(alias, username, password);
			gkclient->set_logger(log_);
			gkclient->on_incoming_call.add(s_on_gk_incoming_call, this);
			gkclient->on_status.add(s_on_gk_status, this);
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

		call_ptr endpoint::make_call(const std::string& url, const std::string& call_id, const std::string& conf_id, int call_ref)
		{
			voip_uri purl;
			purl.parse(url);
			purl.scheme = "h323";
			int port = 1720;
			if (purl.port!=0)
			{
				port = purl.port;
			}

			SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (!sys::socket::ready(s)) {
				if (log_)
				{
					log_->error("H.323 call error: create socket error. url={}",url)->flush();
				}
				return nullptr;
			}

			sockaddr_storage addr = {};
			sys::socket::ep2addr(purl.host.c_str(), port, (sockaddr*)&addr);
			int r = ::connect(s, (const sockaddr*)&addr, sizeof(addr));
			if (r<0)
			{
				sys::socket::close(s);
				if (log_)
				{
					log_->error("H.323 call error: connect error. url={}", url)->flush();
				}
				return nullptr;
			}

			auto askt=ios_.bind(s, (const sockaddr*)&addr, sizeof(addr));
			if (!askt)
			{
				sys::socket::close(s);
				if (log_)
				{
					log_->error("H.323 call error: bind ios error. url={}", url)->flush();
				}
				return nullptr;
			}
			PString alias = local_alias();
			PString nat = nat_address();
			auto h245 = this->h245_ports();
			auto rtp = this->rtp_ports();
			auto callptr=std::make_shared<voip::h323::h323_call>(*this,log_,ios_,askt, alias,voip::call::direction_t::outgoing,nat, port, h245, rtp);
			callptr->setup(purl,call_id,conf_id,call_ref);
			callptr->set_max_bitrate(max_bitrate());
			add_call(callptr);
			return callptr;
		}

		call_ptr endpoint::make_reg_server_call(const std::string& alias, gk_server& gkserver)
		{
			std::string call_id = sys::util::uuid(sys::uuid_fmt_t::bar);
			std::string conf_id = sys::util::uuid(sys::uuid_fmt_t::bar);
			if (!echo_gks_call_.set(call_id, conf_id))
				return nullptr;

			if (!gkserver.call((const PString&)alias, nat_address(), port_, call_id, conf_id))
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


		void endpoint::on_accept_event(void* ctx, sys::async_socket_ptr socket, sys::async_socket_ptr newsocket, const sockaddr* addr, socklen_t addr_size)
		{
			endpoint* p = (endpoint*)ctx;

			PString alias = p->local_alias();
			PString nat = p->nat_address();
			auto h245 = p->h245_ports();
			auto rtp = p->rtp_ports();
			auto callptr = std::make_shared<voip::h323::h323_call>(*p,p->log_,p->ios_, newsocket, alias, voip::call::direction_t::incoming, nat,p->port_, h245, rtp);
			callptr->set_max_bitrate(p->max_bitrate());
			callptr->on_facility_event.add(s_on_facility, p);
			p->add_call(callptr);
			if (callptr->start(p->incoming_audio_, p->incoming_video_))
			{
				if (p->log_)
				{
					std::string ip;
					int port = 0;
					sys::socket::addr2ep(addr, &ip, &port);
					p->log_->trace("H.323 accpeted. addr={}:{}", ip, port)->flush();
				}
			}


			socket->post_accept();
		}

		void endpoint::s_on_facility(void* ctx, call_ptr call, const PGloballyUniqueID& call_id, const PGloballyUniqueID& conf_id)
		{
			endpoint* p = (endpoint*)ctx;
			if (!p->echo_gks_call_.response((const std::string&)call_id.AsString(), call))
			{
			}
		}

		void endpoint::run_timer()
		{
			while (active_)
			{
				signal_.wait(30000);
				
				std::vector<std::shared_ptr<h323_call>> items;
				all_calls(items);
				for (auto itr = items.begin(); itr != items.end(); itr++)
				{
					if ((*itr)->is_inactive())
					{
						remove_call(*itr);
					}
				}
			}
		}

		bool endpoint::add_call(std::shared_ptr<h323_call> call)
		{
			std::lock_guard<std::recursive_mutex> lk(calls_mutex_);
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


		bool endpoint::remove_call(std::shared_ptr<h323_call> call)
		{
			if (call)
			{
				call->on_destroy.invoke(call);
				call->internal_stop();
			}
			{
				std::lock_guard<std::recursive_mutex> lk(calls_mutex_);
				for (auto itr = calls_.begin(); itr != calls_.end();)
				{
					if (*itr == call)
					{
						itr=calls_.erase(itr);
					}
					else
					{
						itr++;
					}
				}
			}
			return true;
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

		void endpoint::s_on_gk_status(void* ctx, bool status)
		{
			endpoint* p = (endpoint*)ctx;
			p->on_gk_status.invoke(status);
		}

		void endpoint::s_on_gk_incoming_call(void* ctx, const H225_ServiceControlIndication& body, H225_ServiceControlResponse_result::Choices& result)
		{
			endpoint* p = (endpoint*)ctx;

			if (!body.HasOptionalField(H225_ServiceControlIndication::e_callSpecific))
			{
				result=H225_ServiceControlResponse_result::e_failed;
				if (p->log_)
				{
					p->log_->error("H.323 ServiceControlIndication error: need callSpecific.")->flush();
				}
				return;
			}
			if (!body.HasOptionalField(H225_ServiceControlIndication::e_genericData)|| body.m_genericData.GetSize()==0)
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (p->log_)
				{
					p->log_->error("H.323 ServiceControlIndication error: need genericData.")->flush();
				}
				return;
			}

			const H225_GenericData& gen = (const H225_GenericData&)body.m_genericData[0];
			if (!gen.HasOptionalField(H225_GenericData::e_parameters)|| gen.m_parameters.GetSize()==0)
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (p->log_)
				{
					p->log_->error("H.323 ServiceControlIndication error: need genericData parameeters.")->flush();
				}
				return;
			}
			const H225_EnumeratedParameter& gen_pm = (const H225_EnumeratedParameter&)gen.m_parameters[0];
			if (gen_pm.m_id.GetTag() != H225_GenericIdentifier::e_standard)
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (p->log_)
				{
					p->log_->error("H.323 ServiceControlIndication error: Unknown genericData parameeters id.")->flush();
				}
				return;
			}
			if (((const PASN_Integer&)gen_pm.m_id) != 1)
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (p->log_)
				{
					p->log_->error("H.323 ServiceControlIndication error: unknown genericData parameeters id.")->flush();
				}
				return;
			}
			if (!gen_pm.HasOptionalField(H225_EnumeratedParameter::e_content))
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (p->log_)
				{
					p->log_->error("H.323 ServiceControlIndication error: need genericData parameeters content.")->flush();
				}
				return;
			}
			if (gen_pm.m_content.GetTag() != H225_Content::e_raw)
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (p->log_)
				{
					p->log_->error("H.323 ServiceControlIndication error: unknown genericData parameeters content.")->flush();
				}
				return;
			}
			H46018_IncomingCallIndication incall;
			if (!((const PASN_OctetString&)gen_pm.m_content).DecodeSubType(incall))
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (p->log_)
				{
					p->log_->error("H.323 ServiceControlIndication error: decode incomingCallIndication failed.")->flush();
				}
				return;
			}

			PIPSocket::Address sig_address;
			WORD sig_port = 0;
			if (!h323_util::h225_transport_address_get(incall.m_callSignallingAddress, sig_address, sig_port))
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (p->log_)
				{
					p->log_->error("H.323 ServiceControlIndication error: get incomingCallIndication signal address failed.")->flush();
				}
				return;
			}

			PString sigaddr = sig_address.AsString();

			SOCKET skt = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (!sys::socket::ready(skt)) 
			{
				result = H225_ServiceControlResponse_result::e_failed;
				if (p->log_)
				{
					p->log_->error("H.323 call error: create socket error. dst={}:{}", sigaddr.c_str(),sig_port)->flush();
				}
				return;
			}

			sockaddr_storage addr = {};
			sys::socket::ep2addr(sigaddr.c_str(), sig_port, (sockaddr*)&addr);
			int r = ::connect(skt, (const sockaddr*)&addr, sizeof(addr));
			if (r < 0)
			{
				sys::socket::close(skt);
				result = H225_ServiceControlResponse_result::e_failed;
				if (p->log_)
				{
					p->log_->error("H.323 call error: socket connect error. dst={}:{}", sigaddr.c_str(), sig_port)->flush();
				}
				return;
			}

			auto askt = p->ios_.bind(skt, (const sockaddr*)&addr, sizeof(addr));

			PString alias = p->local_alias();
			PString nat = p->nat_address();
			auto h245 = p->h245_ports();
			auto rtp = p->rtp_ports();
			auto callptr = std::make_shared<voip::h323::h323_call>(*p, p->log_, p->ios_, askt, (const std::string&)alias, voip::call::direction_t::outgoing, (const std::string&)nat, p->port_,h245, rtp);
			callptr->set_max_bitrate(p->max_bitrate());
			voip_uri url;
			url.scheme = "h323";
			url.host = (const std::string&)sigaddr;
			url.port = sig_port;
			auto gkclient = p->get_gkclient();
			callptr->set_gk_client(gkclient);
			callptr->facility(url,body.m_callSpecific.m_callIdentifier.m_guid,body.m_callSpecific.m_conferenceID);
			p->add_call(callptr);
			callptr->start(p->incoming_audio_, p->incoming_video_);
		}

	}
}


