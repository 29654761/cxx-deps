#include "connection.h"

namespace voip
{
	namespace sip
	{
		connection::connection(const Init& init)
			:SIPConnection(init)
		{

		}

		connection::~connection()
		{

		}

		OpalMediaStream* connection::CreateMediaStream(
			const OpalMediaFormat& mediaFormat, ///<  Media format for stream
			unsigned sessionID,                  ///<  Session number for stream
			PBoolean isSource                        ///<  Is a source stream
		)
		{

			return SIPConnection::CreateMediaStream(mediaFormat, sessionID, isSource);
		}

		void connection::OnReceivedINFO(SIP_PDU& request)
		{
			request.SendResponse(SIP_PDU::Successful_OK);
		}
	}
}



