#include "hmac_sha.h"
#include <memory>

namespace ssl
{

	hmac_sha::hmac_sha(const std::string& algo, const std::string& key)
	{
		if (algo == "sha512")
		{
			evp_ = EVP_sha512();
		}
		else if (algo == "sha256")
		{
			evp_ = EVP_sha256();
		}
		else if (algo == "sha1")
		{
			evp_ = EVP_sha1();
		}

		ctx_ = HMAC_CTX_new();
		HMAC_Init_ex(ctx_, key.data(),key.size(),evp_,nullptr);
	}

	hmac_sha::~hmac_sha()
	{
		if (ctx_)
		{
			HMAC_CTX_free(ctx_);
			ctx_ = nullptr;
		}
	}

	bool hmac_sha::update(const void* data, size_t len)
	{
		return HMAC_Update(ctx_, (const unsigned char*)data, len);
	}

	std::string hmac_sha::final()
	{
		uint8_t out[EVP_MAX_MD_SIZE] = {};
		uint32_t len = 0;
		HMAC_Final(ctx_, out, &len);
		std::string r;
		r.assign((const char*)out, len);
		return r;
	}






}