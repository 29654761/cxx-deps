#pragma once

extern "C"
{
#include <libswresample/swresample.h>
}

namespace ffmpeg
{
	namespace utils
	{

		class swr_convertor
		{
		public:
			swr_convertor();
			~swr_convertor();

			bool is_opened()const { return context_ != nullptr; }

			bool open(const AVChannelLayout* out_ch_layout, AVSampleFormat out_format, int out_samplerate,
				const AVChannelLayout* in_ch_layout, AVSampleFormat in_format, int in_samplerate);

			void close();

			bool is_changed(const AVChannelLayout* in_ch_layout, AVSampleFormat in_format, int in_samplerate)const;

			bool ensure_open(const AVChannelLayout* out_ch_layout, AVSampleFormat out_format, int out_samplerate,
				const AVChannelLayout* in_ch_layout, AVSampleFormat in_format, int in_samplerate);

			AVFrame* convert_frame(const AVFrame* inframe);

			int64_t convert_sample_count(long samples);
		private:
			SwrContext* context_ = nullptr;
			AVChannelLayout out_ch_layout_ = {};
			AVSampleFormat out_format_ = AVSampleFormat::AV_SAMPLE_FMT_NONE;
			int out_samplerate_ = 0;

			AVChannelLayout in_ch_layout_ = {};
			AVSampleFormat in_format_ = AVSampleFormat::AV_SAMPLE_FMT_NONE;
			int in_samplerate_ = 0;

		};
	}
}

