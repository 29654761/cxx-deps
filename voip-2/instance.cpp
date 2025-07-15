#include "instance.h"
#include "sip/registrar.h"
#include "h323/registrar.h"
#include <sys2/security/base64.hpp>
#include <sys2/util.h>
#include <opal.h>

namespace voip
{

	instance::instance()
	{
		
	}

	instance::~instance()
	{
		cleanup();
	}

	void instance::set_nat_address(const PString& address)
	{
		if (address.IsEmpty())
			return;
		std::unique_lock<std::shared_mutex> lk(mutex_);
		nat_address_ = address;
		if (mgr_)
		{
			mgr_->set_nat_address(nat_address_);
		}
	}

	PString instance::get_nat_address()const
	{
		std::shared_lock<std::shared_mutex> lk(mutex_);
		return nat_address_;
	}


	bool instance::init(voip::opal_event* e)
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		if (process_ || mgr_ || localep_)
			return false;

		event_ = e;
		process_ = std::make_shared<process>();
		char* argv[] = { "" };
		process_->PreInitialise(1, argv);


		mgr_ = std::make_shared<manager>(event_);
		if (!mgr_->start())
		{
			mgr_.reset();
			return false;
		}

		if (!nat_address_.IsEmpty())
		{
			mgr_->set_nat_address(nat_address_);
		}

		
		//mgr_->SetTranslationAddress("192.168.3.158");
		//mgr_->SetMediaQoS(OpalMediaType::Audio(), args.GetOptionString("aud-qos"));
		//mgr_->SetMediaQoS(OpalMediaType::Video(), args.GetOptionString("vid-qos"));

		//mgr_->SetMediaTypeOfService(255);
		//mgr_->SetAudioJitterDelay();
		//PTrace::SetLevel(4);


		localep_ = new local::endpoint(*mgr_);
		localep_->SetDeferredAnswer(true);
		localep_->SetDefaultAudioSynchronicity(OpalLocalEndPoint::Synchronicity::e_Asynchronous);
		localep_->SetDefaultVideoSourceSynchronicity(OpalLocalEndPoint::Synchronicity::e_Asynchronous);


		return true;
	}

	void instance::cleanup()
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);

		if (h323ep_)
		{
			h323ep_->ClearAllCalls();
			h323ep_->RemoveGatekeeper();
			h323ep_ = nullptr;
		}
		if (sipep_)
		{
			sipep_->ClearAllCalls();
			sipep_ = nullptr;
		}
		localep_ = nullptr;

		gk_.reset();



		if (mgr_) 
		{
			mgr_->ShutDownEndpoints();
			mgr_->stop();
			mgr_.reset();
		}
		process_.reset();
	}

	bool instance::set_tcp_port(int start, int end)
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		if (!mgr_)
			return false;

		mgr_->SetTCPPorts(start, end);
		return true;
	}

	bool instance::set_udp_port(int start, int end)
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		if (!mgr_)
			return false;

		mgr_->SetUDPPorts(start, end);
		return true;
	}

	bool instance::set_rtp_port(int start, int end)
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		if (!mgr_)
			return false;

		mgr_->SetRtpIpPorts(start, end);
		return true;
	}

	bool instance::enable_h323(const PStringArray& aliases, const PStringArray& listeners)
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		if (!mgr_||h323ep_||aliases.GetSize() == 0)
		{
			return false;
		}

		h323ep_ = new h323::endpoint(*mgr_);
		
		h323ep_->DisableFastStart(true);
		h323ep_->DisableH245Tunneling(true);
		h323ep_->ForceSymmetricTCS(true);

		h323ep_->SetAliasNames(aliases);
		h323ep_->SetDefaultLocalPartyName(aliases[0]);
		h323ep_->SetInitialBandwidth(OpalBandwidth::Rx, 768 * 1024);
		h323ep_->SetInitialBandwidth(OpalBandwidth::Tx, 768 * 1024);
		h323ep_->SetTerminalType(H323EndPoint::TerminalTypes::e_TerminalOnly);
		if (!h323ep_->StartListeners(listeners))
		{
			mgr_->DetachEndPoint(h323ep_);
			h323ep_=nullptr;
			return false;
		}

		return true;
	}

	void instance::disable_h323()
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		if (h323ep_)
		{
			if (mgr_)
			{
				mgr_->DetachEndPoint(h323ep_);
			}
			h323ep_=nullptr;
		}
	}

	bool instance::enable_h323_gk_client(const PString& host, const PString& username,const PString& password)
	{
		std::shared_lock<std::shared_mutex> lk(mutex_);
		if (!mgr_||!h323ep_)
		{
			return false;
		}

		h323ep_->SetGatekeeperTimeToLive(PTimeInterval(30000));
		h323ep_->SetGatekeeperPassword(password, username);
		return h323ep_->UseGatekeeper(host,PString::Empty());
	}

	void instance::disable_h323_gk_client()
	{
		std::shared_lock<std::shared_mutex> lk(mutex_);
		if (h323ep_)
		{
			h323ep_->RemoveGatekeeper();
		}
	}

	bool instance::is_h323_registered()
	{
		std::shared_lock<std::shared_mutex> lk(mutex_);
		if (!h323ep_)
			return false;
		return h323ep_->IsRegisteredWithGatekeeper();
	}

	bool instance::enable_h323_gk_server(const PStringArray& listeners)
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		if (!h323ep_||gk_)
		{
			return false;
		}

		gk_ = std::make_shared<h323::gatekeeper>(*h323ep_);
		gk_->SetTimeToLive(30);  //Ask client for heartbeat interval
		gk_->SetGatekeeperRouted(true);
		gk_->SetGatekeeperIdentifier("H323GK");
		gk_->SetAliasCanBeHostName(false);
		//gk_->SetUsersPassword("test", "test1234");
		//gk_->SetRequiredH235(true);

		if (!gk_->AddListeners(listeners))
		{
			gk_.reset();
			return false;
		}

		return true;
	}

	void instance::disable_h323_gk_server()
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		gk_.reset();
	}


	bool instance::enable_sip(const PString& alias, const PStringArray& listeners, const PString& realm)
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		if (!mgr_ || sipep_)
		{
			return false;
		}

		sipep_ = new sip::endpoint(*mgr_);
		sipep_->SetRealm(realm);
		sipep_->SetDefaultLocalPartyName(alias);
		sipep_->SetRegistrarTimeToLive(PTimeInterval::Seconds(30));
		sipep_->SetInitialBandwidth(OpalBandwidth::Rx, 768 * 1024);
		sipep_->SetInitialBandwidth(OpalBandwidth::Tx, 768 * 1024);
		//sipep_->SetKeepAlive(PTimeInterval::Seconds(30), SIPEndPoint::KeepAliveByOPTION);
		if (!sipep_->StartListeners(listeners))
		{
			mgr_->DetachEndPoint(sipep_);
			sipep_ = nullptr;
			return false;
		}
		return true;
	}

	void instance::disable_sip()
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		if (sipep_)
		{
			if (mgr_)
			{
				mgr_->DetachEndPoint(sipep_);
			}
			sipep_=nullptr;
		}
	}

	PString instance::add_sip_register(const PString& host,const PString& alias,const PString& authname, const PString& password, const PString& authrealm, int interval)
	{
		std::shared_lock<std::shared_mutex> lk(mutex_);
		if (!sipep_ )
		{
			return PString::Empty();
		}

		SIPURL url;
		if (!url.Parse(host))
		{
			return false;
		}
		url.SetParamVar("transport", "tcp");

		SIPRegister::Params params;
		params.m_addressOfRecord = alias;
		params.m_registrarAddress = url;


		SIPURL contact;
		contact.SetScheme("sip");
		contact.SetUserName(alias);
		contact.SetHostName(host);
		params.m_contactAddress = contact.AsQuotedString();
		params.m_authID = authname;
		params.m_password = password;
		params.m_realm = authrealm;
		params.m_expire = 0;
		params.m_minRetryTime = PTimeInterval(10000);
		params.m_maxRetryTime = PTimeInterval(10000);
		params.m_restoreTime= interval;
		params.m_retryForbidden = true;
		
		PString aor;
		if (!sipep_->Register(params, aor, true))
		{
			return PString::Empty();
		}
		return aor;
	}

	bool instance::remove_sip_register(const PString& aor)
	{
		std::shared_lock<std::shared_mutex> lk(mutex_);
		if (!sipep_)
		{
			return false;
		}

		return sipep_->Unregister(aor);
	}

	void instance::remove_all_sip_registers()
	{
		std::shared_lock<std::shared_mutex> lk(mutex_);
		if (sipep_ )
		{
			sipep_->UnregisterAll();
		}
	}

	bool instance::is_sip_registered(const PString& aor)
	{
		std::shared_lock<std::shared_mutex> lk(mutex_);
		if (!sipep_)
			return false;
		return sipep_->IsRegistered(aor);
	}


	int instance::call(const PString& url, PSafePtr<voip::call>& call, const PString& audio, const PString& video)
	{
		OpalConnection::StringOptions stringOptions;
		unsigned int options = OpalConnection::SynchronousSetUp;
		PString autoStart;
		if (!audio.IsEmpty())
		{
			autoStart += "audio:" + audio+"\n";
		}
		if (!video.IsEmpty())
		{
			autoStart += "video:" + video + "\n";
		}
		stringOptions.Set(OPAL_OPT_AUTO_START, autoStart);

		std::shared_lock<std::shared_mutex> lk(mutex_);
		if (!mgr_)
		{
			return -1;
		}


		PURL purl;
		if (!purl.Parse(url))
		{
			return -2;
		}

		PString hostName = purl.GetHostName();
		if (!hostName.IsEmpty())
		{
			/*
			OpalConnection::StringOptions opts;
			if (!nat_address_.IsEmpty())
			{
				opts.SetAt(OPAL_OPT_INTERFACE, nat_address_);
			}
			*/
			call = PSafePtrCast<OpalCall,voip::call>(mgr_->SetUpCall("local:*", purl,nullptr, options,&stringOptions));
			return 0;
		}

		
		PString alias = purl.GetUserName();
		if (alias.IsEmpty())
		{
			return -2;
		}

		OpalEndPoint* ep = mgr_->FindEndPoint(purl.GetScheme());
		if (!ep)
		{
			return -1;
		}

		sip::endpoint* sip = dynamic_cast<sip::endpoint*>(ep);
		if (sip)
		{
			PSafePtr<SIPEndPoint::RegistrarAoR> aor;
			auto aors = sip->GetRegistrarAoRs();
			for (SIPURLList::const_iterator itr = aors.begin(); itr != aors.end(); itr++)
			{
				if (itr->GetUserName() == purl.GetUserName())
				{
					aor = sip->FindRegistrarAoR(*itr);
					break;
				}
			}
			if (!aor)
			{
				return -3;
			}
			sip::registrar* sip_aor = aor.GetObjectAs<sip::registrar>();
			if (!sip_aor)
			{
				printf("call sip error. url=%s\n", url.c_str());
				return -3;
			}
			PString regurl = sip_aor->remote_url();
			printf("call sip url=%s,reg=%s\n", url.c_str(), regurl.c_str());
			/*
			OpalConnection::StringOptions opts;
			if (!nat_address_.IsEmpty())
			{
				opts.SetAt(OPAL_OPT_INTERFACE, nat_address_);
			}
			*/
			call = PSafePtrCast<OpalCall, voip::call>(mgr_->SetUpCall("local:*", regurl,nullptr, options, &stringOptions));
			return 0;
		}

		h323::endpoint* h323 = dynamic_cast<h323::endpoint*>(ep);
		if (h323)
		{
			if (!gk_) {
				printf("Call h323 reg failed: gk not inited\n");
				return -1;
			}
			auto ep = gk_->FindEndPointByAliasString(purl.GetUserName());
			if (!ep)
			{
				printf("Call h323 reg failed: %s not online\n",purl.GetUserName().c_str());
				return -3;
			}

			H225_ServiceControlDescriptor desc(H225_ServiceControlDescriptor::e_callCreditServiceControl);
			desc.CreateObject();
			H225_CallCreditServiceControl& sc = (H225_CallCreditServiceControl&)desc.GetObject();

			H323CallCreditServiceControl credit(desc);
			auto* reg = ep.GetObjectAs<h323::registrar>();
			if (!reg) 
			{
				printf("Call h323 reg failed2: %s not online\n", purl.GetUserName().c_str());
				return -3;
			}
			h323::gatekeeper_listener* chan = reg->ras_channel();
			if (!chan)
			{
				printf("Call h323 reg failed: ras channel not found, name=%s\n", purl.GetUserName().c_str());
				return -1;
			}
			
			call = PSafePtrCast<OpalCall,voip::call>(mgr_->InternalCreateCall());
			if (call == NULL) {
				printf("Call h323 reg failed: Create call failed, name=%s\n", purl.GetUserName().c_str());
				return -1;
			}

			call->SetPartyB(purl);		
			
			auto localep = mgr_->FindEndPointAs<local::endpoint>("local");
			PSafePtr<OpalConnection> connection = localep->MakeConnection(*call, "local:*", nullptr,0, &stringOptions);
			
			PGloballyUniqueID callId = (PString)sys::util::uuid();
			PGloballyUniqueID confId = (PString)sys::util::uuid();

			if (!mgr_->get_calling_map().add((PString)callId, PSafePtrCast<voip::call, OpalCall>(call)))
			{
				call->Clear();
				call.SetNULL();
				printf("Call h323 reg failed: call id has existed, name=%s\n", purl.GetUserName().c_str());
				return -4;
			}

			if (!chan->ServiceControlIndication(*ep, credit, &callId, &confId))
			{
				call->Clear();
				call.SetNULL();
				printf("Call h323 reg failed: send service control failed, name=%s\n", purl.GetUserName().c_str());
				return -1;
			}
			printf("Call h323 reg ok, name=%s\n", purl.GetUserName().c_str());
			return 0;
		}

		return -1;
	}

	bool instance::clear_call(const PString& token,OpalConnection::CallEndReason reason)
	{
		std::shared_lock<std::shared_mutex> lk(mutex_);
		if (!mgr_)
		{
			return false;
		}
		return mgr_->ClearCall(token, reason);
	}

	PSafePtr<voip::call> instance::find_call(const PString& token)
	{
		std::shared_lock<std::shared_mutex> lk(mutex_);
		if (!mgr_)
		{
			return nullptr;
		}

		auto call=mgr_->FindCallWithLock(token,PSafeReference);
		if (!call)
		{
			return nullptr;
		}

		return PSafePtrCast<OpalCall,voip::call>(call);
	}



	void instance::clear_all_calls()
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		if (h323ep_)
		{
			h323ep_->ClearAllCalls();
		}
		if (sipep_)
		{
			sipep_->ClearAllCalls();
		}
	}


	bool instance::on_incoming_call(const PString& token, const PString& alias)
	{
		return true;
	}

	bool instance::on_outgoing_call(const PString& token, const PString& alias)
	{
		return true;
	}

	OpalConnection::AnswerCallResponse instance::on_answer_call(const PString& token, const PString& alias)
	{
		return OpalConnection::AnswerCallResponse::AnswerCallNow;
	}

	void instance::on_alerting(const PString& token)
	{
	}


	bool instance::on_require_password(const PString& alias, PString& password)
	{
		return true;
	}

	void instance::on_registration(const PString& prefix, const PString& username)
	{

	}

	void instance::on_unregistration(const PString& prefix, const PString& username)
	{

	}


	void instance::on_rtp_frame(const OpalMediaStream& mediaStream, RTP_DataFrame& frame)
	{

	}

}

