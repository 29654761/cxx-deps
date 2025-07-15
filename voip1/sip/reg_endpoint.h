#pragma once
#include <hypertext/sip/sip_session.h>

namespace voip
{
	namespace sip
	{
		class reg_endpoint
		{
		public:
			reg_endpoint();
			~reg_endpoint();

			void update()
			{
				updated_at = time(nullptr);
			}
			bool is_timeout()const
			{
				int64_t now = time(nullptr);
				int64_t ts = now - updated_at;
				return ts >= 90;
			}

		public:
			std::string alias;
			sip_address contact;
			sip_session_ptr session;
			int64_t updated_at;
			std::string password;
		};

	}
}

