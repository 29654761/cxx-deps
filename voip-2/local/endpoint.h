#pragma once
#include "../manager.h"
#include <ep/localep.h>

namespace voip
{
	namespace local
	{
		class endpoint :public OpalLocalEndPoint
		{
			PCLASSINFO(endpoint, OpalLocalEndPoint)
		public:
			endpoint(manager& mgr);
			~endpoint();

			virtual OpalLocalConnection* CreateConnection(OpalCall& call, void* userData, unsigned options, OpalConnection::StringOptions* stringOptions);
		};

	}
}


