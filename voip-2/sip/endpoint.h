#pragma once

#include "../manager.h"
#include <sip/sipep.h>

namespace voip
{
	namespace sip
	{
		class endpoint : public SIPEndPoint
		{
			PCLASSINFO(endpoint, SIPEndPoint)
		public:
			endpoint(manager& mgr);
			~endpoint();

			void SetRealm(PString realm) { m_realm = realm; }

			virtual SIPConnection* CreateConnection(
				const SIPConnection::Init& init     ///< Initialisation parameters

			);

			virtual bool OnReceivedINVITE(
				SIP_PDU* pdu
			);

			virtual bool OnReINVITE(
				SIPConnection& connection, ///< Connection re-INVITE has occurred on
				bool fromRemote,            ///< This is a reponse to our re-INVITE
				const PString& remoteSDP   ///< SDP remote system sent.
			);

			virtual bool OnReceivedREGISTER(
				SIP_PDU& request
			);

			virtual RegistrarAoR* CreateRegistrarAoR(const SIP_PDU& request);

			virtual void OnChangedRegistrarAoR(RegistrarAoR& ua);

			virtual void OnRegistrationStatus(
				const PString& aor,
				PBoolean wasRegistering,
				PBoolean reRegistering,
				SIP_PDU::StatusCodes reason
			);

		protected:
			PString m_realm;
		};

	}
}
