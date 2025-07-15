#pragma once
#include <ptlib.h>
#include <opal.h>
#include <ptlib/ipsock.h>
#include <asn/h225.h>

namespace voip
{
	namespace h323
	{
		class reg_endpoint
		{
		public:
			reg_endpoint();
			~reg_endpoint();

			std::string id()const;
			bool is_timeout()const;

			static std::string make_id(const sockaddr* addr);
			static std::string make_id(const std::string& ip, int port);

		public:
			int64_t updated_at = 0;
			PIPSocket::Address signal_address;
			WORD signal_port = 0;
			PIPSocket::Address ras_address;
			WORD ras_port = 0;
			PStringArray aliases;
			int time_to_live=60;
			PString password;
			std::shared_ptr<H225_FeatureSet> feature_set;

			string address;
			int port = 0;


		};

	}
}

