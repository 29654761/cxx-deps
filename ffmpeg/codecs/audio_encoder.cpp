#include "audio_encoder.h"

namespace ffmpeg
{
	namespace codecs
	{

		audio_encoder::audio_encoder(const audio_encoder_options& options)
		{
			options_ = options;
		}

		audio_encoder::~audio_encoder()
		{
			close();
		}

		bool audio_encoder::open()
		{
			if (is_opened())
			{
				return false;
			}
			pts_ = 0;
			if (!fifo_.open(options_.format, options_.channels, options_.samplerate * 10))
			{
				close();
				return false;
			}

			const AVCodec* codec = avcodec_find_encoder(options_.codec_id);
			if (!codec)
			{
				close();
				return false;
			}

			context_ = avcodec_alloc_context3(codec);
			if (!context_)
			{
				close();
				return false;
			}

			context_->sample_fmt = options_.format;
			context_->sample_rate = options_.samplerate;
			context_->thread_count = options_.thread_count;
			context_->bit_rate = 64000; //64Kb
			av_channel_layout_default(&context_->ch_layout, options_.channels);

			AVDictionary* opts = nullptr;

			if (options_.opus_inband_fec)
			{
				av_dict_set(&opts, "fec", "1", 0);

				if (options_.opus_fec_packet_loss > 0)
				{
					av_dict_set_int(&opts, "packet_loss", options_.opus_fec_packet_loss, 0);
				}
			}

			int r = avcodec_open2(context_, codec, &opts);

			av_dict_free(&opts);
			if (r < 0)
			{
				close();
				return false;
			}

			if (options_.frame_size > 0) {
				frame_size_ = options_.frame_size;
			}
			else {
				frame_size_ = context_->frame_size;
			}

			return true;
		}

		void audio_encoder::close()
		{
			if (context_)
			{
				avcodec_close(context_);
				avcodec_free_context(&context_);
			}
			fifo_.close();
		}

		/*
		bool audio_encoder::send_frame(const AVFrame* inframe, AVFrame** encframe)
		{
			int r = 0;

			if (ts_ == 0)
				ts_ = inframe->pts;

			AVFrame* frame2 = nullptr;

			if (frame_size_ > 0)
			{
				r = fifo_.write((void**)inframe->data, inframe->nb_samples);
				if (r <= 0)
				{
					return false;
				}

				while (fifo_.samples() >= frame_size_)
				{
					int64_t ts = ts_.exchange(0);

					frame2 = av_frame_alloc();
					if (!frame2) {
						return false;
					}
					av_channel_layout_copy(&frame2->ch_layout, &context_->ch_layout);
					frame2->format = (int)context_->sample_fmt;
					frame2->nb_samples = frame_size_;
					frame2->sample_rate = context_->sample_rate;
					frame2->duration = frame2->nb_samples;
					frame2->pts = ts;
					frame2->time_base = inframe->time_base;
					r = av_frame_get_buffer(frame2, 0);
					if (r < 0)
					{
						av_frame_free(&frame2);
						return false;
					}
					if (!fifo_.read_samples((void**)frame2->data, frame2->nb_samples))
					{
						av_frame_free(&frame2);
						return false;
					}

					if (encframe) {
						*encframe = av_frame_clone(frame2);
					}

					r = avcodec_send_frame(context_, frame2);
					av_frame_free(&frame2);
					return r >= 0;
				}
			}
			else 
			{
				frame2 = av_frame_clone(inframe);
				if (!frame2) {
					return false;
				}

				if (encframe) {
					*encframe = av_frame_clone(frame2);
				}

				r = avcodec_send_frame(context_, frame2);
				av_frame_free(&frame2);
				return r >= 0;
			}

			
		}
		*/

		bool audio_encoder::receive_packet(AVPacket* packet)
		{
			int r = avcodec_receive_packet(context_, packet);
			if (r < 0)
			{
				return false;
			}

			packet->time_base = context_->time_base;

			return true;
		}


		bool audio_encoder::input(const AVFrame* frame)
		{
			if (frame_size_ > 0)
			{
				if (pts_ == 0) {
					pts_ = frame->pts;
				}
				int r = fifo_.write((void**)frame->data, frame->nb_samples);
				if (r <= 0)
				{
					return false;
				}

				while (fifo_.samples() >= frame_size_)
				{
					AVFrame* frame2 = av_frame_alloc();
					if (!frame2) {
						return false;
					}
					av_channel_layout_copy(&frame2->ch_layout, &context_->ch_layout);
					frame2->format = (int)context_->sample_fmt;
					frame2->nb_samples = frame_size_;
					frame2->sample_rate = context_->sample_rate;
					frame2->duration = frame2->nb_samples;
					frame2->pts = pts_;
					frame2->time_base = frame->time_base;
					r = av_frame_get_buffer(frame2, 0);
					if (r < 0)
					{
						av_frame_free(&frame2);
						return false;
					}
					if (!fifo_.read_samples((void**)frame2->data, frame2->nb_samples))
					{
						av_frame_free(&frame2);
						return false;
					}


					r = avcodec_send_frame(context_, frame2);
					if (r >= 0)
					{
						AVPacket pkt = {};
						while (receive_packet(&pkt))
						{
							on_packet.invoke(&pkt, frame2);
							av_packet_unref(&pkt);
						}
					}
					pts_ += frame2->duration;
					av_frame_free(&frame2);

				}
				return true;
			}
			else
			{
				AVFrame* frame2 = av_frame_clone(frame);
				if (!frame2) {
					return false;
				}


				int r = avcodec_send_frame(context_, frame2);
				if (r >= 0)
				{
					AVPacket pkt = {};
					while (receive_packet(&pkt))
					{
						on_packet.invoke(&pkt, frame2);
						av_packet_unref(&pkt);
					}
				}
				av_frame_free(&frame2);
				return true;
			}

			
		}

		bool audio_encoder::flush()
		{
			if (!context_)
				return false;
			avcodec_flush_buffers(context_);
			return true;
		}

	}
}

