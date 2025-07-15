#include "video_encoder_vp8_soft.h"
#include <string>


namespace ffmpeg
{
	namespace codecs
	{
        video_encoder_vp8_soft::video_encoder_vp8_soft(const video_encoder_options& options)
			:video_encoder(options)
		{
		}

        video_encoder_vp8_soft::~video_encoder_vp8_soft()
		{
            close();
		}

		bool video_encoder_vp8_soft::open()
		{
            if (is_opened())
            {
                return false;
            }

            const AVCodec* codec = avcodec_find_encoder(AVCodecID::AV_CODEC_ID_VP8);
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
            context_->width = options_.width;
            context_->height = options_.height;
            context_->time_base.num = 1;
            context_->time_base.den = options_.framerate;
            context_->framerate.num = options_.framerate;
            context_->framerate.den = 1;
            context_->me_subpel_quality = 4;
            context_->delay = 0;
            context_->pix_fmt = options_.format;
            context_->gop_size = options_.gop_size;
            context_->thread_count = options_.thread_count;
            context_->refs = options_.refs;
            context_->keyint_min = 10;
            context_->max_b_frames = 0;
            context_->has_b_frames = 0;
            context_->flags = 0;
            context_->me_pre_cmp = 2;
            context_->qmin = 10;
            context_->qmax = 30;
            AVDictionary* opt;
            av_dict_set(&opt, "passes", "1", 0);
            av_dict_set(&opt, "cpu-used", "15", 0);
            av_dict_set(&opt, "rt", "", 0); // realtime setting
            //av_dict_set(&opt, "crf" ,"10",         0);

            //VPX bit/s
            std::string vbv_maxrate = std::to_string(options_.max_bitrate);
            std::string vbv_bufsize = std::to_string(options_.max_bitrate * 2);
            av_opt_set(context_->priv_data, "maxrate", vbv_maxrate.c_str(), 0);
            av_opt_set(context_->priv_data, "bufsize", vbv_bufsize.c_str(), 0);
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



