#pragma once
#include <string>

namespace litertp {
	namespace mpeg {
		class psh2
		{
		public:
			psh2();
			~psh2();

			int deserialize(const uint8_t* buffer,size_t size);
			std::string serialize();
			void reset();
		public:
			// 6 bytes
			uint8_t flag : 2;
			uint64_t timestamp : 33;
			uint16_t timestamp_ex : 9;

			// 3 bytes
			uint32_t program_mux_rate : 22;

			// 1 bytes
			uint8_t pack_stuffing_length : 3;
		};

	}
}

