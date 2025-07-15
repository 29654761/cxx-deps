#include "h235_auth_simple_sha1.h"
#include <asn/h225.h>
#include <h323/h323pdu.h>
#include <opal/endpoint.h>
#include <ptclib/cypher.h>


static const char Name_SHA1[] = "SHA1";
static const char OID_SHA1[] = "2.1872458839.1869769076.1751973888.0.0.0.0.0.0.0.0.0.0.0";

//PFACTORY_CREATE(PFactory<H235Authenticator>, H235AuthSimpleSHA1, Name_SHA1, false);

H235AuthSimpleSHA1::H235AuthSimpleSHA1()
{
    usage = AnyApplication;
}

H235AuthSimpleSHA1::~H235AuthSimpleSHA1()
{

}


PObject* H235AuthSimpleSHA1::Clone() const
{
    
    return new H235AuthSimpleSHA1(*this);
}


const char* H235AuthSimpleSHA1::GetName() const
{
    return Name_SHA1;
}


H225_CryptoH323Token* H235AuthSimpleSHA1::CreateCryptoToken(bool digits)
{
    if (!IsEnabled())
        return NULL;

    if (localId.IsEmpty()) {
        PTRACE(2, "H235RAS\tH235AuthSimpleSHA1 requires local ID for encoding.");
        return NULL;
    }

    if (digits && !OpalIsE164(localId, true))
        return NULL;

    // Cisco compatible hash calculation
    H235_ClearToken clearToken;

    // fill the PwdCertToken to calculate the hash
    clearToken.m_tokenOID = "0.0";

    // Create the H.225 crypto token
    H225_CryptoH323Token* cryptoToken = new H225_CryptoH323Token;
    cryptoToken->SetTag(H225_CryptoH323Token::e_cryptoEPPwdHash);
    H225_CryptoH323Token_cryptoEPPwdHash& cryptoEPPwdHash = *cryptoToken;

    // Set the alias
    if (digits) {
        cryptoEPPwdHash.m_alias.SetTag(H225_AliasAddress::e_dialedDigits);
        (PASN_IA5String&)cryptoEPPwdHash.m_alias = localId;
    }
    else {
        cryptoEPPwdHash.m_alias.SetTag(H225_AliasAddress::e_h323_ID);
        /* Avaya ECS Gatekeeper needs a trailing NULL character.
           Awaiting compatibility errors with other gatekeepers now. */
        (PASN_BMPString&)cryptoEPPwdHash.m_alias = localId + '\0';
    }

    // Use SetValueRaw to make sure trailing NULL is included
    clearToken.IncludeOptionalField(H235_ClearToken::e_generalID);
    clearToken.m_generalID.SetValueRaw(localId.AsWide());

    clearToken.IncludeOptionalField(H235_ClearToken::e_password);
    clearToken.m_password.SetValueRaw(password.AsWide());

    clearToken.IncludeOptionalField(H235_ClearToken::e_timeStamp);
    clearToken.m_timeStamp = (int)PTime().GetTimeInSeconds();

    // Encode it into PER
    PPER_Stream strm;
    clearToken.Encode(strm);
    strm.CompleteEncoding();

    // Generate an SHA1 of the clear tokens PER encoding.
    PMessageDigestSHA1 stomach;
    stomach.Process(strm.GetPointer(), strm.GetSize());
    PMessageDigestSHA1::Result digest;
    stomach.Complete(digest);

    // Set the token data that actually goes over the wire
    cryptoEPPwdHash.m_timeStamp = clearToken.m_timeStamp;
    cryptoEPPwdHash.m_token.m_algorithmOID = OID_SHA1;
    cryptoEPPwdHash.m_token.m_hash.SetData(digest);

    return cryptoToken;
}


H235Authenticator::ValidationResult H235AuthSimpleSHA1::ValidateCryptoToken(
    const H225_CryptoH323Token& cryptoToken,
    const PBYTEArray&)
{
    if (!IsEnabled())
        return e_Disabled;

    // verify the token is of correct type
    if (cryptoToken.GetTag() != H225_CryptoH323Token::e_cryptoEPPwdHash)
        return e_Absent;

    const H225_CryptoH323Token_cryptoEPPwdHash& cryptoEPPwdHash = cryptoToken;

    PString alias = H323GetAliasAddressString(cryptoEPPwdHash.m_alias);
    if (!remoteId.IsEmpty() && alias != remoteId) {
        PTRACE(1, "H235RAS\tH235AuthSimpleSHA1 alias is \"" << alias
            << "\", should be \"" << remoteId << '"');
        return e_Error;
    }

    // Build the clear token
    H235_ClearToken clearToken;
    clearToken.m_tokenOID = "0.0";

    clearToken.IncludeOptionalField(H235_ClearToken::e_generalID);
    clearToken.m_generalID.SetValueRaw(alias.AsWide()); // Use SetValueRaw to make sure trailing NULL is included

    clearToken.IncludeOptionalField(H235_ClearToken::e_password);
    clearToken.m_password.SetValueRaw(password.AsWide());

    clearToken.IncludeOptionalField(H235_ClearToken::e_timeStamp);
    clearToken.m_timeStamp = cryptoEPPwdHash.m_timeStamp;

    // Encode it into PER
    PPER_Stream strm;
    clearToken.Encode(strm);
    strm.CompleteEncoding();

    // Generate an SHA1 of the clear tokens PER encoding.
    PMessageDigestSHA1 stomach;
    stomach.Process(strm.GetPointer(), strm.GetSize());
    PMessageDigestSHA1::Result digest;
    stomach.Complete(digest);

    if (cryptoEPPwdHash.m_token.m_hash.GetData() == digest)
        return e_OK;

    PTRACE(1, "H235RAS\tH235AuthSimpleSHA1 digest does not match.");
    return e_BadPassword;
}


PBoolean H235AuthSimpleSHA1::IsCapability(const H235_AuthenticationMechanism& mechanism,
    const PASN_ObjectId& algorithmOID)
{
    return mechanism.GetTag() == H235_AuthenticationMechanism::e_pwdHash &&
        algorithmOID.AsString() == OID_SHA1;
}


PBoolean H235AuthSimpleSHA1::SetCapability(H225_ArrayOf_AuthenticationMechanism& mechanisms,
    H225_ArrayOf_PASN_ObjectId& algorithmOIDs)
{
    return AddCapabilityIfNeeded(H235_AuthenticationMechanism::e_pwdHash, OID_SHA1, mechanisms, algorithmOIDs) != P_MAX_INDEX;
}


PBoolean H235AuthSimpleSHA1::IsSecuredPDU(unsigned rasPDU, PBoolean received) const
{
    if (password.IsEmpty())
        return false;

    switch (rasPDU) {
    case H225_RasMessage::e_registrationRequest:
    case H225_RasMessage::e_unregistrationRequest:
    case H225_RasMessage::e_admissionRequest:
    case H225_RasMessage::e_disengageRequest:
    case H225_RasMessage::e_bandwidthRequest:
    case H225_RasMessage::e_infoRequestResponse:
        return received ? !remoteId.IsEmpty() : !localId.IsEmpty();

    default:
        return false;
    }
}
