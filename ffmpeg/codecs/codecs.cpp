#include "codecs.h"

#include "video_decoder_soft.h"
#include "video_decoder_qsv.h"
#include "video_decoder_cuda.h"
#include "video_encoder_h264_cuda.h"
#include "video_encoder_h264_qsv.h"
#include "video_encoder_h264_soft.h"
#include "video_encoder_vp8_cuda.h"
#include "video_encoder_vp8_qsv.h"
#include "video_encoder_vp8_soft.h"

namespace ffmpeg
{
	namespace codecs
	{
		audio_decoder_ptr create_audio_decoder(const audio_decoder_options& options)
		{
			auto decoder=std::make_shared<audio_decoder>(options);
			if (!decoder->open())
			{
				return nullptr;
			}
			return decoder;
		}

		audio_encoder_ptr create_audio_encoder(const audio_encoder_options& options)
		{
			auto encoder = std::make_shared<audio_encoder>(options);
			if (!encoder->open())
			{
				return nullptr;
			}
			return encoder;
		}


		video_decoder_ptr create_video_decoder(const video_decoder_options& options)
		{
			video_decoder_ptr decoder = create_video_decoder_cuda(options);
			if (decoder)
			{
				return decoder;
			}

			//decoder = create_video_decoder_qsv(codec_id);
			//if (decoder)
			//{
			//	return decoder;
			//}

			decoder = create_video_decoder_soft(options);
			return decoder;
		}

		video_decoder_ptr create_video_decoder_soft(const video_decoder_options& options)
		{
			auto decoder = std::make_shared<video_decoder_soft>(options);
			if (!decoder->open())
			{
				return nullptr;
			}
			return decoder;
		}

		video_decoder_ptr create_video_decoder_cuda(const video_decoder_options& options)
		{
			auto decoder = std::make_shared<video_decoder_cuda>(options);
			if (!decoder->open())
			{
				return nullptr;
			}
			return decoder;
		}

		video_decoder_ptr create_video_decoder_qsv(const video_decoder_options& options)
		{
			auto decoder = std::make_shared<video_decoder_qsv>(options);
			if (!decoder->open())
			{
				return nullptr;
			}
			return decoder;
		}




		video_encoder_ptr create_video_encoder(const video_encoder_options& options)
		{
			video_encoder_ptr encoder = create_video_encoder_cuda(options);
			if (encoder)
			{
				return encoder;
			}

			//encoder = create_video_encoder_qsv(options);
			//if (encoder)
			//{
			//	return encoder;
			//}

			encoder = create_video_encoder_soft(options);
			return encoder;
		}

		video_encoder_ptr create_video_encoder_soft(const video_encoder_options& options)
		{
			video_encoder_ptr encoder;
			if (options.codec_id == AVCodecID::AV_CODEC_ID_H264)
			{
				encoder = std::make_shared<video_encoder_h264_soft>(options);
			}
			else if (options.codec_id == AVCodecID::AV_CODEC_ID_VP8)
			{
				encoder = std::make_shared<video_encoder_h264_soft>(options);
			}
			else
			{
				return nullptr;
			}

			if (!encoder->open())
			{
				return nullptr;
			}

			return encoder;
		}

		video_encoder_ptr create_video_encoder_cuda(const video_encoder_options& options)
		{
			video_encoder_ptr encoder;
			if (options.codec_id == AVCodecID::AV_CODEC_ID_H264)
			{
				encoder = std::make_shared<video_encoder_h264_cuda>(options);
			}
			else if (options.codec_id == AVCodecID::AV_CODEC_ID_VP8)
			{
				encoder = std::make_shared<video_encoder_h264_cuda>(options);
			}
			else
			{
				return nullptr;
			}

			if (!encoder->open())
			{
				return nullptr;
			}

			return encoder;
		}

		video_encoder_ptr create_video_encoder_qsv(const video_encoder_options& options)
		{
			video_encoder_ptr encoder;
			if (options.codec_id == AVCodecID::AV_CODEC_ID_H264)
			{
				encoder = std::make_shared<video_encoder_h264_qsv>(options);
			}
			else if (options.codec_id == AVCodecID::AV_CODEC_ID_VP8)
			{
				encoder = std::make_shared<video_encoder_h264_qsv>(options);
			}
			else
			{
				return nullptr;
			}

			if (!encoder->open())
			{
				return nullptr;
			}

			return encoder;
		}

	}
}



