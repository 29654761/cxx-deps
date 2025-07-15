#pragma once
#include "opal_event.h"
#include "calling_map.h"
#include <opal/manager.h>
#include <ep/localep.h>
#include <sip/sipep.h>
#include <h323/h323ep.h>
#include <mutex>

namespace voip
{

    class manager :public OpalManager
    {
        PCLASSINFO(manager, OpalManager)
    public:
        manager(voip::opal_event* e);
        ~manager();

        bool start();
        void stop();
        
        void set_nat_address(const PString& address);
        PString get_nat_address()const;

        calling_map& get_calling_map() { return calling_map_; }

        OpalMediaFormatList media_formats()const;

        virtual PBoolean IsRTPNATEnabled(
            OpalConnection& connection,            ///< Connection being checked
            const PIPSocket::Address& localAddr,   ///< Local physical address of connection
            const PIPSocket::Address& peerAddr,    ///< Remote physical address of connection
            const PIPSocket::Address& signalAddr,  ///< Remotes signaling address as indicated by protocol of connection
            PBoolean incoming                       ///< Incoming/outgoing connection
        );

        virtual bool OnLocalIncomingCall(
            OpalLocalConnection& connection ///< Connection for incoming call
        );

        virtual bool OnLocalOutgoingCall(
            const OpalLocalConnection& connection ///< Connection for outgoing call
        );

        virtual OpalConnection::AnswerCallResponse OnAnswerCall(OpalConnection& connection, const PString& caller);

        virtual PBoolean OnForwarded(
            OpalConnection& connection,  ///<  Connection that was held
            const PString& remoteParty         ///<  The new remote party
        );

        void OnSipRegistrationStatus(
            const PString& aor,
            PBoolean wasRegistering,
            PBoolean reRegistering,
            SIP_PDU::StatusCodes reason
        );

        void OnH323GatekeeperStatus(
            H323Gatekeeper& gk,
            H323Gatekeeper::RegistrationFailReasons status
        );

        virtual PBoolean OnRequirePassword(const PString& prefix,const PString& name, PString& password);
        virtual bool OnRegistration(const PString& prefix, const PString& username);
        virtual void OnUnregistration(const PString& prefix, const PString& username);
    protected:

        virtual PBoolean OnIncomingConnection(
            OpalConnection& connection,   ///<  Connection that is calling
            unsigned options,              ///<  options for new connection (can't use default as overrides will fail)
            OpalConnection::StringOptions* stringOptions ///< Options to pass to connection
        );


        virtual OpalMediaFormatList GetCommonMediaFormats(
            bool transportable,  ///< Include transportable media formats
            bool pcmAudio        ///< Include raw PCM audio media formats
        ) const;

        virtual void AdjustMediaFormats(
            bool local,                         ///<  Media formats a local ones to be presented to remote
            const OpalConnection& connection,  ///<  Connection that is about to use formats
            OpalMediaFormatList& mediaFormats  ///<  Media formats to use
        ) const;

        virtual OpalCall* CreateCall(
            void* userData            ///<  user data passed to SetUpCall
        );


    protected:
        mutable std::recursive_mutex nat_address_mutex_;
        PString nat_address_;
        voip::opal_event* event_ = nullptr;
        calling_map calling_map_;
    };
}

