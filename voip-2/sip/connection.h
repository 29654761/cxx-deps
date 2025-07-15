#pragma once
#include <sip/sipcon.h>

namespace voip
{
	namespace sip
	{
		class connection :public SIPConnection
		{
			PCLASSINFO(connection, SIPConnection);
		public:
			connection(const Init& init);
			~connection();

			virtual OpalMediaStream* CreateMediaStream(
				const OpalMediaFormat& mediaFormat, ///<  Media format for stream
				unsigned sessionID,                  ///<  Session number for stream
				PBoolean isSource                        ///<  Is a source stream
			);

			virtual void OnReceivedINFO(SIP_PDU& pdu);
		};



	}
}


