#include "opal_opus48_capability.h"
#include <asn/h245.h>

const static BYTE opus48_data[] = { 0x00,0x00,0x00,0x07,0x00,0x00,0x00,0x48,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x30,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0xc8,0x00,0x00,0x00,0x05,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x0e };

namespace voip
{
	namespace mediaformats
	{
		opal_opus48_capability::opal_opus48_capability(const OpalMediaFormat& media_format)
			:H323NonStandardAudioCapability(38,0,8209,(const BYTE*)opus48_data,(PINDEX)sizeof(opus48_data),0,79)
		{
			
		}

		opal_opus48_capability::~opal_opus48_capability()
		{
		}

		PString opal_opus48_capability::GetFormatName() const
		{
			return m_mediaFormat.GetName();
		}


		PObject* opal_opus48_capability::Clone() const
		{
			return new opal_opus48_capability(*this);
		}

		
		//PBoolean opal_opus48_capability::IsMatch(
		//	const PASN_Object& subTypePDU,     ///<  sub-type PDU of H323Capability
		//	const PString& mediaPacketization  ///< Media packetization used
		//) const
		//{
		//	if (!H323Capability::IsMatch(subTypePDU, mediaPacketization))
		//	{
		//		return false;
		//	}

		//	const H245_NonStandardParameter& param = (const H245_NonStandardParameter&)dynamic_cast<const H245_AudioCapability&>(subTypePDU);

		//	//return CompareParam(param) == PObject::EqualTo && CompareData(param.m_data) == PObject::EqualTo;
		//	return CompareData(param.m_data) == PObject::EqualTo;
		//}

	}


}
