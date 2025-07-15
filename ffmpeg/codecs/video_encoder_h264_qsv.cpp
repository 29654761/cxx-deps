#include "video_encoder_h264_qsv.h"


namespace ffmpeg
{
	namespace codecs
	{
        video_encoder_h264_qsv::video_encoder_h264_qsv(const video_encoder_options& options)
			:video_encoder(options)
		{
            codec_name_ = "h264_qsv";
		}

        video_encoder_h264_qsv::~video_encoder_h264_qsv()
		{
            close();
		}

		bool video_encoder_h264_qsv::open()
		{
            if (is_opened())
            {
                return false;
            }

            const AVCodec* codec = avcodec_find_encoder_by_name(codec_name_.c_str());
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

            hwcfg_ = avcodec_get_hw_config(codec, 0);
            if (!hwcfg_)
            {
                close();
                return false;
            }

            AVBufferRef* devctx = nullptr;
            av_hwdevice_ctx_create(&devctx, hwcfg_->device_type, nullptr, nullptr, 0);
            if (!devctx)
            {
                close();
                return false;
            }
            context_->hw_device_ctx = av_buffer_ref(devctx);

            context_->bit_rate = options_.bitrate;
            context_->rc_max_rate = options_.max_bitrate;
            context_->rc_min_rate = options_.min_bitrate;
            context_->rc_buffer_size = 2*(int)context_->rc_max_rate;
            //context_->bit_rate_tolerance = 1024 * 1024 * 8 * 2;
            context_->width = options_.width;
            context_->height = options_.height;
            context_->time_base.num = 1;
            context_->time_base.den = options_.framerate;
            context_->framerate.num = options_.framerate;
            context_->framerate.den = 1;
            context_->me_subpel_quality = 4;
            context_->delay = 0;
            context_->pix_fmt = hwcfg_->pix_fmt;
            context_->sw_pix_fmt = options_.format;
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
            //av_opt_set(context_->priv_data, "preset", "fast", 0);
            //av_opt_set(context_->priv_data, "preset", "medium", 0);
            av_opt_set(context_->priv_data, "preset", "superfast", 0);
            av_opt_set(context_->priv_data, "tune", "zerolatency", 0);

            std::string vbv_maxrate = std::to_string(options_.max_bitrate);
            std::string vbv_bufsize = std::to_string(options_.max_bitrate * 2);
            av_opt_set(context_->priv_data, "maxrate", vbv_maxrate.c_str(), 0);
            av_opt_set(context_->priv_data, "bufsize", vbv_bufsize.c_str(), 0);

            int r = avcodec_open2(context_, codec, nullptr);
            if (r != 0)
            {
                char err[64] = {};
                av_make_error_string(err, 64, r);
                close();
                return false;
            }
            return true;
		}
        
	}

}



