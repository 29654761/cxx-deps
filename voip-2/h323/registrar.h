#pragma once
#include "gatekeeper_listener.h"
#include <h323/h323ep.h>
#include <h323/gkserver.h>
#include <shared_mutex>

namespace voip
{
	namespace h323
	{
		class registrar:public H323RegisteredEndPoint
		{
		public:
			registrar(H323GatekeeperServer& server, ///<  Gatekeeper server data
				const PString& id,
				const H323GatekeeperRequest& request);
			~registrar();

			gatekeeper_listener* ras_channel() { return dynamic_cast<gatekeeper_listener*>(m_rasChannel); }

			virtual PBoolean SendServiceControlSession(const H323ServiceControlSession& session,H323GatekeeperCall* call);


		};

	}
}