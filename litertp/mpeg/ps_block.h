#pragma once
#include <string>

namespace litertp {
	namespace mpeg {
		class ps_block
		{
		public:
			ps_block();
			~ps_block();

			int deserialize(const uint8_t* buffer, size_t size);
			std::string serialize();
			void reset();
		public:
			uint8_t start_code;
			std::string block;
		};

	}
}
