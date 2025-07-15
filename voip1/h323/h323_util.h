#pragma once
#include <ptlib.h>
#include <opal.h>
#include <ptlib/ipsock.h>
#include <asn/h225.h>
#include <h323/h323pdu.h>

#include <litertp/avtypes.h>

#define OpalPluginCodec_Identifer_H264_Aligned        "0.0.8.241.0.0.0.0"
#define OpalPluginCodec_Identifer_H264_NonInterleaved "0.0.8.241.0.0.0.1"
#define OpalPluginCodec_Identifer_H264_Interleaved    "0.0.8.241.0.0.0.2"
#define OpalPluginCodec_Identifer_H264_Generic        "0.0.8.241.0.0.1"
#define OpalPluginCodec_Identifer_H264_Extend         "0.0.8.239.1.2"

namespace voip
{
	namespace h323
	{

		const unsigned H225_ProtocolID[] = { 0,0,8,2250,0,H225_PROTOCOL_VERSION };
		const unsigned H245_ProtocolID[] = { 0,0,8,245 ,0,H245_PROTOCOL_VERSION };
		static const char OID_MD5[] = "1.2.840.113549.2.5";


		class h323_util
		{
		public:
			static void make_md5_hash(const PString& alias, const PString& password, const H235_TimeStamp& timestam, PMessageDigest5::Code& digest);

			static void h225_transport_address_set(H225_TransportAddress& addr, const PIPSocket::Address& ip, WORD port);
			static bool h225_transport_address_get(const H225_TransportAddress& addr, PIPSocket::Address& ip, WORD& port);
			static void h245_transport_unicast_address_set(H245_TransportAddress& addr, const PIPSocket::Address& ip, WORD port);
			static bool h245_transport_unicast_address_get(const H245_TransportAddress& addr, PIPSocket::Address& ip, WORD& port);

			static void make_md5_crypto_token(const PString& alias, const PString& password, H225_CryptoH323Token& token);

			static void fill_vendor(H225_VendorIdentifier& vendor);

			static int get_default_payloadtype(H245_AudioCapability::Choices acodec);
			static int get_default_payloadtype(codec_type_t ct);

			static void fill_video_collapsing(H245_ArrayOf_GenericParameter& collapsing, uint8_t profile, uint16_t level,
				uint32_t mbps = 540, uint32_t mfs = 34, uint32_t max_nal_size = 1400);
			static void get_video_collapsing(const H245_ArrayOf_GenericParameter& collapsing, uint8_t& profile, uint16_t& level,
				uint32_t& mbps, uint32_t& mfs, uint32_t& max_nal_size);

			static codec_type_t convert_audio_codec_type(H245_AudioCapability::Choices codec);
			static H245_AudioCapability::Choices convert_audio_codec_type_back(codec_type_t codec);
		};

	}
}

