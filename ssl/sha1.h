#pragma once

#include "transform.h"
#include <openssl/sha.h>

namespace ssl
{
	class sha1:public transform
	{
	public:
		sha1();
		~sha1();

		virtual bool update(const void* data, size_t len);
		virtual std::string final();
	private:
		SHA_CTX ctx_;
		
	};

}