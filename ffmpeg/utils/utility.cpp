
#include "utility.h"
#include <assert.h>
#include <fstream>

namespace ffmpeg
{
	namespace utils
	{

		void utility::set_log_level(int level)
		{
			av_log_set_level(level);
		}

		void utility::mix_audio_frame(AVFrame* mixed_frame, const AVFrame* src_frame)
		{
			assert(mixed_frame->nb_samples == src_frame->nb_samples);
			assert(mixed_frame->format == src_frame->format);
			assert(mixed_frame->ch_layout.nb_channels == src_frame->ch_layout.nb_channels);

			for (int i = 0; i < 8; i++)
			{
				uint8_t* mixed_buffer = mixed_frame->data[i];
				uint8_t* src_buffer = src_frame->data[i];
				if (!mixed_buffer || !src_buffer)
				{
					break;
				}

				if (mixed_frame->format == (int)AV_SAMPLE_FMT_FLTP)
				{
					mix_audio_buffer_float((float*)mixed_buffer, (float*)src_buffer, mixed_frame->nb_samples);
				}
				else if (mixed_frame->format == (int)AV_SAMPLE_FMT_S16)
				{
					mix_audio_buffer_short((short*)mixed_buffer, (short*)src_buffer, mixed_frame->nb_samples);
				}
			}
		}

		void utility::mix_audio_buffer_short(short* mixed_buffer, short* src_buffer, int samples)
		{
			for (int i = 0; i < samples; i++)
			{
				int v = mixed_buffer[i];
				v += src_buffer[i];
				if (v > 32767)
				{
					v = 32767;
				}
				else if (v < -32768)
				{
					v = -32768;
				}
				mixed_buffer[i] = (short)v;
			}
		}

		void utility::mix_audio_buffer_float(float* mixed_buffer, float* src_buffer, int samples)
		{
			for (int i = 0; i < samples; i++)
			{
				mixed_buffer[i] += src_buffer[i];
				if (mixed_buffer[i] > (float)0.99999999)
				{
					mixed_buffer[i] = (float)0.99999999;
				}
				else if (mixed_buffer[i] < (float)-0.99999999)
				{
					mixed_buffer[i] = (float)-0.99999999;
				}
			}
		}

		void utility::audio_frame_fill_zero(AVFrame* frame)
		{

			for (int i = 0; i < 8; i++)
			{
				uint8_t* buf = frame->data[i];
				if (!buf)
				{
					break;
				}
				int linesize = frame->linesize[0]; //always 0;
				memset(buf, 0, linesize);
			}
		}

		int64_t utility::read_m3u8_duration(const std::string& filename)
		{
			double duration = 0;
			std::ifstream file(filename, std::ios::binary);
			std::string line;
			while (std::getline(file, line))
			{
				if (line.find("#EXTINF:") == 0)
				{
					std::string sduration = line.substr(8);
					char* endptr = nullptr;
					double d=strtod(sduration.c_str(), &endptr);
					duration += d * AV_TIME_BASE;
				}
			}

			return (int64_t)duration;
		}

	}
}


