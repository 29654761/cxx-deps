#pragma once

#include "../manager.h"
#include <h323/h323ep.h>

namespace voip
{
    namespace h323
    {
        class endpoint :public H323EndPoint
        {
            PCLASSINFO(endpoint, H323EndPoint)
        public:
            endpoint(manager& mgr);
            ~endpoint();

            virtual H323Connection* CreateConnection(
                OpalCall& call,                         ///<  Call object to attach the connection to
                const PString& token,                   ///<  Call token for new connection
                void* userData,                         ///<  Arbitrary user data from MakeConnection
                OpalTransport& transport,               ///<  Transport for connection
                const PString& alias,                   ///<  Alias for outgoing call
                const H323TransportAddress& address,    ///<  Address for outgoing call
                H323SignalPDU* setupPDU,                ///<  Setup PDU for incoming call
                unsigned options = 0,
                OpalConnection::StringOptions* stringOptions = NULL ///<  complex string options
            );

            virtual void NewIncomingConnection(
                OpalListener& listener,            ///<  Listner that created transport
                const OpalTransportPtr& transport  ///<  Transport connection came in on
            );

            //virtual H235Authenticators CreateAuthenticators();

            virtual void OnGatekeeperStatus(
                H323Gatekeeper& gk,
                H323Gatekeeper::RegistrationFailReasons status
            );
        };

    }
}
