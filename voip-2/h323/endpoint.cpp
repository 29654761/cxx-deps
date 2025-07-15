#include "endpoint.h"
#include "connection.h"
#include "../local/endpoint.h"
#include <h323/h323pdu.h>

namespace voip
{
    namespace h323
    {
        endpoint::endpoint(manager& mgr)
            :H323EndPoint(mgr)
        {
            SetDefaultH239Control(false);

            m_capabilities.RemoveAll();
            auto fmts = mgr.media_formats();
            for (PINDEX i = 0; i < fmts.GetSize(); i++)
            {
                this->m_capabilities.AddMediaFormat(0, i, fmts[i], H323Capability::e_ReceiveAndTransmit);
            }



        }

        endpoint::~endpoint()
        {

        }

        H323Connection* endpoint::CreateConnection(
            OpalCall& call,                         ///<  Call object to attach the connection to
            const PString& token,                   ///<  Call token for new connection
            void* userData,                         ///<  Arbitrary user data from MakeConnection
            OpalTransport& transport,               ///<  Transport for connection
            const PString& alias,                   ///<  Alias for outgoing call
            const H323TransportAddress& address,    ///<  Address for outgoing call
            H323SignalPDU* setupPDU,                ///<  Setup PDU for incoming call
            unsigned options,
            OpalConnection::StringOptions* stringOptions ///<  complex string options
        )
        {
            return new h323::connection(call, *this, token, alias, address, options, stringOptions);
        }
        /*
        H235Authenticators endpoint::CreateAuthenticators()
        {
            H235Authenticators authenticators;

            PFactory<H235Authenticator>::KeyList_T keyList = PFactory<H235Authenticator>::GetKeyList();
            for (PFactory<H235Authenticator>::KeyList_T::const_iterator it = keyList.begin(); it != keyList.end(); ++it) {
                if ((*it) == "MD5")
                    continue;
                H235Authenticator* auth = PFactory<H235Authenticator>::CreateInstance(*it);
                if (auth->GetApplication() == H235Authenticator::GKAdmission
                    || auth->GetApplication() == H235Authenticator::AnyApplication)
                    authenticators.Append(auth);
                else
                    delete auth;
            }

            return authenticators;
        }
        */


        void endpoint::NewIncomingConnection(OpalListener& listener, const OpalTransportPtr& transport)
        {
            if (transport == NULL)
                return;

            PBoolean reused = false;

            PTRACE(4, "H225\tAwaiting first PDU on " << (reused ? "reused" : "initial") << " connection " << *transport);
            transport->SetReadTimeout(GetFirstSignalPduTimeout());

            H323SignalPDU pdu;
            do {
                if (!pdu.Read(*transport)) {
                    if (reused) {
                        PTRACE(3, "H225\tReusable TCP connection not reused.");
                        transport->Close();
                        return;
                    }

                    PTRACE(2, "H225\tFailed to get initial Q.931 PDU, connection not started.");
                    return;
                }
            } while (!(pdu.GetQ931().GetMessageType() == Q931::SetupMsg || pdu.GetQ931().GetMessageType() == Q931::FacilityMsg));


            unsigned callReference = pdu.GetQ931().GetCallReference();
            PTRACE(3, "H225\tIncoming call, first PDU: callReference=" << callReference
                << " on " << (reused ? "reused" : "initial") << " connection " << *transport);

            // Get a new (or old) connection from the endpoint, calculate token
            PString token = transport->GetRemoteAddress();
            token.sprintf("/%u", callReference);

            PSafePtr<H323Connection> connection = PSafePtrCast<OpalConnection, H323Connection>(GetConnectionWithLock(token, PSafeReadWrite));
            if (connection == NULL) {
                // Get new instance of a call, abort if none created
                OpalCall* call = NULL;
                if (pdu.GetQ931().GetMessageType() == Q931::FacilityMsg)
                {
                    H225_GloballyUniqueID callId = ((H225_Facility_UUIE&)pdu.m_h323_uu_pdu.m_h323_message_body).m_callIdentifier.m_guid;
                    manager& mgr = (manager&)m_manager;
                    call = mgr.get_calling_map().pop((PString)callId);
                    if (!call)
                    {
                        return;
                    }
                }
                else 
                {
                    call = m_manager.InternalCreateCall();
                }
                if (call != NULL) {
                    PTRACE_CONTEXT_ID_SET(*PThread::Current(), call);
                    connection = CreateConnection(*call, token, NULL, *transport, PString::Empty(), PString::Empty(), &pdu);
                    PTRACE(3, "H323\tCreated new connection: " << token);
                }
            }
            else {
                PTRACE_CONTEXT_ID_SET(*PThread::Current(), connection);
            }

            // Make sure transport is attached before AddConnection()
            if (connection != NULL) {
                m_reusableTransportMutex.Wait();
                m_reusableTransports.erase(transport);
                m_reusableTransportMutex.Signal();
                connection->AttachSignalChannel(token, transport, true);
            }

            if (AddConnection(connection) != NULL && connection->HandleSignalPDU(pdu)) {
                m_connectionsByCallId.SetAt(connection->GetIdentifier(), connection);
                // All subsequent PDU's should wait forever
                transport->SetReadTimeout(PMaxTimeInterval);

                if (pdu.GetQ931().GetMessageType() == Q931::FacilityMsg)
                {
                    auto addr = connection->GetRemoteAddress();
                    h323::connection* h323con = connection.GetObjectAs<h323::connection>();
                    if (h323con) {
                        //H225_GloballyUniqueID callId=((H225_Facility_UUIE&)pdu.m_h323_uu_pdu.m_h323_message_body).m_callIdentifier.m_guid;
                        h323con->SendSignalSetup2("", addr);
                    }

                }

                connection->HandleSignallingChannel();
                return;
            }

            PTRACE(1, "H225\tEndpoint could not create connection, "
                "sending release complete PDU: callRef=" << callReference);

            H323SignalPDU releaseComplete;
            Q931& q931PDU = releaseComplete.GetQ931();
            q931PDU.BuildReleaseComplete(callReference, true);
            releaseComplete.m_h323_uu_pdu.m_h323_message_body.SetTag(H225_H323_UU_PDU_h323_message_body::e_releaseComplete);

            H225_ReleaseComplete_UUIE& release = releaseComplete.m_h323_uu_pdu.m_h323_message_body;
            release.m_protocolIdentifier.SetValue(psprintf("0.0.8.2250.0.%u", H225_PROTOCOL_VERSION));

            H225_Setup_UUIE& setup = pdu.m_h323_uu_pdu.m_h323_message_body;
            if (setup.HasOptionalField(H225_Setup_UUIE::e_callIdentifier)) {
                release.IncludeOptionalField(H225_Setup_UUIE::e_callIdentifier);
                release.m_callIdentifier = setup.m_callIdentifier;
            }

            // Set the cause value
            q931PDU.SetCause(Q931::TemporaryFailure);

            // Send the PDU
            releaseComplete.Write(*transport);
        }
        /*
        void endpoint::NewIncomingConnection(OpalListener& listener, const OpalTransportPtr& transport)
        {
            if (transport == NULL)
                return;

            PBoolean reused = false;

            PTRACE(4, "H225\tAwaiting first PDU on " << (reused ? "reused" : "initial") << " connection " << *transport);
            transport->SetReadTimeout(GetFirstSignalPduTimeout());

            H323SignalPDU pdu;
            do {
                if (!pdu.Read(*transport)) {
                    if (reused) {
                        PTRACE(3, "H225\tReusable TCP connection not reused.");
                        transport->Close();
                        return;
                    }

                    PTRACE(2, "H225\tFailed to get initial Q.931 PDU, connection not started.");
                    return;
                }
            } while (!(pdu.GetQ931().GetMessageType() == Q931::SetupMsg|| pdu.GetQ931().GetMessageType()==Q931::FacilityMsg));

            
            unsigned callReference = pdu.GetQ931().GetCallReference();
            PTRACE(3, "H225\tIncoming call, first PDU: callReference=" << callReference
                << " on " << (reused ? "reused" : "initial") << " connection " << *transport);

            // Get a new (or old) connection from the endpoint, calculate token
            PString token = transport->GetRemoteAddress();
            token.sprintf("/%u", callReference);

            PSafePtr<H323Connection> connection = PSafePtrCast<OpalConnection, H323Connection>(GetConnectionWithLock(token, PSafeReadWrite));
            if (connection == NULL) {
                // Get new instance of a call, abort if none created
                OpalCall* call = m_manager.InternalCreateCall();
                if (call != NULL) {
                    PTRACE_CONTEXT_ID_SET(*PThread::Current(), call);
                    connection = CreateConnection(*call, token, NULL, *transport, PString::Empty(), PString::Empty(), &pdu);
                    PTRACE(3, "H323\tCreated new connection: " << token);
                }
            }
            else {
                PTRACE_CONTEXT_ID_SET(*PThread::Current(), connection);
            }

            // Make sure transport is attached before AddConnection()
            if (connection != NULL) {
                m_reusableTransportMutex.Wait();
                m_reusableTransports.erase(transport);
                m_reusableTransportMutex.Signal();
                connection->AttachSignalChannel(token, transport, true);
            }

            if (AddConnection(connection) != NULL && connection->HandleSignalPDU(pdu)) {
                m_connectionsByCallId.SetAt(connection->GetIdentifier(), connection);
                // All subsequent PDU's should wait forever
                transport->SetReadTimeout(PMaxTimeInterval);

                if (pdu.GetQ931().GetMessageType() == Q931::FacilityMsg)
                {
                    auto localep=m_manager.FindEndPointAs<local::endpoint>("local");
                    if (localep)
                    {
                        PStringList aliases=connection->GetLocalAliasNames();
                        for (PINDEX i = 0; i < aliases.GetSize(); i++)
                        {
                            PString a=aliases[i];
                        }
                        PString local_party="local:";
                        if (aliases.GetSize() > 0)
                            local_party += aliases[0];
                        else
                            local_party += "*";
                        connection->GetCall().SetPartyB(local_party);
                        auto localcon=localep->MakeConnection(connection->GetCall(), local_party, nullptr, 0, nullptr);
                    }
                    
                    auto addr = connection->GetRemoteAddress();
                    h323::connection* h323con=connection.GetObjectAs<h323::connection>();
                    if (h323con) {
                        //H225_GloballyUniqueID callId=((H225_Facility_UUIE&)pdu.m_h323_uu_pdu.m_h323_message_body).m_callIdentifier.m_guid;
                        h323con->SendSignalSetup2("", addr);
                    }
                    
                }

                connection->HandleSignallingChannel();
                return;
            }

            PTRACE(1, "H225\tEndpoint could not create connection, "
                "sending release complete PDU: callRef=" << callReference);

            H323SignalPDU releaseComplete;
            Q931& q931PDU = releaseComplete.GetQ931();
            q931PDU.BuildReleaseComplete(callReference, true);
            releaseComplete.m_h323_uu_pdu.m_h323_message_body.SetTag(H225_H323_UU_PDU_h323_message_body::e_releaseComplete);

            H225_ReleaseComplete_UUIE& release = releaseComplete.m_h323_uu_pdu.m_h323_message_body;
            release.m_protocolIdentifier.SetValue(psprintf("0.0.8.2250.0.%u", H225_PROTOCOL_VERSION));

            H225_Setup_UUIE& setup = pdu.m_h323_uu_pdu.m_h323_message_body;
            if (setup.HasOptionalField(H225_Setup_UUIE::e_callIdentifier)) {
                release.IncludeOptionalField(H225_Setup_UUIE::e_callIdentifier);
                release.m_callIdentifier = setup.m_callIdentifier;
            }

            // Set the cause value
            q931PDU.SetCause(Q931::TemporaryFailure);

            // Send the PDU
            releaseComplete.Write(*transport);
        }
        */

        void endpoint::OnGatekeeperStatus(
            H323Gatekeeper& gk,
            H323Gatekeeper::RegistrationFailReasons status
        )
        {
            manager& mgr=(manager&)m_manager;
            mgr.OnH323GatekeeperStatus(gk, status);
        }
    }

}

