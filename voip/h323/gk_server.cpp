#include "gk_server.h"
#include "h323_util.h"
#include <h323/h323pdu.h>
#include <h460/h46018.h>
#include <sys2/util.h>



namespace voip
{
	namespace h323
	{

		gk_server::gk_server(asio::io_context& ioc)
			:ioc_(ioc)
			, timer_(ioc_)
			, skt_(ioc_)
			,request_seq_(0)
		{
		}

		gk_server::~gk_server()
		{
			stop();
		}

		bool gk_server::start(const std::string& addr, int port)
		{
			if (active_)
				return false;
			active_ = true;

			std::error_code ec;

			asio::ip::udp::endpoint ep(asio::ip::address::from_string(addr, ec), port);
			if (ec)
			{
				stop();
				return false;
			}
			
			skt_.open(ep.protocol(), ec);
			if (ec)
			{
				stop();
				return false;
			}
			skt_.set_option(asio::socket_base::reuse_address(true), ec);
			skt_.bind(ep, ec);
			if (ec)
			{
				stop();
				return false;
			}

			auto self = shared_from_this();
			skt_.async_receive_from(asio::buffer(recv_buffer_), recv_ep_, std::bind(&gk_server::handle_read, this, self, std::placeholders::_1, std::placeholders::_2));

			timer_.expires_after(std::chrono::seconds(10));
			timer_.async_wait(std::bind(&gk_server::handle_timer, this, self, std::placeholders::_1));
			return true;
		}

		void gk_server::stop()
		{
			active_ = false;
			std::error_code ec;
			timer_.cancel(ec);
			skt_.cancel(ec);
			
			clear_reg_endpoints();
		}

		bool gk_server::call(const PString& alias,
			const PString& signal_address, int signal_port,
			const std::string& call_id, const std::string& conf_id)
		{
			reg_endpoint ep;
			if (!this->get_reg_endpoint((PStringArray)alias, ep))
				return false;

			PGloballyUniqueID callid,confid;
			if (call_id.empty())
			{
				callid = (PString)sys::util::uuid(sys::uuid_fmt_t::bar);
			}
			else
			{
				callid = (PString)call_id;
			}
			if (conf_id.empty())
			{
				confid = (PString)sys::util::uuid(sys::uuid_fmt_t::bar);
			}
			else
			{
				confid = (PString)conf_id;
			}

			std::error_code ec;
			asio::ip::udp::endpoint addr(asio::ip::address::from_string(ep.address,ec), ep.port);
			H225_RasMessage rsp;
			return this->service_control_indication(this->skt_, callid, confid, signal_address, signal_port, addr,rsp);
		}

		void gk_server::handle_timer(gk_server_ptr self, const std::error_code& ec)
		{
			if (ec || !active_)
				return;

			std::vector<reg_endpoint> eps;
			all_reg_endpoint(eps);

			for (auto itr = eps.begin(); itr != eps.end(); itr++)
			{
				if (itr->is_timeout())
				{
					remove_reg_endpoint(itr->id());
				}
			}

			timer_.expires_after(std::chrono::seconds(10));
			timer_.async_wait(std::bind(&gk_server::handle_timer, this, self, std::placeholders::_1));
		}

		void gk_server::handle_read(gk_server_ptr self, const std::error_code& ec, std::size_t bytes)
		{
			if (ec)
				return;

			PPER_Stream strm((const BYTE*)recv_buffer_.data(),bytes);
			H225_RasMessage ras;
			if (ras.Decode(strm))
			{
				if (ras.GetTag() == H225_RasMessage::e_gatekeeperRequest)
				{
					H225_GatekeeperRequest& request = (H225_GatekeeperRequest&)ras;
					on_gatekeeper_request(skt_, request, recv_ep_);
				}
				else if (ras.GetTag() == H225_RasMessage::e_registrationRequest)
				{
					H225_RegistrationRequest& request = (H225_RegistrationRequest&)ras;
					on_registration_request(skt_, request, recv_ep_);
				}
				else if (ras.GetTag() == H225_RasMessage::e_unregistrationRequest)
				{
					H225_UnregistrationRequest& request = (H225_UnregistrationRequest&)ras;
					on_unregistration_request(skt_, request, recv_ep_);
				}
				else if (ras.GetTag() == H225_RasMessage::e_serviceControlResponse)
				{
					H225_ServiceControlResponse& body = (H225_ServiceControlResponse&)ras;
					uint32_t seq = (uint32_t)body.m_requestSeqNum;
					std::string sseq = std::to_string(seq);
					echo_.response(sseq, ras);
				}
				else if (ras.GetTag() == H225_RasMessage::e_infoRequestResponse)
				{
					H225_InfoRequestResponse& rsp = (H225_InfoRequestResponse&)ras;
					on_info_request_response(skt_, rsp, recv_ep_);
				}
				else if (ras.GetTag() == H225_RasMessage::e_admissionRequest)
				{
					H225_AdmissionRequest& request = (H225_AdmissionRequest&)ras;
					on_admission_request(skt_, request, recv_ep_);
				}
				else if (ras.GetTag() == H225_RasMessage::e_disengageRequest)
				{
					H225_DisengageRequest& request = (H225_DisengageRequest&)ras;
					on_disengage_request(skt_, request, recv_ep_);
				}
				else if (ras.GetTag() == H225_RasMessage::e_locationRequest)
				{
					H225_LocationRequest& request = (H225_LocationRequest&)ras;
					on_location_request(skt_, request, recv_ep_);
				}
			}


			skt_.async_receive_from(asio::buffer(recv_buffer_), recv_ep_, std::bind(&gk_server::handle_read,this,self,std::placeholders::_1,std::placeholders::_2));
		}


		void gk_server::on_gatekeeper_request(asio::ip::udp::socket& socket,
			const H225_GatekeeperRequest& request,
			const asio::ip::udp::endpoint& ep)
		{
			send_gatekeeper_confirm(socket, request,ep);
		}

		void gk_server::on_registration_request(
			asio::ip::udp::socket& socket,
			const H225_RegistrationRequest& request,
			const asio::ip::udp::endpoint& addr)
		{
			if (!request.HasOptionalField(H225_RegistrationRequest::e_cryptoTokens))
			{
				send_registration_reject(socket, request, H225_RegistrationRejectReason::e_securityDenial, addr);
				return;
			}

			std::string id = reg_endpoint::make_id(addr);
			reg_endpoint ep;
			if (request.HasOptionalField(H225_RegistrationRequest::e_terminalAlias))
			{
				ep.aliases = H323GetAliasAddressStrings(request.m_terminalAlias);
			}
			if (!get_reg_endpoint(id, ep))
			{
				if(!require_aliases_password(ep.aliases,ep.password))
				{
					send_registration_reject(socket, request, H225_RegistrationRejectReason::e_securityDenial, addr);
					return;
				}
			}

			if (!check_auth(request.m_cryptoTokens,ep.password))
			{
				send_registration_reject(socket, request, H225_RegistrationRejectReason::e_invalidAlias, addr);
				return;
			}

			if (request.HasOptionalField(H225_RegistrationRequest::e_featureSet))
			{
				if (!ep.feature_set)
				{
					ep.feature_set.reset(request.m_featureSet.CloneAs<H225_FeatureSet>());
				}
			}
			if (request.m_rasAddress.GetSize() > 0) {
				h323_util::h225_transport_address_get(request.m_rasAddress[(PINDEX)0], ep.ras_address, ep.ras_port);
			}
			if (request.m_callSignalAddress.GetSize() > 0) {
				h323_util::h225_transport_address_get(request.m_callSignalAddress[(PINDEX)0], ep.signal_address, ep.signal_port);
			}
			ep.updated_at = time(nullptr);
			std::error_code ec;
			ep.address = addr.address().to_string(ec);
			ep.port = addr.port();
			set_reg_endpoint(ep);
			send_registration_confirm(socket, request, ep.id(), addr);
			if (on_reg_endpoint)
			{
				auto self = shared_from_this();
				on_reg_endpoint(self, ep);
			}
		}

		void gk_server::on_unregistration_request(
			asio::ip::udp::socket& socket,
			const H225_UnregistrationRequest& request, 
			const asio::ip::udp::endpoint& ep)
		{
			if (!request.HasOptionalField(H225_UnregistrationRequest::e_cryptoTokens))
			{
				send_unregistration_reject(socket, request, H225_UnregRejectReason::e_securityDenial, ep);
				return;
			}

			/*
			if (!check_auth(request.m_cryptoTokens))
			{
				send_unregistration_reject(socket, request, H225_UnregRejectReason::e_securityDenial, addr, addr_size);
				return;
			}
			*/
			PString id;
			if (request.HasOptionalField(H225_UnregistrationRequest::e_endpointIdentifier))
			{
				id = request.m_endpointIdentifier;
			}
			else
			{
				id=reg_endpoint::make_id(ep);
			}
			remove_reg_endpoint(id);

			send_unregistration_confirm(socket, request,ep);
		}

		void gk_server::on_info_request_response(asio::ip::udp::socket& socket,
			const H225_InfoRequestResponse& request,
			const asio::ip::udp::endpoint& ep)
		{

			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_infoRequestAck);
			H225_InfoRequestAck& ack = (H225_InfoRequestAck&)msg;
			ack.m_requestSeqNum = request.m_requestSeqNum;
			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			asio::error_code ec;
			socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()), ep,0,ec);
		}

		void gk_server::on_admission_request(asio::ip::udp::socket& socket,
			const H225_AdmissionRequest& request,
			const asio::ip::udp::endpoint& ep)
		{
			PStringArray called_aliases;
			if (request.HasOptionalField(H225_AdmissionRequest::e_destinationInfo))
			{
				called_aliases = H323GetAliasAddressStrings(request.m_destinationInfo);
			}

			PString signal_address;
			WORD signal_port = 0;
			bool result = false;
			if (on_admission)
			{
				auto self = shared_from_this();
				on_admission(self, called_aliases, signal_address, signal_port, result);
			}
			if (!result || signal_address.IsEmpty() || signal_port == 0)
			{
				send_admission_reject(socket, request, H225_AdmissionRejectReason::Choices::e_collectDestination,ep);
				return;
			}
			PIPSocket::Address sigaddr(signal_address);
			if (sigaddr.IsAny() || sigaddr.IsLoopback())
			{
				send_admission_reject(socket, request, H225_AdmissionRejectReason::Choices::e_collectDestination, ep);
				return;
			}

			send_admission_confrim(socket, request, sigaddr,signal_port, 2560, ep);
		}

		void gk_server::on_disengage_request(asio::ip::udp::socket& socket,
			const H225_DisengageRequest& request,
			const asio::ip::udp::endpoint& ep)
		{
			send_disengage_confrim(socket, request,ep);
		}

		void gk_server::on_location_request(asio::ip::udp::socket& socket,
			const H225_LocationRequest& request,
			const asio::ip::udp::endpoint& ep)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_locationConfirm);
			H225_LocationConfirm& confirm = (H225_LocationConfirm&)msg;
			confirm.m_requestSeqNum = request.m_requestSeqNum;
			
			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			asio::error_code ec;
			socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()),ep,0,ec);
		}








		bool gk_server::send_gatekeeper_confirm(
			asio::ip::udp::socket& socket,
			const H225_GatekeeperRequest& request, 
			const asio::ip::udp::endpoint& ep)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_gatekeeperConfirm);
			H225_GatekeeperConfirm& confirm = (H225_GatekeeperConfirm&)msg;
			confirm.m_requestSeqNum = request.m_requestSeqNum;
			confirm.m_protocolIdentifier = request.m_protocolIdentifier;
			confirm.IncludeOptionalField(H225_GatekeeperConfirm::e_gatekeeperIdentifier);
			confirm.m_gatekeeperIdentifier = gk_identifier_;
			confirm.m_rasAddress.SetTag(H225_TransportAddress::e_ipAddress);
			H225_TransportAddress_ipAddress& ras_addr = (H225_TransportAddress_ipAddress&)confirm.m_rasAddress;
			PIPSocket::Address ras_nat(nat_address_);
			ras_addr.m_ip = ras_nat;
			ras_addr.m_port = port_;

			confirm.IncludeOptionalField(H225_GatekeeperConfirm::e_authenticationMode);
			confirm.m_authenticationMode.SetTag(H235_AuthenticationMechanism::e_pwdHash);
			confirm.IncludeOptionalField(H225_GatekeeperConfirm::e_algorithmOID);
			confirm.m_algorithmOID = OID_MD5;
			confirm.IncludeOptionalField(H225_GatekeeperConfirm::e_featureSet);
			confirm.m_featureSet = request.m_featureSet;
			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			asio::error_code ec;
			return socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()),ep,0,ec);
		}

		bool gk_server::send_gatekeeper_reject(
			asio::ip::udp::socket& socket,
			const H225_GatekeeperRequest& request,
			H225_GatekeeperRejectReason::Choices reason,
			const asio::ip::udp::endpoint& ep)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_gatekeeperReject);
			H225_GatekeeperReject& reject = (H225_GatekeeperReject&)msg;
			reject.m_requestSeqNum = request.m_requestSeqNum;
			reject.m_protocolIdentifier = request.m_protocolIdentifier;
			reject.IncludeOptionalField(H225_GatekeeperConfirm::e_gatekeeperIdentifier);
			reject.m_gatekeeperIdentifier = gk_identifier_;
			if (request.HasOptionalField(H225_GatekeeperConfirm::e_featureSet))
			{
				reject.IncludeOptionalField(H225_GatekeeperConfirm::e_featureSet);
				reject.m_featureSet = request.m_featureSet;
			}

			reject.m_rejectReason.SetTag(reason);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			asio::error_code ec;
			return socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()),ep,0,ec);
		}

		bool gk_server::send_registration_confirm(
			asio::ip::udp::socket& socket,
			const H225_RegistrationRequest& request,
			const PString& ep_identifier, 
			const asio::ip::udp::endpoint& ep)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_registrationConfirm);
			H225_RegistrationConfirm& confirm = (H225_RegistrationConfirm&)msg;
			confirm.m_requestSeqNum = request.m_requestSeqNum;
			confirm.m_protocolIdentifier = request.m_protocolIdentifier;
			confirm.m_callSignalAddress = request.m_callSignalAddress;
			if (request.HasOptionalField(H225_RegistrationConfirm::e_terminalAlias))
			{
				confirm.IncludeOptionalField(H225_RegistrationConfirm::e_terminalAlias);
				confirm.m_terminalAlias = request.m_terminalAlias;
			}
			confirm.IncludeOptionalField(H225_RegistrationConfirm::e_gatekeeperIdentifier);
			confirm.m_gatekeeperIdentifier = gk_identifier_;
			confirm.m_endpointIdentifier = ep_identifier;
			confirm.IncludeOptionalField(H225_RegistrationConfirm::e_timeToLive);
			confirm.m_timeToLive = def_time_to_live_;
			confirm.IncludeOptionalField(H225_RegistrationConfirm::e_willRespondToIRR);
			confirm.m_willRespondToIRR = true;
			confirm.m_preGrantedARQ.m_irrFrequencyInCall = 60;
			confirm.IncludeOptionalField(H225_RegistrationConfirm::e_maintainConnection);
			confirm.m_maintainConnection = false;
			confirm.IncludeOptionalField(H225_RegistrationConfirm::e_featureSet);
			confirm.m_featureSet = request.m_featureSet;
			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			asio::error_code ec;
			return socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()),ep,0,ec);
		}

		bool gk_server::send_registration_reject(
			asio::ip::udp::socket& socket,
			const H225_RegistrationRequest& request, 
			H225_RegistrationRejectReason::Choices reason,
			const asio::ip::udp::endpoint& ep)
		{
			PStringArray aliases;
			if (request.HasOptionalField(H225_RegistrationRequest::e_terminalAlias))
			{
				aliases = H323GetAliasAddressStrings(request.m_terminalAlias);
			}
			if (on_reg_endpoint_failed)
			{
				auto self = shared_from_this();
				on_reg_endpoint_failed(self, aliases, reason);
			}

			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_registrationReject);
			H225_RegistrationReject& reject = (H225_RegistrationReject&)msg;
			reject.m_requestSeqNum = request.m_requestSeqNum;
			reject.m_protocolIdentifier = request.m_protocolIdentifier;
			reject.m_rejectReason.SetTag(reason);
			reject.IncludeOptionalField(H225_RegistrationReject::e_gatekeeperIdentifier);
			reject.m_gatekeeperIdentifier = gk_identifier_;
			reject.IncludeOptionalField(H225_RegistrationReject::e_featureSet);
			reject.m_featureSet = request.m_featureSet;
			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			asio::error_code ec;
			return socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()),ep,0,ec);
		}

		bool gk_server::send_unregistration_confirm(
			asio::ip::udp::socket& socket,
			const H225_UnregistrationRequest& request,
			const asio::ip::udp::endpoint& ep)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_unregistrationConfirm);
			H225_UnregistrationConfirm& confirm = (H225_UnregistrationConfirm&)msg;
			confirm.m_requestSeqNum = request.m_requestSeqNum;
			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			asio::error_code ec;
			return socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()),ep,0,ec);
		}

		bool gk_server::send_unregistration_reject(
			asio::ip::udp::socket& socket,
			const H225_UnregistrationRequest& request, 
			H225_UnregRejectReason::Choices reason,
			const asio::ip::udp::endpoint& ep)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_unregistrationReject);
			H225_UnregistrationReject& reject = (H225_UnregistrationReject&)msg;
			reject.m_requestSeqNum = request.m_requestSeqNum;
			reject.m_rejectReason.SetTag(reason);
			reject.IncludeOptionalField(H225_RegistrationReject::e_gatekeeperIdentifier);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			asio::error_code ec;
			return socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()),ep,0,ec);

		}

		bool gk_server::send_admission_confrim(asio::ip::udp::socket& socket,
			const H225_AdmissionRequest& request,
			const PIPSocket::Address& signal_address, WORD signal_port,
			int bandwidth,
			const asio::ip::udp::endpoint& ep)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_admissionConfirm);
			H225_AdmissionConfirm& confirm = (H225_AdmissionConfirm&)msg;
			confirm.m_requestSeqNum = request.m_requestSeqNum;
			confirm.m_bandWidth = bandwidth;
			confirm.m_callModel.SetTag(H225_CallModel::e_gatekeeperRouted);

			if (request.HasOptionalField(H225_AdmissionRequest::e_srcCallSignalAddress)) {
				confirm.m_destCallSignalAddress = request.m_srcCallSignalAddress;
				confirm.m_destCallSignalAddress.SetTag(H225_TransportAddress::e_ipAddress);
			}

			h323_util::h225_transport_address_set(confirm.m_destCallSignalAddress, signal_address, signal_port);

			confirm.IncludeOptionalField(H225_AdmissionConfirm::e_irrFrequency);
			confirm.m_irrFrequency = 60;
			confirm.IncludeOptionalField(H225_AdmissionConfirm::e_willRespondToIRR);
			confirm.m_willRespondToIRR = true;
			confirm.IncludeOptionalField(H225_AdmissionConfirm::e_uuiesRequested);
			confirm.m_uuiesRequested.m_setup = false;
			confirm.m_uuiesRequested.m_callProceeding = false;
			confirm.m_uuiesRequested.m_connect = true;
			confirm.m_uuiesRequested.m_alerting = true;
			confirm.m_uuiesRequested.m_information = false;
			confirm.m_uuiesRequested.m_releaseComplete = false;
			confirm.m_uuiesRequested.m_facility = false;
			confirm.m_uuiesRequested.m_progress = false;
			confirm.m_uuiesRequested.m_empty = false;
			confirm.m_uuiesRequested.IncludeOptionalField(H225_UUIEsRequested::e_status);
			confirm.m_uuiesRequested.m_status = false;
			confirm.m_uuiesRequested.IncludeOptionalField(H225_UUIEsRequested::e_statusInquiry);
			confirm.m_uuiesRequested.m_statusInquiry = false;
			confirm.m_uuiesRequested.IncludeOptionalField(H225_UUIEsRequested::e_setupAcknowledge);
			confirm.m_uuiesRequested.m_setupAcknowledge = false;
			confirm.m_uuiesRequested.IncludeOptionalField(H225_UUIEsRequested::e_notify);
			confirm.m_uuiesRequested.m_notify = false;


			H225_GenericData* item = new H225_GenericData();
			item->m_id.SetTag(H225_GenericIdentifier::e_standard);
			item->IncludeOptionalField(H225_GenericData::e_parameters);
			((PASN_Integer&)item->m_id) = 18; // Signalling Traversal
			confirm.IncludeOptionalField(H225_AdmissionConfirm::e_genericData);
			confirm.m_genericData.Append(item);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			asio::error_code ec;
			return socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()),ep,0,ec);
		}
		bool gk_server::send_admission_reject(asio::ip::udp::socket& socket,
			const H225_AdmissionRequest& request,
			H225_AdmissionRejectReason::Choices reason,
			const asio::ip::udp::endpoint& ep)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_admissionReject);
			H225_AdmissionReject& reject = (H225_AdmissionReject&)msg;
			reject.m_requestSeqNum = request.m_requestSeqNum;
			reject.m_rejectReason = reason;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			asio::error_code ec;
			return socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()),ep,0,ec);
		}

		bool gk_server::send_disengage_confrim(asio::ip::udp::socket& socket,
			const H225_DisengageRequest& request,
			const asio::ip::udp::endpoint& ep)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_disengageConfirm);
			H225_DisengageConfirm& confirm = (H225_DisengageConfirm&)msg;
			confirm.m_requestSeqNum = request.m_requestSeqNum;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			asio::error_code ec;
			return socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()),ep,0,ec);
		}

		bool gk_server::send_disengage_reject(asio::ip::udp::socket& socket,
			const H225_DisengageRequest& request,
			H225_DisengageRejectReason::Choices reason,
			const asio::ip::udp::endpoint& ep)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_disengageReject);
			H225_DisengageReject& reject = (H225_DisengageReject&)msg;
			reject.m_requestSeqNum = request.m_requestSeqNum;
			reject.m_rejectReason = reason;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			asio::error_code ec;
			return socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()),ep,0,ec);
		}

		bool gk_server::send_location_confrim(asio::ip::udp::socket& socket,
			const H225_LocationRequest& request,
			int bandwidth,
			const asio::ip::udp::endpoint& ep)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_locationConfirm);
			H225_LocationConfirm& confirm = (H225_LocationConfirm&)msg;
			confirm.m_requestSeqNum = request.m_requestSeqNum;
			confirm.m_bandWidth = bandwidth;

			confirm.m_rasAddress.SetTag(H225_TransportAddress::e_ipAddress);
			H225_TransportAddress_ipAddress& ras_addr = (H225_TransportAddress_ipAddress&)confirm.m_rasAddress;
			PIPSocket::Address ras_nat(nat_address_);
			ras_addr.m_ip = ras_nat;
			ras_addr.m_port = port_;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			asio::error_code ec;
			return socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()),ep,0,ec);
		}
		bool gk_server::send_location_reject(asio::ip::udp::socket& socket,
			const H225_LocationRequest& request,
			H225_LocationRejectReason::Choices reason,
			const asio::ip::udp::endpoint& ep)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_locationReject);
			H225_LocationReject& reject = (H225_LocationReject&)msg;
			reject.m_requestSeqNum = request.m_requestSeqNum;
			reject.m_rejectReason = reason;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			asio::error_code ec;
			return socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()),ep,0,ec);
		}

		bool gk_server::send_service_control_indication(
			asio::ip::udp::socket& socket,
			const PGloballyUniqueID& call_id,
			const PGloballyUniqueID& conf_id,
			const PString& signal_address, int signal_port,
			const asio::ip::udp::endpoint& ep)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_serviceControlIndication);
			H225_ServiceControlIndication& sci = (H225_ServiceControlIndication&)msg;
			sci.m_requestSeqNum = request_seq_.fetch_add(1);

			sci.IncludeOptionalField(H225_ServiceControlIndication::e_callSpecific);
			sci.m_callSpecific.m_callIdentifier.m_guid = call_id;
			sci.m_callSpecific.m_conferenceID = conf_id;


			H225_GenericData* item = new H225_GenericData();
			item->m_id.SetTag(H225_GenericIdentifier::e_standard);
			item->IncludeOptionalField(H225_GenericData::e_parameters);
			(PASN_Integer&)item->m_id = 18; // Signalling Traversal

			H46018_IncomingCallIndication inCallInd;

			inCallInd.m_callID.m_guid = call_id;
			inCallInd.m_callSignallingAddress.SetTag(H225_TransportAddress::e_ipAddress);
			h323_util::h225_transport_address_set(inCallInd.m_callSignallingAddress, PIPSocket::Address(signal_address), signal_port);

			item->m_parameters[0].SetTag(H225_EnumeratedParameter::e_content);
			item->m_parameters[0].m_id.SetTag(H225_GenericIdentifier::e_standard);
			item->m_parameters[0].IncludeOptionalField(H225_EnumeratedParameter::e_content);
			((PASN_Integer&)item->m_parameters[0].m_id) = 1;  //IncomingCallIndication
			item->m_parameters[0].m_content.SetTag(H225_Content::e_raw);

			PASN_OctetString raw;
			raw.EncodeSubType(inCallInd);
			(PASN_OctetString&)item->m_parameters[0].m_content = raw;

			sci.IncludeOptionalField(H225_ServiceControlIndication::e_genericData);
			sci.m_genericData.Append(item);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			asio::error_code ec;
			return socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()),ep,0,ec);
		}

		bool gk_server::service_control_indication(
			asio::ip::udp::socket& socket,
			const PGloballyUniqueID& call_id,
			const PGloballyUniqueID& conf_id,
			const PString& signal_address, int signal_port,
			const asio::ip::udp::endpoint& ep, H225_RasMessage& rsp)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_serviceControlIndication);
			H225_ServiceControlIndication& sci = (H225_ServiceControlIndication&)msg;
			sci.m_requestSeqNum = request_seq_.fetch_add(1);

			sci.IncludeOptionalField(H225_ServiceControlIndication::e_callSpecific);
			sci.m_callSpecific.m_callIdentifier.m_guid = call_id;
			sci.m_callSpecific.m_conferenceID = conf_id;


			H225_GenericData* item = new H225_GenericData();
			item->m_id.SetTag(H225_GenericIdentifier::e_standard);
			item->IncludeOptionalField(H225_GenericData::e_parameters);
			(PASN_Integer&)item->m_id = 18; // Signalling Traversal

			H46018_IncomingCallIndication inCallInd;

			inCallInd.m_callID.m_guid = call_id;
			inCallInd.m_callSignallingAddress.SetTag(H225_TransportAddress::e_ipAddress);
			h323_util::h225_transport_address_set(inCallInd.m_callSignallingAddress, PIPSocket::Address(signal_address), signal_port);

			item->m_parameters[0].SetTag(H225_EnumeratedParameter::e_content);
			item->m_parameters[0].m_id.SetTag(H225_GenericIdentifier::e_standard);
			item->m_parameters[0].IncludeOptionalField(H225_EnumeratedParameter::e_content);
			((PASN_Integer&)item->m_parameters[0].m_id) = 1;  //IncomingCallIndication
			item->m_parameters[0].m_content.SetTag(H225_Content::e_raw);

			PASN_OctetString raw;
			raw.EncodeSubType(inCallInd);
			(PASN_OctetString&)item->m_parameters[0].m_content = raw;

			sci.IncludeOptionalField(H225_ServiceControlIndication::e_genericData);
			sci.m_genericData.Append(item);

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();

			
			uint32_t seq=sci.m_requestSeqNum;
			std::string sseq = std::to_string(seq);
			echo_.set(sseq, msg);
			asio::error_code ec;
			for (int i = 0; i < 3; i++)
			{
				socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()),ep,0,ec);
				if (echo_.try_get(sseq, msg, rsp, 1000))
				{
					return true;
				}

			}
			echo_.remove(sseq);

			return false;
		}

		bool gk_server::send_info_request(
			asio::ip::udp::socket& socket,
			uint32_t seq,
			const PGloballyUniqueID& call_id,
			const uint32_t call_ref,
			const asio::ip::udp::endpoint& ep)
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
			asio::error_code ec;
			return socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()),ep,0,ec);
		}

		bool gk_server::send_info_request_ack(
			asio::ip::udp::socket& socket,
			uint32_t seq,
			const PString& signal_address, int signal_port,
			const asio::ip::udp::endpoint& ep)
		{
			H225_RasMessage msg;
			msg.SetTag(H225_RasMessage::e_infoRequestAck);
			H225_InfoRequestAck& ack = (H225_InfoRequestAck&)msg;
			ack.m_requestSeqNum = seq;

			PPER_Stream stream;
			msg.Encode(stream);
			stream.CompleteEncoding();
			asio::error_code ec;
			return socket.send_to(asio::buffer(stream.GetPointer(), stream.GetSize()), ep,0,ec);
		}


		bool gk_server::check_auth(const H225_ArrayOf_CryptoH323Token& tokens, const PString& password)
		{
			for (PINDEX i = 0; i < tokens.GetSize(); i++)
			{
				auto& token = tokens[i];
				if (token.GetTag() == H225_CryptoH323Token::e_cryptoEPPwdHash)
				{
					const H225_CryptoH323Token_cryptoEPPwdHash& hash = (const H225_CryptoH323Token_cryptoEPPwdHash&)token;
					if (check_auth_md5(hash,password))
						return true;
				}
			}
			return false;
		}

		bool gk_server::check_auth_md5(const H225_CryptoH323Token_cryptoEPPwdHash& hash,const PString& password)
		{
			PString alias = H323GetAliasAddressString(hash.m_alias);
			if (password.IsEmpty())
				return true;

			PMessageDigest5::Code digest;
			h323_util::make_md5_hash(alias, password, hash.m_timeStamp, digest);

			if (hash.m_token.m_hash.GetData() != digest)
				return false;
			return true;
		}

		bool gk_server::set_reg_endpoint(reg_endpoint& endpoint)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			endpoints_[endpoint.id()] = endpoint;
			return true;
		}

		bool gk_server::get_reg_endpoint(const std::string& id, reg_endpoint& endpoint)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			auto itr=endpoints_.find(id);
			if (itr == endpoints_.end())
				return false;
			endpoint = itr->second;
			return true;
		}

		bool gk_server::get_reg_endpoint(const PStringArray& aliases, reg_endpoint& endpoint)
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			for (auto itr = endpoints_.begin(); itr != endpoints_.end(); itr++)
			{
				for (PINDEX i = 0; i < itr->second.aliases.GetSize(); i++)
				{
					const PString& itr_alias = itr->second.aliases[i];
					for (PINDEX j = 0; j < aliases.GetSize(); j++)
					{
						const PString& alias = aliases[j];
						if (alias == itr_alias)
						{
							endpoint = itr->second;
							return true;
						}
					}
				}
				
			}

			return false;
		}

		bool gk_server::remove_reg_endpoint(const std::string& id)
		{
			reg_endpoint ep;
			bool bfind = false;
			{
				std::lock_guard<std::recursive_mutex> lk(mutex_);
				auto itr = endpoints_.find(id);
				if (itr == endpoints_.end())
					return false;
				ep = itr->second;
				bfind = true;
				endpoints_.erase(itr);
			}
			if (bfind)
			{
				if (on_unreg_endpoint)
				{
					auto self = shared_from_this();
					on_unreg_endpoint(self, ep);
				}
			}
			return true;
		}

		void gk_server::clear_reg_endpoint()
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			endpoints_.clear();
		}

		void gk_server::all_reg_endpoint(std::vector<reg_endpoint>& endpoints)
		{
			endpoints.clear();
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			endpoints.reserve(endpoints_.size());

			for (auto itr = endpoints_.begin(); itr != endpoints_.end(); itr++)
			{
				endpoints.push_back(itr->second);
			}
		}

		size_t gk_server::clear_reg_endpoints()
		{
			std::lock_guard<std::recursive_mutex> lk(mutex_);
			return endpoints_.size();
		}


		bool gk_server::require_aliases_password(const PStringArray& aliases, PString& password)
		{
			for (PINDEX i = 0; i < aliases.GetSize(); i++)
			{
				PString alias = aliases[i];
				bool ok = true;
				std::string pwd;
				if (on_require_password)
				{
					on_require_password(voip::call_type_t::h323, (const std::string&)alias, ok, pwd);
				}
				if (ok)
				{
					password = pwd;
					return true;
				}
			}
			return false;
		}

	}
}
