#include "pes2.h"
#include "ps_frame.h"
#include <string.h>

namespace litertp {
	namespace mpeg {

		pes2::pes2()
		{
			reset();
		}

		pes2::~pes2()
		{
		}

		bool pes2::deserialize(uint8_t stream_id, const uint8_t* buffer, size_t size)
		{
			reset();
			if (stream_id != PS_STREAM_ID_MAP
				&& stream_id != PS_STREAM_ID_PADDING
				&& stream_id != PS_STREAM_ID_PRIVATE_STREAM2
				&& stream_id != PS_STREAM_ID_ECM
				&& stream_id != PS_STREAM_ID_EMM
				&& stream_id != PS_STREAM_ID_DIRECTORY
				&& stream_id != PS_STREAM_ID_DSMCC
				&& stream_id != PS_STREAM_ID_ITU_T_H222_1)
			{
				return deserialize1(buffer, size);
			}
			else if (stream_id == PS_STREAM_ID_MAP
				|| stream_id == PS_STREAM_ID_PRIVATE_STREAM2
				|| stream_id == PS_STREAM_ID_ECM
				|| stream_id == PS_STREAM_ID_EMM
				|| stream_id == PS_STREAM_ID_DIRECTORY
				|| stream_id == PS_STREAM_ID_DSMCC
				|| stream_id == PS_STREAM_ID_ITU_T_H222_1)
			{
				return deserialize2(buffer, size);
			}
			else if (stream_id == PS_STREAM_ID_PADDING)
			{
				return deserialize3(buffer, size);
			}
			else
			{
				return false;
			}
		}

		bool pes2::deserialize1(const uint8_t* buffer, size_t size)
		{
			int pos = 0;

			if (size < 3)
				return false;

			uint8_t v = buffer[pos++];
			pes_scrambling_control = (v & 0x30) >> 4;
			pes_priority= (v & 0x08) >> 3;
			data_alignment_indicator= (v & 0x04) >> 2;
			copyright = (v & 0x02) >> 1;
			original_or_copy = v & 0x01;

			v = buffer[pos++];
			pts_dts_flags = (v & 0xc0) >> 6;
			escr_flag = (v & 0x20) >> 5;
			es_rate_flag = (v & 0x10) >> 4;
			dsm_trick_mode_flag = (v & 0x8) >> 3;
			additional_copy_info_flag = (v & 0x4) >> 2;
			pes_crc_flag = (v & 0x2) >> 1;
			pes_extension_flag = v & 0x01;

			uint8_t pes_header_data_length = buffer[pos++];
			if ((size_t)pos + pes_header_data_length > size)
				return false;
			int pos_header = pos;

			if (pts_dts_flags == 2)
			{
				if ((size_t)pos + 5 > size)
					return false;
				pts = ((uint64_t)buffer[pos] & 0x0e) << 29;
				pts |= ((uint64_t)buffer[pos + 1]) << 22;
				pts |= ((uint64_t)buffer[pos + 2] & 0xfe) << 14;
				pts |= ((uint64_t)buffer[pos + 3]) << 7;
				pts |= ((uint64_t)buffer[pos + 4] & 0xfe) >> 1;
				pos += 5;
			}
			else if (pts_dts_flags == 3)
			{
				if ((size_t)pos + 10 > size)
					return false;
				pts = ((uint64_t)buffer[pos] & 0x0e) << 29;
				pts |= ((uint64_t)buffer[pos + 1]) << 22;
				pts |= ((uint64_t)buffer[pos + 2] & 0xfe) << 14;
				pts |= ((uint64_t)buffer[pos + 3]) << 7;
				pts |= ((uint64_t)buffer[pos + 4] & 0xfe) >> 1;
				pos += 5;

				dts = ((uint64_t)buffer[pos] & 0x0e) << 29;
				dts |= ((uint64_t)buffer[pos + 1]) << 22;
				dts |= ((uint64_t)buffer[pos + 2] & 0xfe) << 14;
				dts |= ((uint64_t)buffer[pos + 3]) << 7;
				dts |= ((uint64_t)buffer[pos + 4] & 0xfe) >> 1;
				pos += 5;
			}

			if (escr_flag == 1)
			{
				if ((size_t)pos + 6 > size)
					return false;
				escr_base = ((uint64_t)buffer[pos] & 0x38) << 27;
				escr_base |= ((uint64_t)buffer[pos] & 0x3) << 28;
				escr_base |= ((uint64_t)buffer[pos + 1]) << 20;
				escr_base |= ((uint64_t)buffer[pos + 2] & 0xf8) << 12;
				escr_base |= ((uint64_t)buffer[pos + 2] & 0x3) << 13;
				escr_base |= ((uint64_t)buffer[pos + 3]) << 5;
				escr_base |= ((uint64_t)buffer[pos + 4] & 0xf8) >> 3;
				
				escr_extension |= ((uint16_t)buffer[pos + 4] & 0x3) << 7;
				escr_extension |= ((uint16_t)buffer[pos + 5] & 0xfe) >> 1;
				pos += 6;
			}
			if (es_rate_flag == 1)
			{
				if ((size_t)pos + 3 > size)
					return false;
				es_rate = ((uint32_t)buffer[pos] & 0x7f) << 14;
				es_rate |= ((uint32_t)buffer[pos + 1]) << 7;
				es_rate |= ((uint32_t)buffer[pos + 2]&0xfe) >> 1;
				pos += 3;
			}
			if (dsm_trick_mode_flag == 1)
			{
				if ((size_t)pos + 1 > size)
					return false;
				uint8_t v = buffer[pos++];
				trick_mode_control = (v & 0xe0) >> 5;
				trick_mode_val = v & 0x1f;
			}
			if (additional_copy_info_flag == 1)
			{
				if ((size_t)pos + 1 > size)
					return false;
				uint8_t v = buffer[pos++];
				additional_copy_info = v & 0x7f;
			}
			if (pes_crc_flag == 1)
			{
				if ((size_t)pos + 2 > size)
					return false;
				previous_pes_packet_crc |= buffer[pos] << 8;
				previous_pes_packet_crc |= buffer[pos+1];
				pos += 2;
			}
			if (pes_extension_flag==1)
			{
				if ((size_t)pos + 1 > size)
					return false;
				pes_private_data_flag = (buffer[pos] & 0x80) >> 7;
				pack_header_field_flag = (buffer[pos] & 0x40) >> 6;
				program_packer_sequence_counter_flag = (buffer[pos] & 0x20) >> 5;
				p_std_buffer_flag = (buffer[pos] & 0x10) >> 4;
				pes_extension_flag_2= (buffer[pos] & 0x1);
				pos++;
				if (pes_private_data_flag == 1)
				{
					if ((size_t)pos + 16 > size)
						return false;
					memcpy(pes_private_data, buffer + pos, 16);
					pos += 16;
				}
				if (pack_header_field_flag == 1)
				{
					if ((size_t)pos + 1 > size)
						return false;
					uint8_t pack_field_length = buffer[pos++];
					if ((size_t)pos + pack_field_length > size)
						return false;
					headers.append((const char*)(buffer + pos), pack_field_length);
					pos += pack_field_length;
				}
				if (program_packer_sequence_counter_flag == 1)
				{
					if ((size_t)pos + 2 > size)
						return false;
					program_packet_sequence_counter = buffer[pos] & 0x7f;
					mpeg1_mpeg2_indentifier = (buffer[pos + 1] & 0x40) >> 6;
					original_stuff_length= (buffer[pos + 1] & 0x3f);
					pos += 2;
				}
				if (p_std_buffer_flag == 1)
				{
					if ((size_t)pos + 2 > size)
						return false;
					p_std_buffer_scale= (buffer[pos] & 0x20) >> 5;
					p_std_buffer_size |= (buffer[pos] & 0x1f) << 8;
					p_std_buffer_size |= (buffer[pos+1]);
					pos += 2;
				}
				if (pes_extension_flag_2 == 1)
				{
					pes_extension_field_length = (buffer[pos] & 0x7f);
					if ((size_t)pos + pes_extension_field_length > size)
						return false;
					pos += pes_extension_field_length + 1; //reserved
				}
			}

			stuffing_length = (int)pes_header_data_length - (pos - pos_header);
			if (stuffing_length < 0)
			{
				return false;
			}
			if ((size_t)pos + stuffing_length > size) {
				return false;
			}
			pos += stuffing_length;
			
			int data_size = (int)size - pos;
			if (data_size < 0)
			{
				return false;
			}
			pes_packet_data_byte.append((const char*)buffer + pos, data_size);
			pos += data_size;
			return true;
		}

		bool pes2::deserialize2(const uint8_t* buffer, size_t size)
		{
			int pos = 0;
			
			pes_packet_data_byte.append((const char*)buffer + pos, size);
			pos += (int)size;
			return true;
		}

		bool pes2::deserialize3(const uint8_t* buffer, size_t size)
		{
			int pos = 0;
			// padding bytes
			pos += (int)size;
			return true;
		}



		std::string pes2::serialize(uint8_t stream_id)
		{
			if (stream_id != PS_STREAM_ID_MAP
				&& stream_id != PS_STREAM_ID_PADDING
				&& stream_id != PS_STREAM_ID_PRIVATE_STREAM2
				&& stream_id != PS_STREAM_ID_ECM
				&& stream_id != PS_STREAM_ID_EMM
				&& stream_id != PS_STREAM_ID_DIRECTORY
				&& stream_id != PS_STREAM_ID_DSMCC
				&& stream_id != PS_STREAM_ID_ITU_T_H222_1)
			{
				return serialize1();
			}
			else if (stream_id == PS_STREAM_ID_MAP
				|| stream_id == PS_STREAM_ID_PRIVATE_STREAM2
				|| stream_id == PS_STREAM_ID_ECM
				|| stream_id == PS_STREAM_ID_EMM
				|| stream_id == PS_STREAM_ID_DIRECTORY
				|| stream_id == PS_STREAM_ID_DSMCC
				|| stream_id == PS_STREAM_ID_ITU_T_H222_1)
			{
				return serialize2();
			}
			else if (stream_id == PS_STREAM_ID_PADDING)
			{
				return serialize3();
			}
			return "";
		}

		std::string pes2::serialize1()
		{
			std::string buffer;
			uint8_t v = 0;
			v |= 0xc0;
			v |= pes_scrambling_control << 4;
			v |= pes_priority << 3;
			v |= data_alignment_indicator << 2;
			v |= copyright << 1;
			v |= original_or_copy;
			buffer.append(1, (const char)v);

			v = 0;
			v |= pts_dts_flags << 6;
			v |= escr_flag << 5;
			v |= es_rate_flag << 4;
			v |= dsm_trick_mode_flag << 3;
			v |= additional_copy_info_flag << 2;
			v |= pes_crc_flag << 1;
			v |= pes_extension_flag;
			buffer.append(1, (const char)v);

			buffer.append(1, 0);
			size_t pes_header_data_length_pos=buffer.size()-1;
			uint8_t pes_header_data_length = 0;

			if (pts_dts_flags == 2)
			{
				v = 0x20;
				v |= (pts & 0x1c0000000) >> 29;
				v |= 1;
				buffer.append(1, (const char)v);

				v = (uint8_t)((pts & 0x3fc00000) >> 22);
				buffer.append(1, (const char)v);

				v = (uint8_t)((pts & 0x3f8000) >> 14);
				buffer.append(1, (const char)v);

				v = (uint8_t)((pts & 0x7F80) >> 7);
				buffer.append(1, (const char)v);

				v = (pts & 0x7F) << 1 | 0x01;
				buffer.append(1, (const char)v);

				pes_header_data_length += 5;
			}
			else if (pts_dts_flags == 3)
			{
				v = 0x30;
				v |= (pts & 0x1c0000000) >> 29;
				v |= 1;
				buffer.append(1, (const char)v);

				v = (uint8_t)((pts & 0x3fc00000) >> 22);
				buffer.append(1, (const char)v);

				v = (uint8_t)((pts & 0x3f8000) >> 14);
				buffer.append(1, (const char)v);

				v = (uint8_t)((pts & 0x7F80) >> 7);
				buffer.append(1, (const char)v);

				v = (uint8_t)((pts & 0x7F) << 1 | 0x01);
				buffer.append(1, (const char)v);

				pes_header_data_length += 5;


				v = 0x10;
				v |= (uint8_t)((dts & 0x1c0000000) >> 29);
				v |= 1;
				buffer.append(1, (const char)v);

				v = (uint8_t)((dts & 0x3fc00000) >> 22);
				buffer.append(1, (const char)v);

				v = (uint8_t)((dts & 0x3f8000) >> 14);
				buffer.append(1, (const char)v);

				v = (uint8_t)((dts & 0x7F80) >> 7);
				buffer.append(1, (const char)v);

				v = (uint8_t)((dts & 0x7F) << 1) | 0x01;
				buffer.append(1, (const char)v);

				pes_header_data_length += 5;
			}
			
			if (escr_flag == 1)
			{
				uint8_t v = 0xc0;
				v |= (uint8_t)((escr_base & 0x1c0000000) >> 27);
				v |= 0x04; //marker bit
				v |= (uint8_t)((escr_base & 0x30000000) >> 28);
				buffer.append(1, (const char)v);

				v= (uint8_t)((escr_base & 0xff00000) >> 20);
				buffer.append(1, (const char)v);

				v = (uint8_t)((escr_base & 0xf8000) >> 12);
				v |= 0x04; //marker bit
				v |= (uint8_t)((escr_base & 0x6000) >> 13);
				buffer.append(1, (const char)v);

				v = (uint8_t)((escr_base & 0x1fe0) >> 5);
				buffer.append(1, (const char)v);

				v = (uint8_t)((escr_base & 0x1f) <<3);
				v |= 0x04; //marker bit
				v |= (uint8_t)((escr_extension & 0x180) >> 7);
				buffer.append(1, (const char)v);

				v = (uint8_t)(((escr_extension & 0x7F) << 1));
				v |= 0x01;//marker bit
				buffer.append(1, (const char)v);

				pes_header_data_length += 6;
			}
			if (es_rate_flag)
			{
				uint8_t v = 0x80; // marker bit
				v |= (uint8_t)((es_rate & 0x3f8000) >> 15);
				buffer.append(1, (const char)v);

				v= (uint8_t)((es_rate & 0x7f80) >> 7);
				buffer.append(1, (const char)v);

				v = (uint8_t)((es_rate & 0x7f) << 1);
				v |= 0x01;// marker bit
				buffer.append(1, (const char)v);

				pes_header_data_length += 3;
			}

			if (dsm_trick_mode_flag == 1)
			{
				v = (trick_mode_control << 5)| trick_mode_val;
				buffer.append(1, (const char)v);

				pes_header_data_length += 1;
			}
			if (additional_copy_info_flag == 1)
			{
				v = 0x80 | additional_copy_info;
				buffer.append(1, (const char)v);

				pes_header_data_length += 1;
			}
			if (pes_crc_flag == 1)
			{
				v = (uint8_t)((previous_pes_packet_crc & 0xFF00) >> 8);
				buffer.append(1, (const char)v);
				v = (uint8_t)(previous_pes_packet_crc & 0x00FF);
				buffer.append(1, (const char)v);
				pes_header_data_length += 2;
			}
			if (pes_extension_flag == 1)
			{
				v = 0;
				v |= pes_private_data_flag << 7;
				v |= pack_header_field_flag << 6;
				v |= program_packer_sequence_counter_flag << 5;
				v |= p_std_buffer_flag << 4;
				v |= 0x0e;//reserved;
				v |= pes_extension_flag_2;
				buffer.append(1, (const char)v);
				pes_header_data_length += 1;

				if (pes_private_data_flag == 1)
				{
					buffer.append((const char*)pes_private_data, 16);
					pes_header_data_length += 16;
				}
				if (pack_header_field_flag == 1)
				{
					uint8_t pack_field_length = (uint8_t)headers.size();
					buffer.append(1, (const char)pack_field_length);
					buffer.append(headers);

					pes_header_data_length += (pack_field_length + 1);
				}
				if (program_packer_sequence_counter_flag == 1)
				{
					v = 0x80;//marker bit
					v |= program_packet_sequence_counter;
					buffer.append(1, (const char)v);

					v = 0x80;//marker bit
					v |= mpeg1_mpeg2_indentifier << 6;
					v |= original_stuff_length;
					buffer.append(1, (const char)v);

					pes_header_data_length += 2;
				}
				if (p_std_buffer_flag == 1)
				{
					v = 0x40;//  '01'
					v |= p_std_buffer_scale << 5;
					v |= (uint8_t)((p_std_buffer_size & 0x1f00) >> 8);
					buffer.append(1, (const char)v);

					v = (uint8_t)(p_std_buffer_size & 0xFF);
					buffer.append(1, (const char)v);

					pes_header_data_length += 2;
				}
				if (pes_extension_flag_2 == 1)
				{
					v = 0x80 | pes_extension_field_length;
					buffer.append(1, (const char)v);
					for (uint8_t i = 0; i < pes_extension_field_length; i++)
					{
						buffer.append(1, (const char)0xFF);
					}
					pes_header_data_length += (1 + pes_extension_field_length);
				}
			}

			for (uint8_t i = 0; i < stuffing_length; i++)
			{
				buffer.append(1, (const char)0xFF);
			}
			pes_header_data_length += stuffing_length;

			//replace pes_header_data_length
			buffer[pes_header_data_length_pos] = pes_header_data_length;

			// packet date
			buffer.append(pes_packet_data_byte);

			return buffer;
		}

		std::string pes2::serialize2()
		{
			return pes_packet_data_byte;
		}

		std::string pes2::serialize3()
		{
			std::string buffer;
			buffer.reserve(stuffing_length);
			for (uint8_t i = 0; i < stuffing_length; i++)
			{
				buffer.append(1, (const char)0xFF);
			}
			return buffer;
		}

		void pes2::reset()
		{
			pes_scrambling_control = 0;
			pes_priority = 0;
			data_alignment_indicator = 0;
			copyright = 0;
			original_or_copy = 0;
			pts_dts_flags = 0;
			escr_flag = 0;
			es_rate_flag = 0;
			dsm_trick_mode_flag = 0;
			additional_copy_info_flag = 0;
			pes_crc_flag = 0;
			pes_extension_flag = 0;

			pts = 0;
			dts = 0;

			escr_base = 0;
			escr_extension = 0;

			es_rate = 0;

			trick_mode_control = 0;
			trick_mode_val = 0;

			additional_copy_info = 0;
			previous_pes_packet_crc = 0;

			pes_private_data_flag = 0;
			pack_header_field_flag = 0;
			program_packer_sequence_counter_flag = 0;
			p_std_buffer_flag = 0;
			pes_extension_flag_2 = 0;
			memset(pes_private_data,0,sizeof(pes_private_data));

			headers="";

			program_packet_sequence_counter = 0;
			mpeg1_mpeg2_indentifier = 0;
			original_stuff_length = 0;

			p_std_buffer_scale = 0;
			p_std_buffer_size = 0;
			pes_extension_field_length = 0;

			stuffing_length = 0;
			pes_packet_data_byte = "";
		}


		void pes2::set_trick_mode_control_field_id(uint8_t v)
		{
			trick_mode_val |= ((v & 0x03) << 3);
		}

		void pes2::set_trick_mode_control_intra_slice_refresh(uint8_t v)
		{
			trick_mode_val |= ((v & 0x01) << 1);
		}

		void pes2::set_trick_mode_control_frequency_truncation(uint8_t v)
		{
			trick_mode_val |= (v & 0x03);
		}

		uint8_t pes2::get_trick_mode_control_field_id()
		{
			return (trick_mode_val & 0x18) >> 3;
		}

		uint8_t pes2::set_trick_mode_control_intra_slice_refresh()
		{
			return (trick_mode_val & 0x04) >> 1;
		}

		uint8_t pes2::set_trick_mode_control_frequency_truncation()
		{
			return (trick_mode_val & 0x03);
		}
	}
}


