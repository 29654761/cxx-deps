#pragma once
#include <string>
#include <vector>

namespace voip
{
	namespace sip
	{
		class credentials
		{
		public:
			credentials();
			~credentials();

			std::string to_string(const std::string& prefix = "Digest")const;
			bool parse(const std::string& s, const std::string& prefix = "Digest");

			std::string digest(const std::string& method,const std::string& password,const std::vector<std::string>* qops, const std::string& body);
		private:
			static void format_value(std::string& val);
			
		public:
			std::string username;
			std::string realm;
			std::string nonce;
			std::string uri;
			std::string response;
			std::string algorithm;
			std::string cnonce;
			std::string opaque;
			std::string qop;
			int nc = 0;
			bool userhash = false;
		};


	}
}