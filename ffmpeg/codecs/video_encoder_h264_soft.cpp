#include "video_encoder_h264_soft.h"
#include <string>


namespace ffmpeg
{
	namespace codecs
	{
		video_encoder_h264_soft::video_encoder_h264_soft(const video_encoder_options& options)
			:video_encoder(options)
		{
		}

		video_encoder_h264_soft::~video_encoder_h264_soft()
		{
            close();
		}

		bool video_encoder_h264_soft::open()
		{
            if (is_opened())
            {
                return false;
            }

            const AVCodec* codec = avcodec_find_encoder(AVCodecID::AV_CODEC_ID_H264);
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

            context_->bit_rate = options_.bitrate;
            context_->rc_max_rate = options_.max_bitrate;
            context_->rc_min_rate = options_.min_bitrate;
            context_->rc_buffer_size = 2*(int)context_->rc_max_rate;
            //context_->bit_rate_tolerance = 1024 * 1024 * 2;
            context_->width = options_.width;
            context_->height = options_.height;
            context_->time_base.num = 1;
            context_->time_base.den = 90000;
            context_->framerate.num = options_.framerate;
            context_->framerate.den = 1;
            context_->me_subpel_quality = 4;
            context_->delay = 0;
            context_->pix_fmt = options_.format;
            context_->thread_count = options_.thread_count;
            context_->refs = options_.refs;
            context_->profile = options_.profile;
            //context_->profile = FF_PROFILE_H264_HIGH_444;
            context_->level = options_.level;
            context_->gop_size = options_.gop_size;
            context_->keyint_min = 10;
            context_->max_b_frames = 0;
            context_->has_b_frames = 0;
            context_->flags = 0;
            context_->me_pre_cmp = 2;
            context_->qmin = 10;
            context_->qmax = 30;
            context_->me_range = 16;
            context_->me_sub_cmp = 7;
            //av_opt_set(context_->priv_data, "preset", "fast", 0);
            //av_opt_set(context_->priv_data, "preset", "medium", 0);
            av_opt_set(context_->priv_data, "preset", "superfast", 0);
            //av_opt_set(context_->priv_data, "tune", "zerolatency", 0);


            //H.264 & H.264  kbit/s;  others bit/s
            std::string vbv_maxrate = std::to_string(options_.max_bitrate / 1024);
            std::string vbv_bufsize = std::to_string(options_.max_bitrate * 2 / 1024);
            av_opt_set(context_->priv_data, "vbv-maxrate", vbv_maxrate.c_str(), 0);
            av_opt_set(context_->priv_data, "vbv-bufsize", vbv_bufsize.c_str(), 0);
            av_opt_set(context_->priv_data, "vbv-init", "0.9", 0);
            //av_opt_set(context_->priv_data, "crf", "23", 0);


            int r = avcodec_open2(context_, codec, nullptr);
            if (r != 0)
            {
                close();
                return false;
            }
            return true;
		}
	}
}



