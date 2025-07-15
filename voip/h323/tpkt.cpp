#include "tpkt.h"

namespace voip
{
	namespace h323
	{
		bool tpkt::deserialize(PBYTEArray& buffer, int offset, uint16_t& len)
		{
			if (buffer.GetSize() - offset < 4)
			{
				return false;
			}
			int pos = offset;
			len = (buffer[(PINDEX)(pos + 2)] << 8) | (buffer[(PINDEX)(pos + 3)]);
			pos += 4;

			if (len < 4)
				return false;

			return true;
		}

		PBYTEArray tpkt::serialize(const PBYTEArray& pdu)
		{
			int length = pdu.GetSize() + 4;
			PBYTEArray tp(length);
			tp[(PINDEX)0] = 3;
			tp[(PINDEX)1] = 0;
			tp[(PINDEX)2] = (BYTE)(length >> 8);
			tp[(PINDEX)3] = (BYTE)length;
			memcpy(tp.GetPointer() + 4, (const BYTE*)pdu, pdu.GetSize());
			return tp;
		}
	}
}


