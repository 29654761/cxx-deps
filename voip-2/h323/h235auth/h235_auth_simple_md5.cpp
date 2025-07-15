#include "h235_auth_simple_md5.h"
#include <asn/h225.h>
#include <h323/h323pdu.h>
#include <opal/endpoint.h>



//PFACTORY_CREATE(PFactory<H235Authenticator>, H235AuthSimpleMD5_2, "MD5", false);

H235AuthSimpleMD5_2::H235AuthSimpleMD5_2()
{
}

H235AuthSimpleMD5_2::~H235AuthSimpleMD5_2()
{

}

PBoolean H235AuthSimpleMD5_2::UseGkAndEpIdentifiers() const
{
    return true;
}


/*
H235Authenticator::ValidationResult H235AuthSimpleMD5_2::ValidateCryptoToken(
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
        PTRACE(1, "H235RAS\tH235AuthSimpleMD5 alias is \"" << alias
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

    // Generate an MD5 of the clear tokens PER encoding.
    PMessageDigest5 stomach;
    stomach.Process(strm.GetPointer(), strm.GetSize());
    PMessageDigest5::Code digest;
    stomach.Complete(digest);

    if (cryptoEPPwdHash.m_token.m_hash.GetData() == digest)
        return e_OK;

    PTRACE(1, "H235RAS\tH235AuthSimpleMD5 digest does not match.");
    return e_BadPassword;
}

*/

