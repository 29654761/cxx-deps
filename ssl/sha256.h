#pragma once

#include "transform.h"
#include <openssl/sha.h>

namespace ssl
{
	class sha256:public transform
	{
	public:
		sha256();
		~sha256();

		virtual bool update(const void* data, size_t len);
		virtual std::string final();
	private:
		SHA256_CTX ctx_;
		
	};

}