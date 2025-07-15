#pragma once

#include "transform.h"
#include <openssl/hmac.h>

namespace ssl
{
	class hmac_sha:public transform
	{
	public:
		hmac_sha(const std::string& algo,const std::string& key);
		~hmac_sha();

		virtual bool update(const void* data, size_t len);
		virtual std::string final();
	protected:
		const EVP_MD* evp_ = nullptr;
		HMAC_CTX* ctx_ = nullptr;
	};
	

}