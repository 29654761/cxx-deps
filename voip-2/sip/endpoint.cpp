#include "endpoint.h"
#include "connection.h"
#include "challenge.h"
#include "credentials.h"
#include "registrar.h"
#include <sys2/util.h>
#include <opal.h>

namespace voip
{
    namespace sip
    {

        endpoint::endpoint(voip::manager& mgr)
            :SIPEndPoint(mgr)
        {

        }

        endpoint::~endpoint()
        {

        }

        SIPConnection* endpoint::CreateConnection(
            const SIPConnection::Init& init     ///< Initialisation parameters
        )
        {
            return new sip::connection(init);
        }

        bool endpoint::OnReceivedINVITE(
            SIP_PDU* pdu
        )
        {
            return SIPEndPoint::OnReceivedINVITE(pdu);
        }

        bool endpoint::OnReINVITE(
            SIPConnection& connection, ///< Connection re-INVITE has occurred on
            bool fromRemote,            ///< This is a reponse to our re-INVITE
            const PString& remoteSDP   ///< SDP remote system sent.
        )
        {
            return false;
        }

        bool endpoint::OnReceivedREGISTER(
            SIP_PDU& request
        )
        {
            SIP_PDU response(request, SIP_PDU::Successful_OK);
            SIPMIMEInfo& mime = request.GetMIME();
            mime.SetRecordRoute(PString::Empty()); // RFC3261/10.3
            
            PString auth = mime.Get("Authorization");
            if (auth.IsEmpty())
            {
                sip::challenge chal;
                chal.nonce = sys::util::random_string(10);
                chal.realm = m_realm.c_str();
                response.GetMIME().SetWWWAuthenticate(chal.to_string());
                response.SetStatusCode(SIP_PDU::Failure_UnAuthorised);
                response.Send();
                return true;
            }

            sip::credentials cred;
            if (!cred.parse(auth))
            {
                response.SetStatusCode(SIP_PDU::Failure_Forbidden);
                response.Send();
                return true;
            }

            PString name = mime.GetFrom().GetUserName();
            PString password;
            manager& mgr = (manager&)m_manager;
            if (!mgr.OnRequirePassword(OPAL_PREFIX_SIP,name, password))
            {
                response.SetStatusCode(SIP_PDU::Failure_Forbidden);
                response.Send();
                return true;
            }
            if (!password.IsEmpty())
            {
                std::string rsp = cred.digest("REGISTER", password, nullptr, "");
                if (cred.response != rsp)
                {
                    response.SetStatusCode(SIP_PDU::Failure_Forbidden);
                    response.Send();
                    return true;
                }
            }

            if (!mime.GetRequire().IsEmpty()) {
                PTRACE(3, "SIP-Reg", "REGISTER required unsupported feature: " << setfill(',') << mime.GetRequire());
                request.SendResponse(SIP_PDU::Failure_BadExtension);
                return true;
            }

            PTRACE(3, "SIP-Reg", "Handling REGISTER: " << mime.GetTo());


            //Force client use server's expires
            SIPURLList contacts;
            int expires=GetRegistrarTimeToLive().GetSeconds();
            mime.GetContacts(contacts, expires);
            for (SIPURLList::iterator itr = contacts.begin(); itr != contacts.end(); itr++)
            {
                itr->SetParamVar("expires", expires);
                mime.SetContact(*itr);
            }
            


            response.SetStatusCode(InternalHandleREGISTER(request, &response));

            if (response.GetStatusCode() == SIP_PDU::Successful_OK) {
                // Private extension for mass registration.
                static const PConstCaselessString AoRListKey("X-OPAL-AoR-List");
                SIPURLList aorList;
                if (aorList.FromString(mime.Get(AoRListKey), SIPURL::ExternalURI)) {
                    mime.Remove(AoRListKey);

                    SIPURLList successList;
                    for (SIPURLList::iterator aor = aorList.begin(); aor != aorList.end(); ++aor) {
                        mime.SetTo(*aor);
                        if (InternalHandleREGISTER(request, NULL))
                            successList.push_back(*aor);
                    }

                    if (!successList.empty())
                        response.GetMIME().Set(AoRListKey, successList.ToString());
                }

                if (!mgr.OnRegistration(OPAL_PREFIX_SIP, name))
                {
                    response.SetStatusCode(SIP_PDU::StatusCodes::Failure_NotFound);
                }

            }

            response.Send();


            

            return true;
        }


        endpoint::RegistrarAoR* endpoint::CreateRegistrarAoR(const SIP_PDU& request)
        {
            return new registrar(request);
            

            //return SIPEndPoint::CreateRegistrarAoR(request);
        }

        void endpoint::OnChangedRegistrarAoR(endpoint::RegistrarAoR& ua)
        {
            if (!ua.HasBindings())
            {
                SIPURL url=ua.GetAoR();
                manager& mgr = (manager&)m_manager;
                mgr.OnUnregistration("sip", url.GetUserName());
            }
        }


        void endpoint::OnRegistrationStatus(
            const PString& aor,
            PBoolean wasRegistering,
            PBoolean reRegistering,
            SIP_PDU::StatusCodes reason
        )
        {
            manager& mgr = (manager&)m_manager;
            mgr.OnSipRegistrationStatus(aor, wasRegistering, reRegistering, reason);
        }

    }
}

