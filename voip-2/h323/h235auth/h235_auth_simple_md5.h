#pragma once
#include <h323/h235auth.h>

class H235AuthSimpleMD5_2 :public H235AuthSimpleMD5
{
    PCLASSINFO(H235AuthSimpleMD5_2, H235AuthSimpleMD5);
public:
    H235AuthSimpleMD5_2();
	~H235AuthSimpleMD5_2();

    virtual PBoolean UseGkAndEpIdentifiers() const;
};



//PFACTORY_LOAD(H235AuthSimpleMD5_2);
