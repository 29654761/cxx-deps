#pragma once
#include <string>
#include <vector>

namespace voip
{
	namespace sip
	{
		class challenge
		{
		public:
			challenge();
			~challenge();

			std::string to_string(const std::string& prefix = "Digest")const;
			bool parse(const std::string& s, const std::string& prefix = "Digest");

			bool supports_qop(const std::string& qop)const;

		private:
			static void format_value(std::string& val);
		public:
			std::string realm;
			std::vector<std::string> domains;
			std::string nonce;
			std::string opaque;
			bool stale = false;
			std::string algorithm;
			std::vector<std::string> qops;
			std::string charset;
			bool userhash = false;
		};


	}
}