#pragma once
#include <string>
#include <vector>

namespace rtpx {
	namespace mpeg {

		class pes2
		{
		public:
			pes2();
			~pes2();

			bool deserialize(uint8_t stream_id,const uint8_t* buffer, size_t size);
			std::string serialize(uint8_t stream_id);
			void reset();

			bool deserialize1(const uint8_t* buffer, size_t size);
			bool deserialize2(const uint8_t* buffer, size_t size);
			bool deserialize3(const uint8_t* buffer, size_t size);

			std::string serialize1();
			std::string serialize2();
			std::string serialize3();

			void set_trick_mode_control_field_id(uint8_t v);
			void set_trick_mode_control_intra_slice_refresh(uint8_t v);
			void set_trick_mode_control_frequency_truncation(uint8_t v);
			uint8_t get_trick_mode_control_field_id();
			uint8_t set_trick_mode_control_intra_slice_refresh();
			uint8_t set_trick_mode_control_frequency_truncation();
		public:
			uint8_t pes_scrambling_control : 2;
			uint8_t pes_priority : 1;
			uint8_t data_alignment_indicator : 1;
			uint8_t copyright : 1;
			uint8_t original_or_copy : 1;
			uint8_t pts_dts_flags : 2;  // 2-pts; 3-pts&dts
			uint8_t escr_flag : 1;
			uint8_t es_rate_flag : 1;
			uint8_t dsm_trick_mode_flag : 1;
			uint8_t additional_copy_info_flag : 1;
			uint8_t pes_crc_flag : 1;
			uint8_t pes_extension_flag : 1;
			//uint8_t pes_header_data_length : 8;

			uint64_t pts : 33;
			uint64_t dts : 33;

			uint64_t escr_base : 33;
			uint16_t escr_extension : 9;

			uint32_t es_rate:22;

			uint8_t trick_mode_control : 3;
			uint8_t trick_mode_val : 5;

			uint8_t additional_copy_info : 7;
			uint16_t previous_pes_packet_crc : 16;

			uint8_t pes_private_data_flag : 1;
			uint8_t pack_header_field_flag : 1;
			uint8_t program_packer_sequence_counter_flag : 1;
			uint8_t p_std_buffer_flag : 1;
			uint8_t pes_extension_flag_2 : 1;
			uint8_t pes_private_data[16];

			std::string headers;

			uint8_t program_packet_sequence_counter : 7;
			uint8_t mpeg1_mpeg2_indentifier : 1;
			uint8_t original_stuff_length : 6;

			uint8_t p_std_buffer_scale : 1;
			uint16_t p_std_buffer_size : 13;
			uint8_t pes_extension_field_length : 7;

			uint8_t stuffing_length;

			std::string pes_packet_data_byte;

		};
	}
}

