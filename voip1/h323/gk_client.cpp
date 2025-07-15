#include "gk_client.h"
#include "h323_util.h"
#include "endpoint.h"
#include <h323/h323pdu.h>
#include <hypertext/sip/voip_uri.h>
#include <sys2/util.h>

namespace voip
{
	namespace h323
	{
		gk_client::gk_client(endpoint& ep, sys::async_service& ios)
			:ios_(ios)
			,ep_(ep)
			, request_seq_(0)
			, interval_(10000)
			, updated_at_(0)
		{
		}

		gk_client::~gk_client()
		{
		}


		bool gk_client::start(const std::string& ras_addr, int ras_port, int local_port, int interval)
		{
			if (alias_.IsEmpty())
				return false;

			if (active_)
				return true;
			active_ = true;

			ras_addr_ = ras_addr;
			ras_port_ = ras_port;
			time_to_live_ = interval;

			SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (!sys::socket::ready(s))
			{
				stop();
				return false;
			}

			sockaddr_storage addr = { 0 };
			sys::socket::ep2addr("0.0.0.0", local_port, (sockaddr*)&addr);
			if (::bind(s, (const sockaddr*)&addr, sizeof(addr)) < 0)
			{
				stop();
				return false;
			}

			socket_ = ios_.bind(s, (const sockaddr*)&addr, sizeof(addr));
			if (!socket_)
			{
				stop();
				return false;
			}

			if (local_port == 0)
			{
				std::string local_ip;
				sys::socket::local_addr(socket_->handle(), local_ip, local_port);
			}
			local_port_ = (WORD)local_port;


			sys::socket::ep2addr(ras_addr_.c_str(), ras_port, (sockaddr*)&addr_);

			socket_->read_event.add(s_on_read_event, this);
			socket_->post_read_from();

			signal_.notify();
			timer_ = std::make_shared<std::thread>(&gk_client::run_timer, this);
			sys::util::set_thread_name("gkc.timer", timer_.get());
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

			if (socket_)
			{
				socket_->close();
				socket_.reset();
			}
		}

		call_ptr gk_client::make_call(const PString& alias,
			const PGloballyUniqueID& call_id,
			const PGloballyUniqueID& conf_id,
			int call_ref, int bandwidth)
		{
			if (!is_registered_)
				return nullptr;

			H225_RasMessage rsp;
			if (!admission_request(alias, call_id, conf_id, call_ref, bandwidth,rsp))
				return nullptr;

			if (rsp.GetTag() != H225_RasMessage::e_admissionConfirm)
				return nullptr;

			H225_AdmissionConfirm& confirm = (H225_AdmissionConfirm&)rsp;
			PIPSocket::Address sig_addr;
			WORD sig_port = 0;
			if (!h323_util::h225_transport_address_get(confirm.m_destCallSignalAddress, sig_addr, sig_port))
			{
				if (log_)
				{
					log_->error("H323 AdmissionConfirm error. need destCallSignalAddress, alias={}", alias.c_str())->flush();
				}
				return nullptr;
			}


			voip_uri url;
			url.scheme = "h323";
			url.username = (const std::string&)alias;
			url.host = sig_addr.AsString().c_str();
			url.port = sig_port;
			auto call = ep_.make_call(url.to_full_string(), (const std::string&)call_id.AsString(), (const std::string&)conf_id.AsString(), call_ref);
			if (!call)
				return nullptr;
			call->start(ep_.incoming_audio_, ep_.incoming_video_);
			return call;
		}


		bool gk_client::send_service_control_response(const H225_ServiceControlIndication& request,
			H225_ServiceControlResponse_result::Choices result)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_serviceControlResponse);
			H225_ServiceControlResponse& rsp = (H225_ServiceControlResponse&)msg;
			rsp.m_requestSeqNum = request.m_requestSeqNum;

			rsp.IncludeOptionalField(H225_ServiceControlResponse::e_result);
			rsp.m_result.SetTag(result);

			H225_CryptoH323Token* token=new H225_CryptoH323Token();
			h323_util::make_md5_crypto_token(username_, password_, *token);
			rsp.IncludeOptionalField(H225_ServiceControlResponse::e_cryptoTokens);
			rsp.m_cryptoTokens.Append(token);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			return socket_->send_to((const char*)stream.GetPointer(), stream.GetSize(), (const sockaddr*)&addr_, sizeof(addr_));

		}

		bool gk_client::send_admission_request(const PString& dst_alias,
			const PGloballyUniqueID& call_id,
			const PGloballyUniqueID& conf_id,
			int call_ref, int bandwidth)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_admissionRequest);
			H225_AdmissionRequest& req = (H225_AdmissionRequest&)msg;
			req.m_requestSeqNum = request_seq_.fetch_add(1);

			req.m_callType.SetTag(H225_CallType::e_pointToPoint);
			req.m_endpointIdentifier = endpoint_identifier_;

			req.IncludeOptionalField(H225_AdmissionRequest::e_destinationInfo);
			H225_AliasAddress* dst = new H225_AliasAddress();
			H323SetAliasAddress(dst_alias,*dst);
			req.m_destinationInfo.Append(dst);

			H225_AliasAddress* src= new H225_AliasAddress();
			H323SetAliasAddress(alias_, *src);
			req.m_srcInfo.Append(src);

			req.IncludeOptionalField(H225_AdmissionRequest::e_srcCallSignalAddress);
			h323_util::h225_transport_address_set(req.m_srcCallSignalAddress, PIPSocket::Address(nat_address_), signal_port_);

			req.m_bandWidth = bandwidth;
			req.m_callReferenceValue = call_ref;

			req.m_conferenceID = conf_id;

			req.IncludeOptionalField(H225_AdmissionRequest::e_callIdentifier);
			req.m_callIdentifier.m_guid = call_id;

			req.m_activeMC = false;
			req.m_answerCall = true;
			req.IncludeOptionalField(H225_AdmissionRequest::e_canMapAlias);
			req.m_canMapAlias = true;

			req.IncludeOptionalField(H225_AdmissionRequest::e_gatekeeperIdentifier);
			req.m_gatekeeperIdentifier = gk_identifier_;

			H225_CryptoH323Token* token=new H225_CryptoH323Token();
			h323_util::make_md5_crypto_token(username_, password_, *token);
			req.IncludeOptionalField(H225_AdmissionRequest::e_cryptoTokens);
			req.m_cryptoTokens.Append(token);

			req.IncludeOptionalField(H225_AdmissionRequest::e_willSupplyUUIEs);
			req.m_willSupplyUUIEs = true;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();


			return socket_->send_to((const char*)stream.GetPointer(), stream.GetSize(), (const sockaddr*)&addr_, sizeof(addr_));
		}

		bool gk_client::admission_request(const PString& dst_alias,
			const PGloballyUniqueID& call_id,
			const PGloballyUniqueID& conf_id,
			int call_ref, int bandwidth, H225_RasMessage& rsp)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_admissionRequest);
			H225_AdmissionRequest& req = (H225_AdmissionRequest&)msg;
			req.m_requestSeqNum = request_seq_.fetch_add(1);

			req.m_callType.SetTag(H225_CallType::e_pointToPoint);
			req.m_endpointIdentifier = endpoint_identifier_;

			req.IncludeOptionalField(H225_AdmissionRequest::e_destinationInfo);
			H225_AliasAddress* dst = new H225_AliasAddress();
			H323SetAliasAddress(dst_alias, *dst);
			req.m_destinationInfo.Append(dst);

			H225_AliasAddress* src = new H225_AliasAddress();
			H323SetAliasAddress(alias_, *src);
			req.m_srcInfo.Append(src);

			req.IncludeOptionalField(H225_AdmissionRequest::e_srcCallSignalAddress);
			h323_util::h225_transport_address_set(req.m_srcCallSignalAddress, PIPSocket::Address(nat_address_), signal_port_);

			req.m_bandWidth = bandwidth;
			req.m_callReferenceValue = call_ref;

			req.m_conferenceID = conf_id;

			req.IncludeOptionalField(H225_AdmissionRequest::e_callIdentifier);
			req.m_callIdentifier.m_guid = call_id;

			req.m_activeMC = false;
			req.m_answerCall = true;
			req.IncludeOptionalField(H225_AdmissionRequest::e_canMapAlias);
			req.m_canMapAlias = true;

			req.IncludeOptionalField(H225_AdmissionRequest::e_gatekeeperIdentifier);
			req.m_gatekeeperIdentifier = gk_identifier_;

			H225_CryptoH323Token* token = new H225_CryptoH323Token();
			h323_util::make_md5_crypto_token(username_, password_, *token);
			req.IncludeOptionalField(H225_AdmissionRequest::e_cryptoTokens);
			req.m_cryptoTokens.Append(token);

			req.IncludeOptionalField(H225_AdmissionRequest::e_willSupplyUUIEs);
			req.m_willSupplyUUIEs = true;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();

			uint32_t seq = req.m_requestSeqNum;
			std::string sseq=std::to_string(seq);
			if (!echo_.set(sseq, msg))
				return false;

			for (int i = 0; i < 3; i++)
			{
				socket_->send_to((const char*)stream.GetPointer(), stream.GetSize(), (const sockaddr*)&addr_, sizeof(addr_));
				if (echo_.try_get(sseq, msg, rsp, 1000))
				{
					return true;
				}

			}
			echo_.remove(sseq);

			return false;
		}

		bool gk_client::send_disengage_request(
			H225_DisengageReason::Choices reason,
			const PGloballyUniqueID& call_id,
			const PGloballyUniqueID& conf_id,
			int call_ref)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_disengageRequest);
			H225_DisengageRequest& req = (H225_DisengageRequest&)msg;
			req.m_requestSeqNum = request_seq_.fetch_add(1);

			req.m_endpointIdentifier = endpoint_identifier_;
			req.m_conferenceID = conf_id;
			req.m_callReferenceValue = call_ref;
			req.m_disengageReason.SetTag(reason);

			req.IncludeOptionalField(H225_DisengageRequest::e_callIdentifier);
			req.m_callIdentifier.m_guid = call_id;
			req.IncludeOptionalField(H225_DisengageRequest::e_gatekeeperIdentifier);
			req.m_gatekeeperIdentifier = gk_identifier_;

			H225_CryptoH323Token* token = new H225_CryptoH323Token();
			h323_util::make_md5_crypto_token(username_, password_, *token);
			req.IncludeOptionalField(H225_DisengageRequest::e_cryptoTokens);
			req.m_cryptoTokens.Append(token);

			req.IncludeOptionalField(H225_DisengageRequest::e_answeredCall);
			req.m_answeredCall = true;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			return socket_->send_to((const char*)stream.GetPointer(), stream.GetSize(), (const sockaddr*)&addr_, sizeof(addr_));
		}

		bool gk_client::send_gatekeeper_request()
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_gatekeeperRequest);
			H225_GatekeeperRequest& req = (H225_GatekeeperRequest&)msg;
			req.m_requestSeqNum = request_seq_.fetch_add(1);
			req.m_protocolIdentifier.SetValue(H225_ProtocolID, PARRAYSIZE(H225_ProtocolID));

			h323_util::h225_transport_address_set(req.m_rasAddress, PIPSocket::Address(nat_address_), local_port_);

			//req.m_endpointType.IncludeOptionalField(H225_EndpointType::e_terminal);
			req.m_endpointType.IncludeOptionalField(H225_EndpointType::e_vendor);
			h323_util::fill_vendor(req.m_endpointType.m_vendor);
			req.m_endpointType.m_mc = false;
			req.m_endpointType.m_undefinedNode = false;

			req.IncludeOptionalField(H225_GatekeeperRequest::e_endpointAlias);
			req.m_endpointAlias.RemoveAll();
			H323SetAliasAddresses((const PStringArray&)alias_,req.m_endpointAlias);

			
			req.IncludeOptionalField(H225_GatekeeperRequest::e_tokens);
			H235_ClearToken* token = new H235_ClearToken();
			token->m_tokenOID.SetValue("0.0.8.235.0.3.48");
			req.m_tokens.Append(token);

			req.IncludeOptionalField(H225_GatekeeperRequest::e_authenticationCapability);
			req.m_authenticationCapability.RemoveAll();
			H235_AuthenticationMechanism* auth = new H235_AuthenticationMechanism();
			auth->SetTag(H235_AuthenticationMechanism::e_pwdHash);
			req.m_authenticationCapability.Append(auth);

			req.IncludeOptionalField(H225_GatekeeperRequest::e_algorithmOIDs);
			req.m_algorithmOIDs.RemoveAll();
			PASN_ObjectId* algorithm_oids = new PASN_ObjectId();
			algorithm_oids->SetValue("1.2.840.113549.2.5");
			req.m_algorithmOIDs.Append(algorithm_oids);

			req.IncludeOptionalField(H225_GatekeeperRequest::e_featureSet);
			req.m_featureSet.m_replacementFeatureSet = false;
			req.m_featureSet.IncludeOptionalField(H225_FeatureSet::e_supportedFeatures);
			req.m_featureSet.m_supportedFeatures.RemoveAll();
			H225_FeatureDescriptor* fea_desc = new H225_FeatureDescriptor();
			fea_desc->m_id.SetTag(H225_GenericIdentifier::e_standard);
			((PASN_Integer&)fea_desc->m_id) = 18;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			return socket_->send_to((const char*)stream.GetPointer(), stream.GetSize(), (const sockaddr*)&addr_, sizeof(addr_));
		}

		bool gk_client::send_registration_request()
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_registrationRequest);
			H225_RegistrationRequest& req = (H225_RegistrationRequest&)msg;
			req.m_requestSeqNum = request_seq_.fetch_add(1);
			req.m_protocolIdentifier.SetValue(H225_ProtocolID, PARRAYSIZE(H225_ProtocolID));
			req.m_discoveryComplete = false;

			H225_TransportAddress* sig_addr = new H225_TransportAddress();
			h323_util::h225_transport_address_set(*sig_addr, PIPSocket::Address(nat_address_), signal_port_);
			req.m_callSignalAddress.Append(sig_addr);


			H225_TransportAddress* ras_addr = new H225_TransportAddress();
			h323_util::h225_transport_address_set(*ras_addr, PIPSocket::Address(nat_address_), local_port_);
			req.m_rasAddress.Append(ras_addr);

			req.m_terminalType.IncludeOptionalField(H225_EndpointType::e_vendor);
			h323_util::fill_vendor(req.m_terminalType.m_vendor);
			req.m_terminalType.IncludeOptionalField(H225_EndpointType::e_terminal);
			req.m_terminalType.m_mc = false;
			req.m_terminalType.m_undefinedNode = false;

			req.IncludeOptionalField(H225_RegistrationRequest::e_terminalAlias);
			req.m_terminalAlias.RemoveAll();
			H323SetAliasAddresses((const PStringArray&)alias_, req.m_terminalAlias);

			req.m_endpointVendor.IncludeOptionalField(H225_EndpointType::e_vendor);
			h323_util::fill_vendor(req.m_endpointVendor);

			req.IncludeOptionalField(H225_RegistrationRequest::e_timeToLive);
			req.m_timeToLive = time_to_live_;


			H225_CryptoH323Token* token = new H225_CryptoH323Token();
			h323_util::make_md5_crypto_token(username_, password_, *token);
			req.IncludeOptionalField(H225_RegistrationRequest::e_cryptoTokens);
			req.m_cryptoTokens.Append(token);

			req.IncludeOptionalField(H225_RegistrationRequest::e_keepAlive);
			req.m_keepAlive=false;
			if (!endpoint_identifier_.IsEmpty()) {
				req.IncludeOptionalField(H225_RegistrationRequest::e_endpointIdentifier);
				req.m_endpointIdentifier = endpoint_identifier_;
				req.m_keepAlive = true;
			}
			req.IncludeOptionalField(H225_RegistrationRequest::e_willSupplyUUIEs);
			req.m_willSupplyUUIEs = true;
			req.IncludeOptionalField(H225_RegistrationRequest::e_maintainConnection);
			req.m_maintainConnection = false;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			return socket_->send_to((const char*)stream.GetPointer(), stream.GetSize(), (const sockaddr*)&addr_, sizeof(addr_));
		}

		bool gk_client::send_unregistration_request(H225_UnregRequestReason::Choices reason)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_unregistrationRequest);
			H225_UnregistrationRequest& req = (H225_UnregistrationRequest&)msg;
			req.m_requestSeqNum = request_seq_.fetch_add(1);
			
			if (!endpoint_identifier_.IsEmpty())
			{
				req.IncludeOptionalField(H225_UnregistrationRequest::e_endpointIdentifier);
				req.m_endpointIdentifier = endpoint_identifier_;
			}
			//if (!gk_identifier_.IsEmpty())
			//{
			//	req.IncludeOptionalField(H225_UnregistrationRequest::e_gatekeeperIdentifier);
			//	req.m_gatekeeperIdentifier = gk_identifier_;
			//}
			//req.IncludeOptionalField(H225_UnregistrationRequest::e_reason);
			//req.m_reason = reason;

			H225_TransportAddress* sig_addr = new H225_TransportAddress();
			h323_util::h225_transport_address_set(*sig_addr, PIPSocket::Address(nat_address_), signal_port_);
			req.m_callSignalAddress.Append(sig_addr);

			H225_CryptoH323Token* token = new H225_CryptoH323Token();
			h323_util::make_md5_crypto_token(username_, password_, *token);
			req.IncludeOptionalField(H225_UnregistrationRequest::e_cryptoTokens);
			req.m_cryptoTokens.Append(token);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			return socket_->send_to((const char*)stream.GetPointer(), stream.GetSize(), (const sockaddr*)&addr_, sizeof(addr_));
		}

		bool gk_client::send_info_request(
			sys::async_socket_ptr socket,
			uint32_t seq,
			const PGloballyUniqueID& call_id,
			const uint32_t call_ref,
			const sockaddr* addr, socklen_t addr_size)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_infoRequest);
			H225_InfoRequest& req = (H225_InfoRequest&)msg;
			req.m_requestSeqNum = request_seq_.fetch_add(1);
			req.m_callReferenceValue = call_ref;
			req.IncludeOptionalField(H225_InfoRequest::e_callIdentifier);
			req.m_callIdentifier.m_guid = call_id;

			req.IncludeOptionalField(H225_InfoRequest::e_genericData);
			req.m_genericData.RemoveAll();
			H225_GenericData* gen = new H225_GenericData();
			gen->m_id.SetTag(H225_GenericIdentifier::e_standard);
			((PASN_Integer&)gen->m_id) = 18;
			req.m_genericData.Append(gen);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			return socket->send_to((const char*)stream.GetPointer(), stream.GetSize(), addr, addr_size);
		}

		bool gk_client::send_info_request_ack(
			sys::async_socket_ptr socket,
			uint32_t seq,
			const PString& signal_address, int signal_port,
			const sockaddr* addr, socklen_t addr_size)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_infoRequestAck);
			H225_InfoRequestAck& ack = (H225_InfoRequestAck&)msg;
			ack.m_requestSeqNum = seq;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			return socket->send_to((const char*)stream.GetPointer(), stream.GetSize(), addr, addr_size);
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

				if (!is_registered_)
				{
					send_gatekeeper_request();
				}
				else
				{
					send_registration_request();
				}
			}

			send_unregistration_request(H225_UnregRequestReason::Choices::e_undefinedReason);
		}


		void gk_client::s_on_read_event(void* ctx, sys::async_socket_ptr socket, const char* buffer, uint32_t size, const sockaddr* addr, socklen_t addr_size)
		{
			gk_client* p = (gk_client*)ctx;
			PPER_Stream strm((const BYTE*)buffer, size);
			H225_RasMessage ras;
			if (ras.Decode(strm))
			{
				if (ras.GetTag() == H225_RasMessage::e_gatekeeperConfirm)
				{
					H225_GatekeeperConfirm& body = (H225_GatekeeperConfirm&)ras;
					p->on_gatekeeper_confirm(body);
				}
				else if (ras.GetTag() == H225_RasMessage::e_gatekeeperReject)
				{
					H225_GatekeeperReject& body = (H225_GatekeeperReject&)ras;
					p->on_gatekeeper_reject(body);
				}
				else if (ras.GetTag() == H225_RasMessage::e_registrationConfirm)
				{
					H225_RegistrationConfirm& body = (H225_RegistrationConfirm&)ras;
					p->on_registration_confirm(body);
				}
				else if (ras.GetTag() == H225_RasMessage::e_registrationReject)
				{
					H225_RegistrationReject& body = (H225_RegistrationReject&)ras;
					p->on_registration_reject(body);
				}
				else if (ras.GetTag() == H225_RasMessage::e_serviceControlIndication)
				{
					H225_ServiceControlIndication& body = (H225_ServiceControlIndication&)ras;
					p->on_service_control_indication(body);
				}
				else if (ras.GetTag() == H225_RasMessage::e_infoRequestAck)
				{
					H225_InfoRequestAck& body = (H225_InfoRequestAck&)ras;
					p->on_info_request_ack(body);
				}
				else if (ras.GetTag() == H225_RasMessage::e_disengageConfirm)
				{
					H225_DisengageConfirm& body = (H225_DisengageConfirm&)ras;
					p->on_disengage_confirm(body);
				}
				else if (ras.GetTag() == H225_RasMessage::e_disengageReject)
				{
					H225_DisengageReject& body = (H225_DisengageReject&)ras;
					p->on_disengage_reject(body);
				}
				else if (ras.GetTag() == H225_RasMessage::e_admissionConfirm)
				{
					p->on_admission_confirm(ras);
				}
				else if (ras.GetTag() == H225_RasMessage::e_admissionReject)
				{
					p->on_admission_reject(ras);
				}
			}


			socket->post_read_from();
		}

		void gk_client::on_gatekeeper_confirm(const H225_GatekeeperConfirm& body)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			send_registration_request();
		}

		void gk_client::on_gatekeeper_reject(const H225_GatekeeperReject& body)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			set_status(false);
		}

		void gk_client::on_registration_confirm(const H225_RegistrationConfirm& body)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			if (body.HasOptionalField(H225_RegistrationConfirm::e_timeToLive))
			{
				time_to_live_ = body.m_timeToLive;
			}

			endpoint_identifier_ = body.m_endpointIdentifier;
			if (body.HasOptionalField(H225_RegistrationConfirm::e_gatekeeperIdentifier))
			{
				gk_identifier_ = body.m_gatekeeperIdentifier;
			}

			updated_at_ = time(nullptr);
			set_status(true);
		}

		void gk_client::on_registration_reject(const H225_RegistrationReject& body)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			set_status(false);
			endpoint_identifier_ = "";
			gk_identifier_ = "";
		}

		void gk_client::on_service_control_indication(const H225_ServiceControlIndication& body)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			H225_ServiceControlResponse_result::Choices result = H225_ServiceControlResponse_result::e_started;
			send_service_control_response(body, result);
			on_incoming_call.invoke(body,result);
		}

		void gk_client::on_info_request_ack(const H225_InfoRequestAck& body)
		{
		}

		void gk_client::on_disengage_confirm(const H225_DisengageConfirm& body)
		{

		}

		void gk_client::on_disengage_reject(const H225_DisengageReject& body)
		{

		}

		void gk_client::on_admission_confirm(const H225_RasMessage& ras)
		{
			const H225_AdmissionConfirm& body = (const H225_AdmissionConfirm&)ras;
			std::string sseq = std::to_string((uint32_t)body.m_requestSeqNum);
			echo_.response(sseq, ras);
		}

		void gk_client::on_admission_reject(const H225_RasMessage& ras)
		{
			const H225_AdmissionReject& body = (const H225_AdmissionReject&)ras;
			std::string sseq = std::to_string((uint32_t)body.m_requestSeqNum);
			echo_.response(sseq, ras);
		}
	}
}

