
#include "registrar.h"
#include <mutex>

namespace voip
{
	namespace sip
	{

		registrar::registrar(const SIP_PDU& request):
			SIPEndPoint::RegistrarAoR(request.GetMIME().GetTo())
		{
			auto transport = request.GetTransport();
			if (transport) {
				set_remote_address(transport->GetRemoteAddress());
			}
		}

		registrar::~registrar()
		{
		}

		SIP_PDU::StatusCodes registrar::OnReceivedREGISTER(SIPEndPoint& endpoint, const SIP_PDU& request)
		{
			auto transport = request.GetTransport();
			if (transport) {
				set_remote_address(transport->GetRemoteAddress());
			}
			return SIPEndPoint::RegistrarAoR::OnReceivedREGISTER(endpoint,request);
		}

		OpalTransportAddress registrar::remote_address()const
		{
			std::shared_lock<std::shared_mutex> lk(remote_address_mutex_);
			return remote_address_;
		}

		void registrar::set_remote_address(const OpalTransportAddress& remote_address)
		{
			std::unique_lock<std::shared_mutex> lk(remote_address_mutex_);
			remote_address_ = remote_address;
		}

		PURL registrar::remote_url()const
		{
			OpalTransportAddress addr = remote_address();
			PIPSocket::Address ip;
			WORD port = 0;
			addr.GetIpAndPort(ip, port);
			SIPURL url;
			url.SetScheme(m_aor.GetScheme());
			url.SetUserName(m_aor.GetUserName());
			url.SetHostName(ip);
			url.SetPort(port);
			return url;
		}

	}
}

