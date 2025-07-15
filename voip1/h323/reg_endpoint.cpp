#include "reg_endpoint.h"
#include <h323/h323pdu.h>
#include <sys2/socket.h>

namespace voip
{
	namespace h323
	{
		reg_endpoint::reg_endpoint()
		{
		}

		reg_endpoint::~reg_endpoint()
		{
		}

		std::string reg_endpoint::id()const
		{
			return "udp$" + address + ":" + std::to_string(port);
		}

		bool reg_endpoint::is_timeout()const
		{
			int64_t now = time(nullptr);
			if ((now - updated_at) > time_to_live + 10)
			{
				return true;
			}
			return false;
		}


		std::string reg_endpoint::make_id(const sockaddr* addr)
		{
			std::string ip;
			int port = 0;
			sys::socket::addr2ep(addr, &ip, &port);
			return make_id(ip, port);
		}

		std::string reg_endpoint::make_id(const std::string& ip, int port)
		{
			return "udp$" + ip + ":" + std::to_string(port);
		}
	}
}
