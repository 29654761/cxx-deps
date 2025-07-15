#pragma once
#include <h323/h323ep.h>
#include <h323/gkserver.h>

namespace voip
{
	namespace h323
	{
		class gatekeeper_listener:public H323GatekeeperListener
		{
			PCLASSINFO(gatekeeper_listener, H323GatekeeperListener);
		public:
			gatekeeper_listener(
				H323EndPoint& endpoint,               ///<  Local endpoint
				H323GatekeeperServer& server,         ///<  Database for gatekeeper
				const PString& gatekeeperIdentifier,  ///<  Name of this gatekeeper
				H323Transport* transport = NULL       ///<  Transport over which gatekeepers communicates.
			);
			~gatekeeper_listener();

			virtual PBoolean ServiceControlIndication(
				H323RegisteredEndPoint& ep,
				const H323ServiceControlSession& session,
				H323GatekeeperCall* call = NULL
			);

			virtual PBoolean ServiceControlIndication(
				H323RegisteredEndPoint& ep,
				const H323ServiceControlSession& session,
				const OpalGloballyUniqueID* callId,
				const OpalGloballyUniqueID* confId
			);

			virtual H323GatekeeperRequest::Response OnAdmission(
				H323GatekeeperARQ& request
			);
		};

	}
}

