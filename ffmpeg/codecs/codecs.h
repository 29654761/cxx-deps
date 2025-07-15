#pragma once
#include "video_decoder.h"
#include "video_encoder.h"
#include "audio_decoder.h"
#include "audio_encoder.h"

namespace ffmpeg
{
	namespace codecs
	{
		audio_decoder_ptr create_audio_decoder(const audio_decoder_options& options);
		audio_encoder_ptr create_audio_encoder(const audio_encoder_options& options);

		video_decoder_ptr create_video_decoder(const video_decoder_options& options);
		video_decoder_ptr create_video_decoder_soft(const video_decoder_options& options);
		video_decoder_ptr create_video_decoder_cuda(const video_decoder_options& options);
		video_decoder_ptr create_video_decoder_qsv(const video_decoder_options& options);

		video_encoder_ptr create_video_encoder(const video_encoder_options& options);
		video_encoder_ptr create_video_encoder_soft(const video_encoder_options& options);
		video_encoder_ptr create_video_encoder_cuda(const video_encoder_options& options);
		video_encoder_ptr create_video_encoder_qsv(const video_encoder_options& options);
	}
}

