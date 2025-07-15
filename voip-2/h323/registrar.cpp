
#include "registrar.h"
#include <sys2/util.h>

namespace voip
{
	namespace h323
	{

		registrar::registrar(H323GatekeeperServer& server, ///<  Gatekeeper server data
			const PString& id,
			const H323GatekeeperRequest& request):
			H323RegisteredEndPoint(server, id)
		{

		}

		registrar::~registrar()
		{
		}



		PBoolean registrar::SendServiceControlSession(const H323ServiceControlSession& session, H323GatekeeperCall* call)
		{
			// Send SCI to endpoint(s)
			if (m_rasChannel == NULL) {
				// Can't do SCI as have no RAS channel to use (probably logic error)
				PAssertAlways("Tried to do SCI to endpoint we did not receive RRQ for!");
				return false;
			}
			this->GetAuthenticators();
			return m_rasChannel->ServiceControlIndication(*this, session, call);
		}


	}
}

