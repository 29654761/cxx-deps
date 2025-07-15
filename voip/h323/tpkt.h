#pragma once
#include <ptlib.h>

namespace voip
{
	namespace h323
	{
		class tpkt
		{
		public:
			static bool deserialize(PBYTEArray& buffer, int offset,uint16_t& len);
			static PBYTEArray serialize(const PBYTEArray& pdu);
		};


	}
}

