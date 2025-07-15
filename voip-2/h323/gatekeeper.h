#pragma once
#include "endpoint.h"
#include <h323/gkserver.h>

namespace voip
{
	namespace h323
	{

		class gatekeeper :public H323GatekeeperServer
		{
			PCLASSINFO(gatekeeper, H323GatekeeperServer);
		public:
			gatekeeper(H323EndPoint& endpoint);
			~gatekeeper();

			virtual PBoolean GetUsersPassword(
				const PString& alias,
				PString& password,
				H323RegisteredEndPoint& registeredEndpoint
			) const;

			virtual H323GatekeeperRequest::Response OnDiscovery(
				H323GatekeeperGRQ& request
			);

			virtual H323GatekeeperRequest::Response OnRegistration(
				H323GatekeeperRRQ& request
			);

			virtual H323GatekeeperRequest::Response OnUnregistration(
				H323GatekeeperURQ& request
			);

			virtual H323GatekeeperRequest::Response OnInfoResponse(
				H323GatekeeperIRR& request
			);

			virtual H323GatekeeperRequest::Response OnAdmission(
				H323GatekeeperARQ& request
			);

			virtual H323GatekeeperRequest::Response OnDisengage(
				H323GatekeeperDRQ& request
			);


			virtual H323RegisteredEndPoint* CreateRegisteredEndPoint(
				H323GatekeeperRequest& request
			);

			virtual PBoolean RemoveEndPoint(
				H323RegisteredEndPoint* ep
			);

			virtual PSafePtr<H323RegisteredEndPoint> FindEndPointByIdentifier(
				const PString& identifier, PSafetyMode mode);

			virtual H323Transactor* CreateListener(H323Transport* transport);

			virtual PBoolean OnSendFeatureSet(H460_MessageType messageType, H225_FeatureSet& featureSet) const;


			//virtual PBoolean TranslateAliasAddressToSignalAddress(
			//	const H225_AliasAddress& alias,
			//	H323TransportAddress& address
			//);
		};


	}
}



