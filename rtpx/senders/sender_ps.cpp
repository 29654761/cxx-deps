/**
 * @file sender_ps.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#include "sender_ps.h"
#include "../mpeg/ps_frame.h"
#include "../h264/h264.h"

namespace rtpx
{
	sender_ps::sender_ps(uint8_t pt, uint32_t ssrc, media_type_t mt, const sdp_format& fmt, spdlogger_ptr log)
		:sender(pt,ssrc,mt,fmt, log)
	{
	}

	sender_ps::~sender_ps()
	{
	}


	bool sender_ps::send_frame(const uint8_t* frame, uint32_t size, uint32_t duration)
	{
        std::unique_lock<std::shared_mutex>lk(mutex_);
        size_t offset = 0;
		size_t nal_start = 0, nal_size = 0;
        bool islast = false;
        std::vector<std::string> nals;
        while (h264::find_next_nal(frame, size, offset, nal_start, nal_size, islast))
        {
            std::string nal;
            nal.assign((const char*)(frame + nal_start), nal_size);
            nals.push_back(nal);
        }
		return send_nal(duration, nals);
	}

	bool sender_ps::send_nal(uint32_t duration,const std::vector<std::string>& nals)
	{
		mpeg::ps_frame psframe;
		uint64_t ts = timestamp_ + duration;
		psframe.psh.timestamp = ts;
		psframe.psh.program_mux_rate = 10036;
		psframe.psh.pack_stuffing_length = 6;

		for (auto itr = nals.begin(); itr != nals.end(); itr++)
		{
			int64_t nal_start=h264::find_nal_start((const uint8_t*)itr->data(), itr->size());
			if (nal_start >= 0) {
				h264::nal_type_t nal_type=h264::get_nal_type((*itr)[(size_t)nal_start]);
				if (h264::is_key_frame(nal_type))
				{
					if (!psframe.has_header(PS_STREAM_ID_SYSTEM_HEADER))
					{
						mpeg::ps_block block;
						block.start_code = PS_STREAM_ID_SYSTEM_HEADER;
						mpeg::sys_header sys;
						sys.rate_bound = 10036;
						sys.video_bound = 1;
						sys.audio_bound = 1;
						sys.system_video_lock_flag = 1;
						sys.system_audio_lock_flag = 1;
						{
							mpeg::sys_header::stream_t stream;
							stream.stream_id = PS_STREAM_ID_VIDEO_START;
							stream.flag = 0x03;
							stream.p_std_buffer_bound_scale = 0x01;
							stream.p_std_buffer_size_bound = 0x0080;
							sys.streams.push_back(stream);
						}
						{
							mpeg::sys_header::stream_t stream;
							stream.stream_id = PS_STREAM_ID_AUDIO_START;
							stream.flag = 0x03;
							stream.p_std_buffer_bound_scale = 0x00;
							stream.p_std_buffer_size_bound = 0x0008;
							sys.streams.push_back(stream);
						}
						block.block = sys.serialize();
						psframe.blocks.push_back(block);
					}

					if (!psframe.has_header(PS_STREAM_ID_MAP))
					{
						mpeg::ps_block block;
						block.start_code = PS_STREAM_ID_MAP;
						mpeg::psm2 psm;
						psm.current_next_indicator = 1;
						psm.program_stream_map_version = 0x17;
						block.block = psm.serialize();
						psframe.blocks.push_back(block);
					}
				}

				
				for (size_t pos = 0; pos < itr->size();)
				{
					mpeg::pes2 pes;
					if (h264::is_key_frame(nal_type))
					{
						pes.pes_priority = 1;
					}
					else
					{
						pes.pes_priority = 0;
					}
					pes.data_alignment_indicator = 0;
					pes.pts_dts_flags = 2;
					pes.pts = ts;
					pes.stuffing_length = 2;
					pes.pes_packet_data_byte.append(3, (const char)0);
					pes.pes_packet_data_byte.append(1, (const char)1);
					uint16_t len = std::min<uint16_t>(65000, (uint16_t)(itr->size() - pos));
					pes.pes_packet_data_byte.append(pos + itr->data(), len);
					pos += len;


					mpeg::ps_block block;
					block.start_code = PS_STREAM_ID_VIDEO_START;
					block.block = pes.serialize(block.start_code);
					psframe.blocks.push_back(block);
				}
				
			}
		}

		std::string frame=psframe.serialize();

		return send_rtp_frame((const uint8_t*)frame.data(),(uint32_t)frame.size(),duration);
	}

	bool sender_ps::send_rtp_frame(const uint8_t* frame, uint32_t size, uint32_t duration)
	{
		uint32_t payload_duration = 0;
		for (uint32_t index = 0; index * MAX_RTP_PAYLOAD_SIZE < size; index++)
		{
			uint32_t offset = (index == 0) ? 0 : (index * MAX_RTP_PAYLOAD_SIZE);
			uint32_t payload_length = (offset + MAX_RTP_PAYLOAD_SIZE < size) ? MAX_RTP_PAYLOAD_SIZE : size - offset;


			packet_ptr pkt = std::make_shared<packet>((uint8_t)format_.pt, ssrc_, seq_, timestamp_);

			// RFC3551 specifies that for audio the marker bit should always be 0 except for when returning
			// from silence suppression. For video the marker bit DOES get set to 1 for the last packet
			// in a frame.
			pkt->mutable_header()->m = 0;

			pkt->set_payload((const uint8_t*)(frame + offset), payload_length);
			this->send_packet(pkt);

			//logger.LogDebug($"send audio { audioRtpChannel.RTPLocalEndPoint}->{AudioDestinationEndPoint}.");

			payload_duration = (uint32_t)((payload_length * 1.0 / size) * duration);
			timestamp_ += payload_duration;
		}


		return true;
	}




}
