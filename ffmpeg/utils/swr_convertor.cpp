#include "swr_convertor.h"

namespace ffmpeg
{
	namespace utils
	{

		swr_convertor::swr_convertor()
		{
		}

		swr_convertor::~swr_convertor()
		{
			close();
		}

		bool swr_convertor::open(const AVChannelLayout* out_ch_layout, AVSampleFormat out_format, int out_samplerate,
			const AVChannelLayout* in_ch_layout, AVSampleFormat in_format, int in_samplerate)
		{
			if (is_opened())
			{
				return false;
			}
			av_channel_layout_copy(&out_ch_layout_, out_ch_layout);
			out_format_ = out_format;
			out_samplerate_ = out_samplerate;
			av_channel_layout_copy(&in_ch_layout_, in_ch_layout);
			in_format_ = in_format;
			in_samplerate_ = in_samplerate;

			swr_alloc_set_opts2(&context_, &out_ch_layout_, out_format_, out_samplerate_,
				&in_ch_layout_, in_format_, in_samplerate_, 0, nullptr);
			if (!context_)
			{
				close();
				return false;
			}
			int ret = swr_init(context_);
			if (ret < 0)
			{
				close();
				return false;
			}

			return true;
		}

		void swr_convertor::close()
		{
			if (context_)
			{
				swr_close(context_);
				swr_free(&context_);
			}
		}

		bool swr_convertor::is_changed(const AVChannelLayout* in_ch_layout, AVSampleFormat in_format, int in_samplerate)const
		{
			if (in_ch_layout_.nb_channels != in_ch_layout->nb_channels
				|| in_format_ != in_format || in_samplerate_ != in_samplerate)
			{
				return true;
			}
			return false;
		}

		bool swr_convertor::ensure_open(const AVChannelLayout* out_ch_layout, AVSampleFormat out_format, int out_samplerate,
			const AVChannelLayout* in_ch_layout, AVSampleFormat in_format, int in_samplerate)
		{
			if (is_changed(in_ch_layout, in_format, in_samplerate))
			{
				close();
			}
			if (!is_opened())
			{
				return open(out_ch_layout, out_format, out_samplerate,
					in_ch_layout, in_format, in_samplerate);
			}

			return true;
		}

		AVFrame* swr_convertor::convert_frame(const AVFrame* frame)
		{
			if (out_ch_layout_.nb_channels == frame->ch_layout.nb_channels && (int)out_format_ == frame->format
				&& out_samplerate_ == frame->sample_rate)
			{
				return av_frame_clone(frame);
			}
			
			AVFrame* out_frame = av_frame_alloc();
			if (!out_frame)
				return nullptr;

			out_frame->format = (int)out_format_;
			out_frame->sample_rate = out_samplerate_;
			out_frame->time_base = av_make_q(1, out_samplerate_);
			av_channel_layout_copy(&out_frame->ch_layout, &out_ch_layout_);


			int r = swr_convert_frame(context_, out_frame, frame);
			if (r < 0)
			{
				char msg[1000] = {};
				av_make_error_string(msg, 1000, r);
				return nullptr;
			}
			if (frame->pts != AV_NOPTS_VALUE) {
				out_frame->pts = av_rescale_q(frame->pts, frame->time_base, out_frame->time_base);
			}
			else {
				out_frame->pts = AV_NOPTS_VALUE;
			}
			return out_frame;
		}

		int64_t swr_convertor::convert_sample_count(long samples)
		{
			if (in_samplerate_ < out_samplerate_)
			{
				return av_rescale_rnd(swr_get_delay(context_, in_samplerate_) + samples, out_samplerate_, in_samplerate_, AVRounding::AV_ROUND_UP);
			}
			else if (in_samplerate_ > out_samplerate_)
			{
				return av_rescale_rnd(swr_get_delay(context_, in_samplerate_) + samples, out_samplerate_, in_samplerate_, AVRounding::AV_ROUND_DOWN);
			}
			else
			{
				return samples;
			}
		}

	}
}

