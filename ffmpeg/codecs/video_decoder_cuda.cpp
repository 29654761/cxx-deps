#include "video_decoder_cuda.h"

namespace ffmpeg
{
	namespace codecs
	{

		video_decoder_cuda::video_decoder_cuda(const video_decoder_options& options)
			:video_decoder(options)
		{
			if (options_.codec_id == AVCodecID::AV_CODEC_ID_H264)
			{
				codec_name_ = "h264_cuvid";
			}
			else if (options_.codec_id == AVCodecID::AV_CODEC_ID_HEVC)
			{
				codec_name_ = "hevc_cuvid";
			}
			else if (options_.codec_id == AVCodecID::AV_CODEC_ID_VP8)
			{
				codec_name_ = "vp8_cuvid";
			}
			else if (options_.codec_id == AVCodecID::AV_CODEC_ID_VP9)
			{
				codec_name_ = "vp9_cuvid";
			}
		}

		video_decoder_cuda::~video_decoder_cuda()
		{
			close();
		}

		bool video_decoder_cuda::open()
		{
			const AVCodec* codec = avcodec_find_decoder_by_name(codec_name_.c_str());
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
			context_->thread_count = options_.thread_count;
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

			int r = avcodec_open2(context_, codec, nullptr);
			if (r != 0)
			{
				close();
				return false;
			}

			return true;
		}


		bool video_decoder_cuda::receive_frame(AVFrame* frame)
		{
			AVFrame gpu = {};
			if (!video_decoder::receive_frame(&gpu))
				return false;

			/*AVPixelFormat* fmts = nullptr;
			int r=av_hwframe_transfer_get_formats(gpu.hw_frames_ctx, AVHWFrameTransferDirection::AV_HWFRAME_TRANSFER_DIRECTION_FROM, &fmts, 0);
			if (r >= 0)
			{
				if (fmts[0] != AVPixelFormat::AV_PIX_FMT_NONE)
				{
					frame->format = fmts[0];
				}
			}
			av_free(fmts);*/
			frame->format = AVPixelFormat::AV_PIX_FMT_NV12;
			
			int r=av_hwframe_transfer_data(frame, &gpu, 0);
			av_frame_unref(&gpu);
			if (r < 0)
				return false;

			return true;
		}

	}
}




