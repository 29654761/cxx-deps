#pragma once
#include <h323/h323con.h>

namespace voip
{
	namespace h323
	{
		class connection :public H323Connection
		{
			PCLASSINFO(connection, H323Connection);
		public:
			connection(
				OpalCall& call,                         ///<  Call object connection belongs to
				H323EndPoint& endpoint,                 ///<  H323 End Point object
				const PString& token,                   ///<  Token for new connection
				const PString& alias,                   ///<  Alias for outgoing call
				const H323TransportAddress& address,    ///<  Address for outgoing call
				unsigned options = 0,                    ///<  Connection option bits
				OpalConnection::StringOptions* stringOptions = NULL ///<  complex string options
			);
			~connection();

			//void SetConferenceIdentifier(const OpalGloballyUniqueID& id){
			//	m_conferenceIdentifier = id;
			//}
			//OpalGloballyUniqueID GetConferenceIdentifier()const { return m_conferenceIdentifier; }

			

			virtual OpalMediaStream* CreateMediaStream(
				const OpalMediaFormat& mediaFormat, ///<  Media format for stream
				unsigned sessionID,                  ///<  Session number for stream
				PBoolean isSource                        ///<  Is a source stream
			);

			virtual OpalMediaStreamPtr OpenMediaStream(
				const OpalMediaFormat& mediaFormat, ///<  Media format to open
				unsigned sessionID,                  ///<  Session to start stream on
				bool isSource                        ///< Stream is a source/sink
			);

			virtual void AdjustMediaFormats(
				bool local,                             ///<  Media formats a local ones to be presented to remote
				const OpalConnection* otherConnection, ///<  Other connection we are adjusting media for
				OpalMediaFormatList& mediaFormats      ///<  Media formats to use
			) const;

			virtual PBoolean OpenLogicalChannel(
				const H323Capability& capability,  ///<  Capability to open channel with
				unsigned sessionID,                 ///<  Session for the channel
				H323Channel::Directions dir         ///<  Direction of channel
			);

			virtual void OnSetLocalCapabilities();


			virtual CallEndReason SendSignalSetup2(
				const PString& alias,                ///<  Name of remote party
				const H323TransportAddress& address  ///<  Address of destination
			);
		};


	}
}

