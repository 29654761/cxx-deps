
#pragma once

extern "C"
{
#include <libavutil/frame.h>
}
#include <string>

namespace ffmpeg
{
	namespace utils
	{

		class utility
		{
		public:
			static void set_log_level(int level);

			static void mix_audio_frame(AVFrame* mixed_frame, const AVFrame* src_frame);


			static void mix_audio_buffer_short(short* mixed_buffer, short* src_buffer, int samples);
			static void mix_audio_buffer_float(float* mixed_buffer, float* src_buffer, int samples);
			static void audio_frame_fill_zero(AVFrame* frame);

			static int64_t read_m3u8_duration(const std::string& filename);

		};

	}
}
