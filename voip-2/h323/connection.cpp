#include "connection.h"
#if OPAL_H450
#include <h323/h450pdu.h>
#endif

#include <h323/h323ep.h>
#include <h323/h323neg.h>

namespace voip
{
	namespace h323
	{

        static bool BuildFastStartList(const H323Channel& channel,
            H225_ArrayOf_PASN_OctetString& array,
            H323Channel::Directions reverseDirection)
        {
            H245_OpenLogicalChannel open;

            if (channel.GetDirection() != reverseDirection) {
                if (!channel.OnSendingPDU(open))
                    return false;
            }
            else {
                open.IncludeOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters);
                if (!channel.OnSendingPDU(open))
                    return false;

                open.m_reverseLogicalChannelParameters.m_dataType = open.m_forwardLogicalChannelParameters.m_dataType;
                open.m_forwardLogicalChannelParameters.m_dataType.SetTag(H245_DataType::e_nullData);
                open.m_forwardLogicalChannelParameters.m_multiplexParameters.SetTag(
                    H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters::e_none);
            }

            PTRACE(4, &channel, "H225\tBuild fastStart:\n  " << setprecision(2) << open);
            PINDEX last = array.GetSize();
            array.SetSize(last + 1);
            array[last].EncodeSubType(open);

            PTRACE(3, &channel, "H225\tBuilt fastStart for " << channel << ' ' << channel.GetCapability());
            return true;
        }





		connection::connection(
			OpalCall& call,                         ///<  Call object connection belongs to
			H323EndPoint& endpoint,                 ///<  H323 End Point object
			const PString& token,                   ///<  Token for new connection
			const PString& alias,                   ///<  Alias for outgoing call
			const H323TransportAddress& address,    ///<  Address for outgoing call
			unsigned options,                    ///<  Connection option bits
			OpalConnection::StringOptions* stringOptions ///<  complex string options
		) :H323Connection(call, endpoint, token, alias, address, options, stringOptions)
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
			//return new media_stream(*this,mediaFormat, sessionID, isSource);
			return H323Connection::CreateMediaStream(mediaFormat, sessionID, isSource);
		}

		OpalMediaStreamPtr connection::OpenMediaStream(
			const OpalMediaFormat& mediaFormat, ///<  Media format to open
			unsigned sessionID,                  ///<  Session to start stream on
			bool isSource                        ///< Stream is a source/sink
		)
		{
			return H323Connection::OpenMediaStream(mediaFormat, sessionID, isSource);
		}

		void connection::AdjustMediaFormats(
			bool local,                             ///<  Media formats a local ones to be presented to remote
			const OpalConnection* otherConnection, ///<  Other connection we are adjusting media for
			OpalMediaFormatList& mediaFormats      ///<  Media formats to use
		) const
		{
			H323Connection::AdjustMediaFormats(local, otherConnection, mediaFormats);
		}

		PBoolean connection::OpenLogicalChannel(
			const H323Capability& capability,  ///<  Capability to open channel with
			unsigned sessionID,                 ///<  Session for the channel
			H323Channel::Directions dir         ///<  Direction of channel
		)
		{
			return H323Connection::OpenLogicalChannel(capability, sessionID, dir);
		}

		void connection::OnSetLocalCapabilities()
		{
			H323Connection::OnSetLocalCapabilities();
		}

		OpalConnection::CallEndReason connection::SendSignalSetup2(
			const PString& alias,                ///<  Name of remote party
			const H323TransportAddress& address  ///<  Address of destination
		)
		{
            PSafeLockReadWrite safeLock(*this);
            if (!safeLock.IsLocked())
                return EndedByCallerAbort;

            // Start the call, first state is asking gatekeeper
            m_connectionState = AwaitingGatekeeperAdmission;

            if (m_stringOptions.Has(OPAL_OPT_CALLING_PARTY_NUMBER))
                SetLocalPartyName(m_stringOptions.GetString(OPAL_OPT_CALLING_PARTY_NUMBER));
            else  if (m_stringOptions.Has(OPAL_OPT_CALLING_PARTY_NAME))
                SetLocalPartyName(m_stringOptions.GetString(OPAL_OPT_CALLING_PARTY_NAME));

            // See if we are "proxied" that is the destCallSignalAddress is different
            // from the transport connection address
            H323TransportAddress destCallSignalAddress = address;
            PINDEX atInAlias = alias.Find('@');
            if (atInAlias != P_MAX_INDEX)
                destCallSignalAddress = H323TransportAddress(alias.Mid(atInAlias + 1), m_endpoint.GetDefaultSignalPort());

            // Initial value for alias in SETUP, could be overridden by gatekeeper
            H225_ArrayOf_AliasAddress newAliasAddresses;
            if (!alias.IsEmpty() && atInAlias > 0) {
                newAliasAddresses.SetSize(1);
                H323SetAliasAddress(alias.Left(atInAlias), newAliasAddresses[0]);
            }

            // Start building the setup PDU to get various ID's
            H323SignalPDU setupPDU;
            H225_Setup_UUIE& setup = setupPDU.BuildSetup(*this, destCallSignalAddress);

#if OPAL_H450
            m_h450dispatcher->AttachToSetup(setupPDU);
#endif

            // Save the identifiers generated by BuildSetup
            m_callReference = setupPDU.GetQ931().GetCallReference();
            m_conferenceIdentifier = setup.m_conferenceID;
            setupPDU.GetQ931().GetCalledPartyNumber(m_remotePartyNumber);

            H323TransportAddress gatekeeperRoute = address;

            // Update the field e_destinationAddress in the SETUP PDU to reflect the new
            // alias received in the ACF (m_destinationInfo).
            if (newAliasAddresses.GetSize() > 0) {
                setup.IncludeOptionalField(H225_Setup_UUIE::e_destinationAddress);
                setup.m_destinationAddress = newAliasAddresses;

                // Update the Q.931 Information Element (if is an E.164 address)
                PString e164 = H323GetAliasAddressE164(newAliasAddresses);
                if (!e164.IsEmpty())
                    m_remotePartyNumber = e164;
            }

            if (m_addAccessTokenToSetup && !m_gkAccessTokenOID.IsEmpty() && !m_gkAccessTokenData.IsEmpty()) {
                PString oid1, oid2;
                m_gkAccessTokenOID.Split(',', oid1, oid2, PString::SplitTrim | PString::SplitDefaultToBefore | PString::SplitDefaultToAfter);
                setup.IncludeOptionalField(H225_Setup_UUIE::e_tokens);
                PINDEX last = setup.m_tokens.GetSize();
                setup.m_tokens.SetSize(last + 1);
                setup.m_tokens[last].m_tokenOID = oid1;
                setup.m_tokens[last].IncludeOptionalField(H235_ClearToken::e_nonStandard);
                setup.m_tokens[last].m_nonStandard.m_nonStandardIdentifier = oid2;
                setup.m_tokens[last].m_nonStandard.m_data = m_gkAccessTokenData;
            }

            if (!m_signallingChannel->SetRemoteAddress(gatekeeperRoute)) {
                PTRACE(1, "H225\tInvalid "
                    << (gatekeeperRoute != address ? "gatekeeper" : "user")
                    << " supplied address: \"" << gatekeeperRoute << '"');
                m_connectionState = AwaitingTransportConnect;
                return EndedByConnectFail;
            }

#if OPAL_H460
            {
                /* When sending the H460 message in the Setup PDU you have to ensure the
                   ARQ is received first then add the Fields to the Setup PDU */
                H225_FeatureSet fs;
                if (OnSendFeatureSet(H460_MessageType::e_setup, fs)) {
                    if (fs.HasOptionalField(H225_FeatureSet::e_neededFeatures)) {
                        setup.IncludeOptionalField(H225_Setup_UUIE::e_neededFeatures);
                        H225_ArrayOf_FeatureDescriptor& fsn = setup.m_neededFeatures;
                        fsn = fs.m_neededFeatures;
                    }

                    if (fs.HasOptionalField(H225_FeatureSet::e_desiredFeatures)) {
                        setup.IncludeOptionalField(H225_Setup_UUIE::e_desiredFeatures);
                        H225_ArrayOf_FeatureDescriptor& fsn = setup.m_desiredFeatures;
                        fsn = fs.m_desiredFeatures;
                    }

                    if (fs.HasOptionalField(H225_FeatureSet::e_supportedFeatures)) {
                        setup.IncludeOptionalField(H225_Setup_UUIE::e_supportedFeatures);
                        H225_ArrayOf_FeatureDescriptor& fsn = setup.m_supportedFeatures;
                        fsn = fs.m_supportedFeatures;
                    }
                }
            }
#endif

            // Do the transport connect
            m_connectionState = AwaitingTransportConnect;

            // Release the mutex as can deadlock trying to clear call during connect.
            safeLock.Unlock();

            bool connectFailed = !m_signallingChannel->Connect();

            // Lock while checking for shutting down.
            if (!safeLock.Lock() || IsReleased())
                return EndedByCallerAbort;

            // See if transport connect failed, abort if so.
            if (connectFailed) {
                m_connectionState = NoConnectionActive;
                switch (m_signallingChannel->GetErrorNumber()) {
                case ENETUNREACH:
                    return EndedByUnreachable;
                case ECONNREFUSED:
                    return EndedByNoEndPoint;
                case ETIMEDOUT:
                    return EndedByHostOffline;
                }
                return EndedByConnectFail;
            }

            PTRACE(3, "H225\tSending Setup PDU");
            m_connectionState = AwaitingSignalConnect;

            // Put in all the signalling addresses for link
            H323TransportAddress transportAddress = m_signallingChannel->GetLocalAddress();
            setup.IncludeOptionalField(H225_Setup_UUIE::e_sourceCallSignalAddress);
            transportAddress.SetPDU(setup.m_sourceCallSignalAddress);
            if (!setup.HasOptionalField(H225_Setup_UUIE::e_destCallSignalAddress)) {
                transportAddress = m_signallingChannel->GetRemoteAddress();
                setup.IncludeOptionalField(H225_Setup_UUIE::e_destCallSignalAddress);
                transportAddress.SetPDU(setup.m_destCallSignalAddress);
            }

            OnApplyStringOptions();

            // Get the local capabilities before fast start is handled
            OnSetLocalCapabilities();

            if (IsReleased())
                return EndedByCallerAbort;

            // Start channels, if allowed
            m_fastStartChannels.RemoveAll();
            if (m_fastStartState == FastStartInitiate) {
                PTRACE(3, "H225\tFast connect by local endpoint");
                OnSelectLogicalChannels();
            }

            // If application called OpenLogicalChannel, put in the fastStart field
            if (!m_fastStartChannels.IsEmpty()) {
                PTRACE(3, "H225\tFast start begun by local endpoint");
                for (H323LogicalChannelList::iterator channel = m_fastStartChannels.begin(); channel != m_fastStartChannels.end(); ++channel)
                    BuildFastStartList(*channel, setup.m_fastStart, H323Channel::IsReceiver);
                if (setup.m_fastStart.GetSize() > 0)
                    setup.IncludeOptionalField(H225_Setup_UUIE::e_fastStart);
                else
                    m_fastStartChannels.RemoveAll();
            }

            SetOutgoingBearerCapabilities(setupPDU);

#if OPAL_H235_6
            {
                OpalMediaCryptoSuite::List cryptoSuites = OpalMediaCryptoSuite::FindAll(m_endpoint.GetMediaCryptoSuites(), "H.235");
                for (OpalMediaCryptoSuite::List::iterator it = cryptoSuites.begin(); it != cryptoSuites.end(); ++it)
                    m_dh.AddForAlgorithm(*it);
                if (m_dh.ToTokens(setup.m_tokens))
                    setup.IncludeOptionalField(H225_Setup_UUIE::e_tokens);
            }
#endif

            // Do this again (was done when PDU was constructed) in case
            // OnSendSignalSetup() changed something.
            setupPDU.SetQ931Fields(*this, true);
            setupPDU.GetQ931().GetCalledPartyNumber(m_remotePartyNumber);

            m_fastStartState = FastStartDisabled;
            PBoolean set_lastPDUWasH245inSETUP = false;

            if (m_h245Tunneling && m_doH245inSETUP && !m_endpoint.IsH245Disabled()) {
                m_h245TunnelTxPDU = &setupPDU;

                // Try and start the master/slave and capability exchange through the tunnel
                // Note: this used to be disallowed but is now allowed as of H323v4
                PBoolean ok = StartControlNegotiations();

                m_h245TunnelTxPDU = NULL;

                if (!ok)
                    return EndedByTransportFail;

                if (m_doH245inSETUP && (setup.m_fastStart.GetSize() > 0)) {

                    // Now if fast start as well need to put this in setup specific field
                    // and not the generic H.245 tunneling field
                    setup.IncludeOptionalField(H225_Setup_UUIE::e_parallelH245Control);
                    setup.m_parallelH245Control = setupPDU.m_h323_uu_pdu.m_h245Control;
                    setupPDU.m_h323_uu_pdu.RemoveOptionalField(H225_H323_UU_PDU::e_h245Control);
                    set_lastPDUWasH245inSETUP = true;
                }
            }

            if (alias == "register" && m_endpoint.GetProductInfo() == H323EndPoint::AvayaPhone()) {
                PTRACE(4, "Setting SETUP goal for Avaya IP Phone");
                setup.m_conferenceGoal = H225_Setup_UUIE_conferenceGoal::e_callIndependentSupplementaryService;
                setup.RemoveOptionalField(H225_Setup_UUIE::e_sourceAddress);
                setup.RemoveOptionalField(H225_Setup_UUIE::e_sourceCallSignalAddress);
                setup.RemoveOptionalField(H225_Setup_UUIE::e_destinationAddress);
                setup.RemoveOptionalField(H225_Setup_UUIE::e_destCallSignalAddress);
                setup.m_mediaWaitForConnect = true;
                setup.m_canOverlapSend = true;
                setup.m_multipleCalls = true;
            }

            if (!OnSendSignalSetup(setupPDU))
                return EndedByNoAccept;

            // Send the initial PDU
            if (!WriteSignalPDU(setupPDU))
                return EndedByTransportFail;

            SetPhase(SetUpPhase);

            // WriteSignalPDU always resets lastPDUWasH245inSETUP.
            // So set it here if required
            if (set_lastPDUWasH245inSETUP)
                m_lastPDUWasH245inSETUP = true;

            // Set timeout for remote party to answer the call
            m_signallingChannel->SetReadTimeout(m_endpoint.GetSignallingChannelCallTimeout());

            m_connectionState = AwaitingSignalConnect;

            return NumCallEndReasons;
		}
	}
}



