#include "gatekeeper_listener.h"
#include "../manager.h"
#include "registrar.h"
#include <sys2/util.h>
#include <h460/h46018.h>

namespace voip
{
	namespace h323
	{

		gatekeeper_listener::gatekeeper_listener(
			H323EndPoint& endpoint,               ///<  Local endpoint
			H323GatekeeperServer& server,         ///<  Database for gatekeeper
			const PString& gatekeeperIdentifier,  ///<  Name of this gatekeeper
			H323Transport* transport       ///<  Transport over which gatekeepers communicates.
		):H323GatekeeperListener(endpoint,server,gatekeeperIdentifier,transport)
		{
		}

		gatekeeper_listener::~gatekeeper_listener()
		{
		}


		PBoolean gatekeeper_listener::ServiceControlIndication(
			H323RegisteredEndPoint& ep,
			const H323ServiceControlSession& session,
			H323GatekeeperCall* call
		)
		{
			OpalGloballyUniqueID callId = NULL, confId=NULL;
			if (call != NULL)
			{
				callId = call->GetCallIdentifier();
				confId = call->GetConferenceIdentifier();
			}
			
			H323RasPDU pdu(ep.GetAuthenticators());
			H225_ServiceControlIndication& sci = pdu.BuildServiceControlIndication(GetNextSequenceNumber(), &callId);
			sci.m_callSpecific.m_conferenceID = (H225_GloballyUniqueID)(*confId);
			sci.m_callSpecific.m_answeredCall = true;

			H225_ArrayOf_GenericData genericDataItems;
			H225_GenericData* item = new H225_GenericData(H225_GenericData::e_parameters);
			item->m_id.SetTag(H225_GenericIdentifier::e_standard);
			item->IncludeOptionalField(H225_GenericData::e_parameters);
			(PASN_Integer&)item->m_id.GetObject() = 18; // Signalling Traversal


			H225_ArrayOf_TransportAddress addresses;
			if (!SetUpCallSignalAddresses(addresses))
			{
				return false;
			}

			H225_TransportAddress addr = addresses[0];
			manager& mgr = (manager&)this->m_endpoint.GetManager();
			PString natAddr = mgr.get_nat_address();
			if (!natAddr.IsEmpty())
			{
				PIPSocket::Address natIp = (PIPSocket::Address)natAddr;
				((H225_TransportAddress_ipAddress&)addr).m_ip.SetValue((const BYTE*)natIp.GetPointer(), natIp.GetSize());
			}

			/*
			auto listeners = this->GetGatekeeper().GetOwnerEndPoint().GetListeners();
			for (PINDEX i = 0; i < listeners.GetSize(); i++)
			{
				auto& listener = listeners[i];
				PString prefix = listener.GetProtoPrefix();
				if (prefix != OpalTransportAddress::TcpPrefix())
					continue;

				auto address = listener.GetLocalAddress();
				if (address.GetIpAndPort(ip, port))
					break;
			}

			manager& mgr = (manager&)this->GetGatekeeper().GetOwnerEndPoint().GetManager();
			PString address = mgr.get_nat_address();
			if (address.IsEmpty())
			{
				auto addrs = this->GetGatekeeper().GetOwnerEndPoint().GetInterfaceAddresses(m_transport);
				for (PINDEX i = 0; i < addrs.GetSize(); i++)
				{
					auto& addr = addrs[i];
					if (addr.GetProtoPrefix() != OpalTransportAddress::TcpPrefix())
						continue;
					PIPSocket::Address ip2;
					if (!addr.GetIpAddress(ip2))
						continue;

					if (ip2.IsAny() || ip2.IsLoopback() || ip2.IsBroadcast() || ip2.IsLinkLocal() || ip2.IsMulticast())
						continue;

					ip = ip2;
					break;
				}
			}
			else
			{
				ip = address;
			}
			*/

			H46018_IncomingCallIndication inCallInd;

			inCallInd.m_callID.m_guid = (H225_GloballyUniqueID)(*callId);
			inCallInd.m_callSignallingAddress.SetTag(H225_TransportAddress::e_ipAddress);
			inCallInd.m_callSignallingAddress = addr;
			//((H225_TransportAddress_ipAddress&)(inCallInd.m_callSignallingAddress)).m_ip.SetValue((const BYTE*)ip.GetPointer(), ip.GetSize());
			//((H225_TransportAddress_ipAddress&)(inCallInd.m_callSignallingAddress)).m_port = 1720;

			item->m_parameters[0].SetTag(H225_EnumeratedParameter::e_content);
			item->m_parameters[0].m_id.SetTag(H225_GenericIdentifier::e_standard);
			item->m_parameters[0].IncludeOptionalField(H225_EnumeratedParameter::e_content);
			(PASN_Integer&)item->m_parameters[0].m_id.GetObject() = 1;  //IncomingCallIndication
			item->m_parameters[0].m_content.SetTag(H225_Content::e_raw);

			PASN_OctetString raw;
			raw.EncodeSubType(inCallInd);
			(PASN_OctetString&)item->m_parameters[0].m_content = raw;

			genericDataItems.Append(item);

			sci.IncludeOptionalField(H225_ServiceControlIndication::e_genericData);
			sci.m_genericData = genericDataItems;

			ep.AddServiceControlSession(session, sci.m_serviceControl);
			H225_RAS::Request request(sci.m_requestSeqNum, pdu, ep.GetRASAddresses());
			return MakeRequest(request);

		}

		PBoolean gatekeeper_listener::ServiceControlIndication(
			H323RegisteredEndPoint& ep,
			const H323ServiceControlSession& session,
			const OpalGloballyUniqueID* callId,
			const OpalGloballyUniqueID* confId
		)
		{
			//registrar& reg = (registrar&)ep;
			H323RasPDU pdu(ep.GetAuthenticators());
			H225_ServiceControlIndication& sci = pdu.BuildServiceControlIndication(GetNextSequenceNumber(), callId);
			sci.m_callSpecific.m_conferenceID = *confId;
			sci.m_callSpecific.m_answeredCall = true;
			

			//sci.IncludeOptionalField(H225_ServiceControlIndication::e_endpointIdentifier);
			//sci.m_endpointIdentifier = ep.GetIdentifier();


			
			H225_ArrayOf_GenericData genericDataItems;
			H225_GenericData* item=new H225_GenericData(H225_GenericData::e_parameters);
			item->m_id.SetTag(H225_GenericIdentifier::e_standard);
			item->IncludeOptionalField(H225_GenericData::e_parameters);
			(PASN_Integer&)item->m_id.GetObject()= 18; // Signalling Traversal



			/*
			PIPSocket::Address ip;
			WORD port = 1720;
			
			auto listeners=this->GetGatekeeper().GetOwnerEndPoint().GetListeners();
			for (PINDEX i = 0; i < listeners.GetSize(); i++)
			{
				auto& listener = listeners[i];
				PString prefix = listener.GetProtoPrefix();
				if (prefix != OpalTransportAddress::TcpPrefix())
					continue;
				
				auto address=listener.GetLocalAddress();
				if (address.GetIpAndPort(ip, port))
					break;
			}
			
			manager& mgr = (manager&)this->GetGatekeeper().GetOwnerEndPoint().GetManager();
			PString address = mgr.get_nat_address();
			printf("=============>call reg h323 get nat ip=%s\n", ((PString)address).c_str());
			if (address.IsEmpty())
			{
				auto addrs = this->GetGatekeeper().GetOwnerEndPoint().GetInterfaceAddresses(m_transport);
				for (PINDEX i = 0; i < addrs.GetSize(); i++)
				{
					auto& addr = addrs[i];
					if (addr.GetProtoPrefix() != OpalTransportAddress::TcpPrefix())
						continue;
					PIPSocket::Address ip2;
					if (!addr.GetIpAddress(ip2))
						continue;

					if (ip2.IsAny() || ip2.IsLoopback() || ip2.IsBroadcast() || ip2.IsLinkLocal() || ip2.IsMulticast())
						continue;

					ip = ip2;
					break;
				}
				printf("=============>call reg h323 use local ip=%s\n", ((PString)ip).c_str());
			}
			else
			{
				ip = address;
				printf("=============>call reg h323 use nat ip=%s\n", ((PString)address).c_str());
			}
			printf("=============>call reg h323. ip=%s\n",((PString)ip).c_str());
			*/

			H225_ArrayOf_TransportAddress addresses;
			if (!SetUpCallSignalAddresses(addresses))
			{
				return false;
			}

			H225_TransportAddress addr = addresses[0];
			manager& mgr = (manager&)this->m_endpoint.GetManager();
			PString natAddr = mgr.get_nat_address();
			if (!natAddr.IsEmpty())
			{
				PIPSocket::Address natIp = (PIPSocket::Address)natAddr;
				((H225_TransportAddress_ipAddress&)addr).m_ip.SetValue((const BYTE*)natIp.GetPointer(), natIp.GetSize());
			}

			H46018_IncomingCallIndication inCallInd;
			
			inCallInd.m_callID.m_guid = *callId;
			inCallInd.m_callSignallingAddress.SetTag(H225_TransportAddress::e_ipAddress);
			inCallInd.m_callSignallingAddress = addr;
			//((H225_TransportAddress_ipAddress&)(inCallInd.m_callSignallingAddress)).m_ip.SetValue((const BYTE*)ip.GetPointer(),ip.GetSize());
			//((H225_TransportAddress_ipAddress&)(inCallInd.m_callSignallingAddress)).m_port = port;

			//H225_EnumeratedParameter* pm=new H225_EnumeratedParameter(H225_EnumeratedParameter::e_content);
			item->m_parameters[0].SetTag(H225_EnumeratedParameter::e_content);
			item->m_parameters[0].m_id.SetTag(H225_GenericIdentifier::e_standard);
			item->m_parameters[0].IncludeOptionalField(H225_EnumeratedParameter::e_content);
			(PASN_Integer&)item->m_parameters[0].m_id.GetObject()= 1;  //IncomingCallIndication
			item->m_parameters[0].m_content.SetTag(H225_Content::e_raw);

			PASN_OctetString raw;
			raw.EncodeSubType(inCallInd);
			(PASN_OctetString&)item->m_parameters[0].m_content = raw;
			
			genericDataItems.Append(item);

			sci.IncludeOptionalField(H225_ServiceControlIndication::e_genericData);
			sci.m_genericData = genericDataItems;

			ep.AddServiceControlSession(session, sci.m_serviceControl);
			H225_RAS::Request request(sci.m_requestSeqNum, pdu, ep.GetRASAddresses());
			return MakeRequest(request);
		}


		H323GatekeeperRequest::Response gatekeeper_listener::OnAdmission(
			H323GatekeeperARQ& info
		)
		{
			PTRACE_BLOCK("H323GatekeeperListener::OnAdmission");

			if (!info.CheckGatekeeperIdentifier())
				return H323GatekeeperRequest::Reject;

			if (!info.GetRegisteredEndPoint())
				return H323GatekeeperRequest::Reject;

			if (!info.CheckCryptoTokens()) {
				H235Authenticators adjustedAuthenticators;
				if (!m_gatekeeper.GetAdmissionRequestAuthentication(info, adjustedAuthenticators))
					return H323GatekeeperRequest::Reject;

				PTRACE(3, "RAS\tARQ received with separate credentials: "
					<< setfill(',') << adjustedAuthenticators << setfill(' '));
				info.SetAuthenticators(adjustedAuthenticators);
				if (!info.H323Transaction::CheckCryptoTokens()) {
					PTRACE(2, "RAS\tARQ rejected, alternate security tokens invalid.");

					return H323GatekeeperRequest::Reject;
				}

				if (info.alternateSecurityID.IsEmpty() && !adjustedAuthenticators.IsEmpty())
					info.alternateSecurityID = adjustedAuthenticators.front().GetRemoteId();
			}

			H323GatekeeperRequest::Response response = m_gatekeeper.OnAdmission(info);
			if (response != H323GatekeeperRequest::Confirm)
				return response;

			if (info.acf.m_callModel.GetTag() == H225_CallModel::e_gatekeeperRouted) {
				
				H225_ArrayOf_TransportAddress addresses;
				if (SetUpCallSignalAddresses(addresses))
				{
					H225_TransportAddress addr = addresses[0];

					manager& mgr = (manager&)this->m_endpoint.GetManager();
					PString natAddr = mgr.get_nat_address();
					if (!natAddr.IsEmpty())
					{
						PIPSocket::Address natIp = (PIPSocket::Address)natAddr;
						((H225_TransportAddress_ipAddress&)addr).m_ip.SetValue((const BYTE*)natIp.GetPointer(), natIp.GetSize());
					}

					info.acf.m_destCallSignalAddress = addr;
				}
				
			}

			return H323GatekeeperRequest::Confirm;
		}
	}
}

