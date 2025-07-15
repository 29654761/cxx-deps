#include "h323_util.h"

namespace voip
{
	namespace h323
	{
		void h323_util::make_md5_hash(const PString& alias,const PString& password,const H235_TimeStamp& timestam, PMessageDigest5::Code& digest)
		{
			H235_ClearToken clearToken;
			clearToken.m_tokenOID = "0.0";

			clearToken.IncludeOptionalField(H235_ClearToken::e_generalID);
			clearToken.m_generalID.SetValueRaw(alias.AsWide()); // Use SetValueRaw to make sure trailing NULL is included

			clearToken.IncludeOptionalField(H235_ClearToken::e_password);
			clearToken.m_password.SetValueRaw(password.AsWide());

			clearToken.IncludeOptionalField(H235_ClearToken::e_timeStamp);
			clearToken.m_timeStamp = timestam;

			// Encode it into PER
			PPER_Stream strm;
			clearToken.Encode(strm);
			strm.CompleteEncoding();

			// Generate an MD5 of the clear tokens PER encoding.
			PMessageDigest5 stomach;
			stomach.Process(strm.GetPointer(), strm.GetSize());
			stomach.Complete(digest);
		}

		void h323_util::h225_transport_address_set(H225_TransportAddress& addr, const PIPSocket::Address& ip, WORD port)
		{
			if (ip.GetVersion() == 4)
			{
				addr.SetTag(H225_TransportAddress::e_ipAddress);
				H225_TransportAddress_ipAddress& ipaddr = (H225_TransportAddress_ipAddress&)addr;
				for (PINDEX i = 0; i < 4; i++)
				{
					ipaddr.m_ip[i] = ip[i];
				}
				ipaddr.m_port = port;
			}
			else if (ip.GetVersion() == 6)
			{
				addr.SetTag(H225_TransportAddress::e_ipAddress);
				H225_TransportAddress_ip6Address& ipaddr = (H225_TransportAddress_ip6Address&)addr;
				for (PINDEX i = 0; i < 16; i++)
				{
					ipaddr.m_ip[i] = ip[i];
				}
				ipaddr.m_port = port;
			}
		}

		bool h323_util::h225_transport_address_get(const H225_TransportAddress& addr, PIPSocket::Address& ip, WORD& port)
		{
			if (addr.GetTag() == H225_TransportAddress::e_ipAddress)
			{
				const H225_TransportAddress_ipAddress& ipaddr = (const H225_TransportAddress_ipAddress&)addr;
				if (ipaddr.m_ip.GetSize() < 4) {
					return false;
				}
				ip=PIPSocket::Address (ipaddr.m_ip.GetSize(), ipaddr.m_ip.GetValue());
				port = ipaddr.m_port;
				return true;
			}
			else if (addr.GetTag() == H225_TransportAddress::e_ip6Address)
			{
				const H225_TransportAddress_ip6Address& ipaddr = (const H225_TransportAddress_ip6Address&)addr;
				if (ipaddr.m_ip.GetSize() < 16) {
					return false;
				}
				ip = PIPSocket::Address(ipaddr.m_ip.GetSize(), ipaddr.m_ip.GetValue());
				port = ipaddr.m_port;
				return true;
			}
			else
			{
				return false;
			}
		}

		void h323_util::h245_transport_unicast_address_set(H245_TransportAddress& addr, const PIPSocket::Address& ip, WORD port)
		{
			addr.SetTag(H245_TransportAddress::e_unicastAddress);
			H245_UnicastAddress& unicast = (H245_UnicastAddress&)addr;

			if (ip.GetVersion() == 4)
			{
				unicast.SetTag(H245_UnicastAddress::e_iPAddress);
				H245_UnicastAddress_iPAddress& ipaddr = (H245_UnicastAddress_iPAddress&)unicast;

				for (PINDEX i = 0; i < 4; i++)
				{
					ipaddr.m_network[i] = ip[i];
				}
				ipaddr.m_tsapIdentifier = port;
			}
			else if (ip.GetVersion() == 6)
			{
				unicast.SetTag(H245_UnicastAddress::e_iP6Address);
				H245_UnicastAddress_iP6Address& ipaddr = (H245_UnicastAddress_iP6Address&)unicast;

				for (PINDEX i = 0; i < 16; i++)
				{
					ipaddr.m_network[i] = ip[i];
				}
				ipaddr.m_tsapIdentifier = port;
			}
		}

		bool h323_util::h245_transport_unicast_address_get(const H245_TransportAddress& addr, PIPSocket::Address& ip, WORD& port)
		{
			if (addr.GetTag() != H245_TransportAddress::e_unicastAddress)
				return false;

			const H245_UnicastAddress& unicast = (const H245_UnicastAddress&)addr;
			if (unicast.GetTag() == H245_UnicastAddress::e_iPAddress)
			{
				const H245_UnicastAddress_iPAddress& ipaddr = (const H245_UnicastAddress_iPAddress&)unicast;
				if (ipaddr.m_network.GetSize() < 4) {
					return false;
				}

				ip = PIPSocket::Address(ipaddr.m_network.GetSize(), ipaddr.m_network.GetValue());
				port = ipaddr.m_tsapIdentifier;
				return true;
			}
			else if (unicast.GetTag() == H245_UnicastAddress::e_iP6Address)
			{
				const H245_UnicastAddress_iP6Address& ipaddr = (const H245_UnicastAddress_iP6Address&)unicast;
				if (ipaddr.m_network.GetSize() < 16) {
					return false;
				}

				ip = PIPSocket::Address(ipaddr.m_network.GetSize(),ipaddr.m_network.GetValue());
				port = ipaddr.m_tsapIdentifier;
				return true;
			}
			else
			{
				return false;
			}
		}

		void h323_util::make_md5_crypto_token(const PString& alias, const PString& password, H225_CryptoH323Token& token)
		{
			token.SetTag(H225_CryptoH323Token::e_cryptoEPPwdHash);
			H225_CryptoH323Token_cryptoEPPwdHash& hash = (H225_CryptoH323Token_cryptoEPPwdHash&)token;
			H323SetAliasAddress(alias, hash.m_alias);
			hash.m_timeStamp = (int)PTime().GetTimeInSeconds();;
			hash.m_token.m_algorithmOID = OID_MD5;
			PMessageDigest5::Code digest;
			h323_util::make_md5_hash(alias, password, hash.m_timeStamp, digest);
			hash.m_token.m_hash.SetData(digest);
		}

		void h323_util::fill_vendor(H225_VendorIdentifier& vendor)
		{
			vendor.m_vendor.m_t35CountryCode = 9;
			vendor.m_vendor.m_t35Extension = 0;
			vendor.m_vendor.m_manufacturerCode = 61;
			vendor.IncludeOptionalField(H225_VendorIdentifier::e_productId);
			vendor.m_productId = "devanget";
			vendor.IncludeOptionalField(H225_VendorIdentifier::e_versionId);
			vendor.m_versionId = "devanget (v1.0)";
			//vendor.IncludeOptionalField(H225_VendorIdentifier::e_enterpriseNumber);
			//PUnsignedArray epnum;
			//epnum.SetSize(4);
			//epnum[0] = 0x90;
			//epnum[1] = 0x00;
			//epnum[2] = 0x00;
			//epnum[3] = 0x3d;
			//vendor.m_enterpriseNumber.SetValue(epnum);
		}

		int h323_util::get_default_payloadtype(H245_AudioCapability::Choices acodec)
		{
			switch (acodec)
			{
			case H245_AudioCapability::e_g711Alaw64k:
			case H245_AudioCapability::e_g711Alaw56k:
				return 8;
			case H245_AudioCapability::e_g711Ulaw64k:
			case H245_AudioCapability::e_g711Ulaw56k:
				return 0;
			case H245_AudioCapability::e_g722_64k:
			case H245_AudioCapability::e_g722_56k:
			case H245_AudioCapability::e_g722_48k:
				return 9;
			case H245_AudioCapability::e_g7231:
				return 4;
			case H245_AudioCapability::e_g728:
				return 15;
			case H245_AudioCapability::e_g729:
				return 18;
			default:
				return -1;
			}
		}

		int h323_util::get_default_payloadtype(codec_type_t ct)
		{
			switch (ct)
			{
			case codec_type_pcma:
				return 8;
			case codec_type_pcmu:
				return 0;
			case codec_type_g722:
				return 9;
			default:
				return -1;
			}
		}

		void h323_util::fill_video_collapsing(H245_ArrayOf_GenericParameter& collapsing, uint8_t profile, uint16_t level,
			uint32_t mbps, uint32_t mfs, uint32_t max_nal_size)
		{
			H323AddGenericParameterAs<PASN_Integer>(collapsing, 41, H245_ParameterValue::Choices::e_booleanArray) = profile;
			H323AddGenericParameterAs<PASN_Integer>(collapsing, 42, H245_ParameterValue::Choices::e_unsignedMin) = level;
			H323AddGenericParameterAs<PASN_Integer>(collapsing, 3, H245_ParameterValue::Choices::e_unsignedMin) = mbps;
			H323AddGenericParameterAs<PASN_Integer>(collapsing, 4, H245_ParameterValue::Choices::e_unsignedMin) = mfs;
			H323AddGenericParameterAs<PASN_Integer>(collapsing, 9, H245_ParameterValue::Choices::e_unsignedMin) = max_nal_size;
		}

		void h323_util::get_video_collapsing(const H245_ArrayOf_GenericParameter& collapsing, uint8_t& profile, uint16_t& level,
			uint32_t& mbps, uint32_t& mfs, uint32_t& max_nal_size)
		{
			profile = H323GetGenericParameterInteger(collapsing, 41,0,H245_ParameterValue::Choices::e_booleanArray);
			level = H323GetGenericParameterInteger(collapsing, 42);
			mbps = H323GetGenericParameterInteger(collapsing, 3);
			mfs = H323GetGenericParameterInteger(collapsing, 4);
			max_nal_size = H323GetGenericParameterInteger(collapsing, 9);
		}

		codec_type_t h323_util::convert_audio_codec_type(H245_AudioCapability::Choices codec)
		{
			switch (codec)
			{
			case H245_AudioCapability::e_g711Alaw64k:
			case H245_AudioCapability::e_g711Alaw56k:
				return codec_type_pcma;
			case H245_AudioCapability::e_g711Ulaw64k:
			case H245_AudioCapability::e_g711Ulaw56k:
				return codec_type_pcmu;
			case H245_AudioCapability::e_g722_48k:
			case H245_AudioCapability::e_g722_56k:
			case H245_AudioCapability::e_g722_64k:
				return codec_type_g722;
			default:
				return codec_type_unknown;
			}
		}



		H245_AudioCapability::Choices h323_util::convert_audio_codec_type_back(codec_type_t codec)
		{
			switch (codec)
			{
			case codec_type_t::codec_type_pcma:
				return H245_AudioCapability::Choices::e_g711Alaw64k;
			case codec_type_t::codec_type_pcmu:
				return H245_AudioCapability::Choices::e_g711Ulaw64k;
			case codec_type_t::codec_type_g722:
				return H245_AudioCapability::Choices::e_g722_64k;
			default:
				return H245_AudioCapability::Choices::e_nonStandard;
			}
		}
	}
}

