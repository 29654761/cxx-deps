#pragma once
#include <opal/mediafmt.h>
#include <h323/h323caps.h>

namespace voip
{
	namespace mediaformats
	{
		class opal_opus48_capability :public H323NonStandardAudioCapability
		{
			PCLASSINFO(opal_opus48_capability, H323NonStandardAudioCapability);
		public:

			


			opal_opus48_capability(const OpalMediaFormat& media_format);
			~opal_opus48_capability();

			virtual PString GetFormatName() const;


			virtual PObject* Clone() const;

			/*virtual PBoolean IsMatch(
				const PASN_Object& subTypePDU,     ///<  sub-type PDU of H323Capability
				const PString& mediaPacketization  ///< Media packetization used
			) const;*/
		};
	}
}