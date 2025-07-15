#pragma once
#include <string>

namespace ssl
{

	class transform
	{
	public:
		transform();
		~transform();

		virtual bool update(const void* data, size_t len) = 0;
		virtual std::string final() = 0;

		static void hex_string(const std::string& hex, std::string& out);
	};

}

