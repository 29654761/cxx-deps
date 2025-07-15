#pragma once
#include <string>
#include <vector>

namespace litertp {
	namespace mpeg {
		class psm2
		{
		public:

			struct elementary_stream_map_t
			{
				uint8_t stream_type;
				uint8_t elementary_stream_id;
				std::string elementary_stream_info;
			};

			psm2();
			~psm2();

			bool deserialize(const uint8_t* buffer, size_t size);
			std::string serialize();
			void reset();

		public:
			uint8_t current_next_indicator : 1;
			uint8_t program_stream_map_version : 5;
			std::string program_stream_info;
			std::vector<elementary_stream_map_t> stream_map;
		};


	}
}
