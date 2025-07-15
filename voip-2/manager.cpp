#include "manager.h"
#include "mediaformats/opal_opus48_capability.h"
#include "h323/h235auth/h235_auth_simple_md5.h"
#include "h323/endpoint.h"
#include "sip/endpoint.h"
#include "sip/registrar.h"
#include "call.h"

#include <h460/h460_std19.h>

namespace voip
{

    manager::manager(voip::opal_event* e)
    {
        event_ = e;
        

        //PFactory<H235Authenticator>::Register("SHA1", new H235AuthSimpleSHA1(), true);
        //PFactory<H235Authenticator>::Unregister("MD5");
        //PFactory<H235Authenticator>::Register("MD5", new H235AuthSimpleMD5_2(), true);

        AddRouteEntry("sip:.*   = local:*");
        AddRouteEntry("sips:.*  = local:*");
        AddRouteEntry("h323:.*  = local:*");
        AddRouteEntry("h323s:.* = local:*");

        SetAutoStartReceiveVideo(true);
        SetAutoStartTransmitVideo(true);
        //SetAudioJitterDelay(0, 0);
    }

    manager::~manager()
    {
        stop();
    }

    bool manager::start()
    {
        return calling_map_.start();
    }

    void manager::stop()
    {
        calling_map_.stop();
    }

    OpalMediaFormatList manager::media_formats()const
    {
        OpalMediaFormatList media_formats;
        
        //auto afmt = OpalG711ALaw;
        //afmt.SetOptionBoolean(OpalMediaFormat::NeedsJitterOption(), false);
        media_formats += OpalG711ALaw;

        auto vfmt = OpalH264_MODE1;
        vfmt.SetPayloadType(RTP_DataFrame::PayloadTypes(98));
        vfmt.AddOption(new OpalMediaOptionString("FMTP", true, "packetization-mode=1;max-br=1920;max-fs=8192;max-mbps=245760;profile-level-id=420028"));
        media_formats += vfmt;
        
        vfmt = OpalH264_MODE1;
        vfmt.SetPayloadType(RTP_DataFrame::PayloadTypes(99));
        vfmt.AddOption(new OpalMediaOptionString("FMTP", true, "packetization-mode=1;max-br=1920;max-fs=8192;max-mbps=245760;profile-level-id=640020"));
        media_formats += vfmt;
        /*
        static OpalMediaFormat psfmt("gb28181", 
            OpalMediaType::Video(),
            RTP_DataFrame::PayloadTypes::DynamicBase, 
            "PS",
            false,
            OpalBandwidth(),
            0,
            0,
            90000);
        psfmt.SetPayloadType(RTP_DataFrame::PayloadTypes(96));
        media_formats += psfmt;
        */

        /*
        vfmt = OpalH264_MODE0;
        vfmt.SetPayloadType(RTP_DataFrame::PayloadTypes(100));
        vfmt.AddOption(new OpalMediaOptionString("FMTP", true, "packetization-mode=0;max-br=4096;max-fs=8192;max-mbps=491520;profile-level-id=640020"));
        media_formats += vfmt;

        vfmt = OpalH264_MODE0;
        vfmt.SetPayloadType(RTP_DataFrame::PayloadTypes(101));
        vfmt.AddOption(new OpalMediaOptionString("FMTP", true, "packetization-mode=0;max-br=4096;max-fs=8192;max-mbps=491520;profile-level-id=420028"));
        media_formats += vfmt;
        */
        //auto opus48 = OpalOpus48;
        //opus48.SetOptionInteger(OpalAudioFormat::MaxFrameSizeOption(), 160);
        //H323CapabilityFactory::Register(opus48.GetName(), new mediaformats::opal_opus48_capability(opus48), true);
        //media_formats += opus48;

        return media_formats;
    }

    PBoolean manager::IsRTPNATEnabled(
        OpalConnection& connection,            ///< Connection being checked
        const PIPSocket::Address& localAddr,   ///< Local physical address of connection
        const PIPSocket::Address& peerAddr,    ///< Remote physical address of connection
        const PIPSocket::Address& signalAddr,  ///< Remotes signaling address as indicated by protocol of connection
        PBoolean incoming                       ///< Incoming/outgoing connection
    )
    {
        // return true will use the address from first received rpt packet to send destination.
        // return false will use the address from signal command(sip/h323)
        // return OpalManager::IsRTPNATEnabled() will auto detect.
        // 
        // if MTU Discover mode is enabled this must return true
        // 
        return true;
        //return OpalManager::IsRTPNATEnabled(connection, localAddr, peerAddr, signalAddr, incoming);
    }

    void manager::set_nat_address(const PString& address)
    {
        std::lock_guard<std::recursive_mutex> lk(nat_address_mutex_);
        nat_address_ = address;

        SetNATServer(PNatMethod_Fixed::MethodName(), address);
        SetNATServer(PNatMethod_H46019::MethodName(), address);
    }

    PString manager::get_nat_address()const
    {
        std::lock_guard<std::recursive_mutex> lk(nat_address_mutex_);
        return nat_address_;
    }


    bool manager::OnLocalIncomingCall(
        OpalLocalConnection& connection ///< Connection for incoming call
    )
    {
        string alias = connection.GetCall().GetPartyB();

        if (event_) {
            return event_->on_incoming_call(connection.GetCall().GetToken(), alias);
        }
        else {
            return true;
        }
        //connection.GetCall().SetCallEndReason(SIPConnection::EndedByNoUser);
       
    }

    bool manager::OnLocalOutgoingCall(
        const OpalLocalConnection& connection ///< Connection for outgoing call
    )
    {
        auto remoteCon=connection.GetCall().GetOtherPartyConnection(connection);
        
        string alias = connection.GetCall().GetPartyA();
        if (event_) {
            return event_->on_outgoing_call(connection.GetCall().GetToken(), alias);
        }
        else {
            return true;
        }
    }

    OpalConnection::AnswerCallResponse manager::OnAnswerCall(OpalConnection& connection, const PString& caller)
    {
        if (event_) {
            return event_->on_answer_call(connection, caller);
        }
        else {
            return OpalManager::OnAnswerCall(connection, caller);
        }
    }

    PBoolean manager::OnForwarded(
        OpalConnection& connection,  ///<  Connection that was held
        const PString& remoteParty         ///<  The new remote party
    )
    {
        if (!event_)
            return true;
        return event_->on_forwarded(connection, remoteParty);
    }


    PBoolean manager::OnRequirePassword(const PString& prefix, const PString& name, PString& password)
    {
        if (event_) {
            return event_->on_require_password(prefix,name,password);
        }
        else {
            return true;
        }
        
    }

    bool manager::OnRegistration(const PString& prefix, const PString& username)
    {
        if (event_) {
            return event_->on_registration(prefix, username);
        }
        else {
            return true;
        }
    }

    void manager::OnUnregistration(const PString& prefix, const PString& username)
    {
        if (event_) {
            return event_->on_unregistration(prefix, username);
        }
    }








    PBoolean manager::OnIncomingConnection(
        OpalConnection& connection,   ///<  Connection that is calling
        unsigned options,              ///<  options for new connection (can't use default as overrides will fail)
        OpalConnection::StringOptions* stringOptions ///< Options to pass to connection
    )
    {
        /*
        OpalConnection::StringOptions opts;
        PString nat=get_nat_address();
        if (!nat.IsEmpty())
        {
            if (stringOptions)
            {
                stringOptions->SetAt(OPAL_OPT_INTERFACE, nat);
            }
            else
            {
                opts.SetAt(OPAL_OPT_INTERFACE, nat);
                stringOptions = &opts;
            }
        }
        */
        return OpalManager::OnIncomingConnection(connection, options, stringOptions);
    }


    void manager::OnSipRegistrationStatus(
        const PString& aor,
        PBoolean wasRegistering,
        PBoolean reRegistering,
        SIP_PDU::StatusCodes reason
    )
    {
        if (event_)
        {
            event_->on_sip_reg_status(aor, wasRegistering, reRegistering, reason);
        }
    }

    void manager::OnH323GatekeeperStatus(
        H323Gatekeeper& gk,
        H323Gatekeeper::RegistrationFailReasons status
    )
    {
        if (event_)
        {
            event_->on_h323_reg_status(gk, status);
        }
    }

    OpalMediaFormatList manager::GetCommonMediaFormats(
        bool transportable,  ///< Include transportable media formats
        bool pcmAudio        ///< Include raw PCM audio media formats
    ) const
    {
        //OpalMediaFormatList fmts = OpalManager::GetCommonMediaFormats(transportable, pcmAudio);
        //fmts += media_formats_;
        //return fmts;
        return media_formats();
    }

    void manager::AdjustMediaFormats(
        bool local,                         ///<  Media formats a local ones to be presented to remote
        const OpalConnection& connection,  ///<  Connection that is about to use formats
        OpalMediaFormatList& mediaFormats  ///<  Media formats to use
    ) const
    {
        if (local)
        {
            mediaFormats.RemoveAll();
            mediaFormats += media_formats();
        }

        OpalManager::AdjustMediaFormats(local,connection, mediaFormats);
    }

    OpalCall* manager::CreateCall(
        void* userData            ///<  user data passed to SetUpCall
    )
    {
        return new voip::call(*this);
    }
}
