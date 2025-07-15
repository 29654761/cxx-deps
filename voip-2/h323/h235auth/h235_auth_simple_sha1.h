#pragma once
#include <h323/h235auth.h>

class H235AuthSimpleSHA1 :public H235Authenticator
{
    PCLASSINFO(H235AuthSimpleSHA1, H235Authenticator);
public:
	H235AuthSimpleSHA1();
	~H235AuthSimpleSHA1();

    PObject* Clone() const;

    virtual const char* GetName() const;

    virtual H225_CryptoH323Token* CreateCryptoToken(bool digits);

    virtual ValidationResult ValidateCryptoToken(
        const H225_CryptoH323Token& cryptoToken,
        const PBYTEArray& rawPDU
    );

    virtual PBoolean IsCapability(
        const H235_AuthenticationMechanism& mechansim,
        const PASN_ObjectId& algorithmOID
    );

    virtual PBoolean SetCapability(
        H225_ArrayOf_AuthenticationMechanism& mechansim,
        H225_ArrayOf_PASN_ObjectId& algorithmOIDs
    );

    virtual PBoolean IsSecuredPDU(
        unsigned rasPDU,
        PBoolean received
    ) const;
};



//PFACTORY_LOAD(H235AuthSimpleSHA1);
