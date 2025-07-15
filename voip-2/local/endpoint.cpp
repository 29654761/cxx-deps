#include "endpoint.h"
#include "connection.h"

namespace voip
{
	namespace local
	{
		endpoint::endpoint(manager& mgr) :OpalLocalEndPoint(mgr)
		{
			SetDefaultAudioSynchronicity(e_Asynchronous);
			SetDefaultVideoSourceSynchronicity(e_Asynchronous);
		}

		endpoint::~endpoint()
		{
		}

		OpalLocalConnection* endpoint::CreateConnection(OpalCall& call, void* userData, unsigned options, OpalConnection::StringOptions* stringOptions)
		{
			return new local::connection(call, *this, userData, options, stringOptions);
		}
	}
}

