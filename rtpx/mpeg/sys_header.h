#pragma once
#include <string>
#include <vector>




namespace rtpx {
	namespace mpeg {

		class sys_header
		{
		public:
			struct stream_t
			{
				uint8_t stream_id;
				uint8_t flag : 2; // '11'
				uint8_t p_std_buffer_bound_scale : 1;
				uint16_t p_std_buffer_size_bound : 13;
			};

			sys_header();
			~sys_header();

			bool deserialize(const uint8_t* buffer, size_t size);
			std::string serialize();
			void reset();
		public:
			uint32_t rate_bound : 22;
			uint8_t audio_bound : 6;
			uint8_t fixed_flag : 1;
			uint8_t csps_flag : 1;
			uint8_t system_audio_lock_flag : 1;
			uint8_t system_video_lock_flag : 1;
			uint8_t video_bound : 5;
			uint8_t packet_rate_restriction_flag : 1;
			std::vector<stream_t> streams;
		};


	}
}
