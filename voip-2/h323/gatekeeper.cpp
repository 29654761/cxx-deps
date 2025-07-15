#include "gatekeeper.h"
#include "registrar.h"
#include "gatekeeper_listener.h"
#include "registrar.h"

#include <opal.h>

namespace voip
{
	namespace h323
	{
		gatekeeper::gatekeeper(H323EndPoint& endpoint) :H323GatekeeperServer(endpoint)
		{
			//SetAnswerCallPreGrantedARQ(true);
			//SetMakeCallPreGrantedARQ(true);
		}

		gatekeeper::~gatekeeper()
		{
		}

		PBoolean gatekeeper::GetUsersPassword(
			const PString& alias,
			PString& password,
			H323RegisteredEndPoint& registeredEndpoint
		) const
		{
			manager& mgr = dynamic_cast<manager&>(this->GetOwnerEndPoint().GetManager());
			return mgr.OnRequirePassword(OPAL_PREFIX_H323,alias,password);
		}

		H323GatekeeperRequest::Response gatekeeper::OnDiscovery(H323GatekeeperGRQ& request)
		{
			return H323GatekeeperServer::OnDiscovery(request);
		}

		H323GatekeeperRequest::Response gatekeeper::OnRegistration(H323GatekeeperRRQ& request)
		{

			H323GatekeeperRequest::Response rsp = H323GatekeeperServer::OnRegistration(request);
			if (rsp == H323GatekeeperRequest::Response::Confirm)
			{
				manager& mgr = dynamic_cast<manager&>(this->GetOwnerEndPoint().GetManager());

				PString name;
				if (request.rrq.m_terminalAlias.GetSize() > 0)
				{
					name= H323GetAliasAddressString(request.rrq.m_terminalAlias[0]);
					if (!mgr.OnRegistration(OPAL_PREFIX_H323, name))
					{
						return H323GatekeeperRequest::Response::Reject;
					}
				}
				else
				{
					if (request.m_endpoint != NULL)
					{
						name = request.m_endpoint->GetAlias(0);
						if (!mgr.OnRegistration(OPAL_PREFIX_H323, name))
						{
							return H323GatekeeperRequest::Response::Reject;
						}
					}
					else
					{
						auto ep = FindEndPointBySignalAddresses(request.rrq.m_callSignalAddress);
						if (ep && ep->GetAliasCount() > 0)
						{
							name = ep->GetAlias(0);
							if (!mgr.OnRegistration(OPAL_PREFIX_H323, name))
							{
								return H323GatekeeperRequest::Response::Reject;
							}
						}
					}
				}
			}

			return rsp;
		}

		H323GatekeeperRequest::Response gatekeeper::OnUnregistration(H323GatekeeperURQ& request)
		{
			return H323GatekeeperServer::OnUnregistration(request);
		}

		H323GatekeeperRequest::Response gatekeeper::OnInfoResponse(H323GatekeeperIRR& request)
		{
			return H323GatekeeperServer::OnInfoResponse(request);
		}

		H323GatekeeperRequest::Response gatekeeper::OnAdmission(H323GatekeeperARQ& request)
		{
			//adapt for keda
			request.arq.IncludeOptionalField(H225_AdmissionRequest::e_srcCallSignalAddress);

			return H323GatekeeperServer::OnAdmission(request);
		}

		H323GatekeeperRequest::Response gatekeeper::OnDisengage(H323GatekeeperDRQ& request)
		{
			return H323GatekeeperServer::OnDisengage(request);
		}


		H323RegisteredEndPoint* gatekeeper::CreateRegisteredEndPoint(H323GatekeeperRequest& request)
		{
			registrar* reg = new h323::registrar(*this, this->CreateEndPointIdentifier(), request);
			//return H323GatekeeperServer::CreateRegisteredEndPoint(request);
			return reg;
		}

		PBoolean gatekeeper::RemoveEndPoint(H323RegisteredEndPoint* ep)
		{
			if (ep->GetAliasCount() > 0) 
			{
				PString name = ep->GetAlias(0);
				manager& mgr = dynamic_cast<manager&>(this->GetOwnerEndPoint().GetManager());
				mgr.OnUnregistration("h323", name);
			}
			PBoolean r = H323GatekeeperServer::RemoveEndPoint(ep);
			return r;
		}

		PSafePtr<H323RegisteredEndPoint> gatekeeper::FindEndPointByIdentifier(
			const PString& identifier, PSafetyMode mode)
		{
			PSafePtr<H323RegisteredEndPoint> ep = m_byIdentifier.Find(identifier, mode);
			if (ep) {
				for (PINDEX i = 0; i < ep->GetAliasCount(); i++)
				{
					PString alias = ep->GetAlias(i);
				}
			}
			return ep;
		}


		H323Transactor* gatekeeper::CreateListener(H323Transport* transport)
		{
			return new gatekeeper_listener(m_ownerEndPoint, *this, m_gatekeeperIdentifier, transport);
			//return H323GatekeeperServer::CreateListener(transport);
		}

		PBoolean gatekeeper::OnSendFeatureSet(H460_MessageType messageType, H225_FeatureSet& featureSet) const
		{
			if (messageType != H460_MessageType::e_serviceControlIndication)
			{
				featureSet.m_replacementFeatureSet = true;
				featureSet.IncludeOptionalField(H225_FeatureSet::e_supportedFeatures);
				H225_FeatureDescriptor* desc = new H225_FeatureDescriptor();
				(PASN_Integer&)desc->m_id.GetObject() = 18;

				featureSet.m_supportedFeatures.Append(desc);
				return true;
			}
			
			return false;
		}

	}
}


