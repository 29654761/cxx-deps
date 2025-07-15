#pragma once
#include <sip/sipep.h>
#include <shared_mutex>

namespace voip
{
	namespace sip
	{
		class registrar:public SIPEndPoint::RegistrarAoR
		{
		public:
			registrar(const SIP_PDU& request);
			~registrar();


			virtual SIP_PDU::StatusCodes OnReceivedREGISTER(SIPEndPoint& endpoint, const SIP_PDU& request);

			OpalTransportAddress remote_address()const;
			void set_remote_address(const OpalTransportAddress& remote_address);


			virtual PURL remote_url()const;

		protected:
			mutable std::shared_mutex remote_address_mutex_;
			OpalTransportAddress remote_address_;
		};

	}
}