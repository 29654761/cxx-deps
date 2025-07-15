#pragma once

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace ffmpeg
{
	namespace utils
	{
		class def_convertor
		{
		public:
			static color_format_t convert_pixel_formt(AVPixelFormat fmt)
			{
				if (fmt == AVPixelFormat::AV_PIX_FMT_RGB24)
					return color_format_rgb24;
				else if (fmt == AVPixelFormat::AV_PIX_FMT_BGR24)
					return color_format_bgr24;
				else if (fmt == AVPixelFormat::AV_PIX_FMT_ARGB)
					return color_format_argb;
				else if (fmt == AVPixelFormat::AV_PIX_FMT_RGBA)
					return color_format_rgba;
				else if (fmt == AVPixelFormat::AV_PIX_FMT_ABGR)
					return color_format_abgr;
				else if (fmt == AVPixelFormat::AV_PIX_FMT_BGRA)
					return color_format_bgra;
				else if (fmt == AVPixelFormat::AV_PIX_FMT_YUV444P)
					return color_format_yuv444;
				else if (fmt == AVPixelFormat::AV_PIX_FMT_YUV422P)
					return color_format_yuv422;
				else if (fmt == AVPixelFormat::AV_PIX_FMT_YUV420P)
					return color_format_yuv420;
				else if (fmt == AVPixelFormat::AV_PIX_FMT_NV12)
					return color_format_nv12;
				else if (fmt == AVPixelFormat::AV_PIX_FMT_NV21)
					return color_format_nv21;
				else if (fmt == AVPixelFormat::AV_PIX_FMT_NV24)
					return color_format_nv24;
				else if (fmt == AVPixelFormat::AV_PIX_FMT_NV42)
					return color_format_nv42;
				else
					return color_format_unknown;
			}

			static AVPixelFormat convert_color_formt(color_format_t fmt)
			{
				if (fmt == color_format_rgb24)
					return AVPixelFormat::AV_PIX_FMT_RGB24;
				else if (fmt == color_format_bgr24)
					return AVPixelFormat::AV_PIX_FMT_BGR24;
				else if (fmt == color_format_argb)
					return AVPixelFormat::AV_PIX_FMT_ARGB;
				else if (fmt == color_format_rgba)
					return AVPixelFormat::AV_PIX_FMT_RGBA;
				else if (fmt == color_format_abgr)
					return AVPixelFormat::AV_PIX_FMT_ABGR;
				else if (fmt == color_format_bgra)
					return AVPixelFormat::AV_PIX_FMT_BGRA;
				else if (fmt == color_format_yuv444)
					return AVPixelFormat::AV_PIX_FMT_YUV444P;
				else if (fmt == color_format_yuv422)
					return AVPixelFormat::AV_PIX_FMT_YUV422P;
				else if (fmt == color_format_yuv420)
					return AVPixelFormat::AV_PIX_FMT_YUV420P;
				else if (fmt == color_format_nv12)
					return AVPixelFormat::AV_PIX_FMT_NV12;
				else if (fmt == color_format_nv21)
					return AVPixelFormat::AV_PIX_FMT_NV21;
				else if (fmt == color_format_nv24)
					return AVPixelFormat::AV_PIX_FMT_NV24;
				else if (fmt == color_format_nv42)
					return AVPixelFormat::AV_PIX_FMT_NV42;
				else
					return AVPixelFormat::AV_PIX_FMT_NONE;
			}

			static codec_type_t convert_avcodec_id(AVCodecID codec_id)
			{
				switch (codec_id)
				{
				case AV_CODEC_ID_PCM_ALAW:
					return codec_type_pcma;
				case AV_CODEC_ID_PCM_MULAW:
					return codec_type_pcmu;
				case AV_CODEC_ID_OPUS:
					return codec_type_opus;
				case AV_CODEC_ID_AAC:
					return codec_type_mpeg4_generic;
				case AV_CODEC_ID_AAC_LATM:
					return codec_type_mp4a_latm;
				case AV_CODEC_ID_ADPCM_G722:
					return codec_type_g722;
				case AV_CODEC_ID_H264:
					return codec_type_h264;
				case AV_CODEC_ID_HEVC:
					return codec_type_h265;
				case AV_CODEC_ID_VP8:
					return codec_type_vp8;
				case AV_CODEC_ID_VP9:
					return codec_type_vp9;
				default:
					return codec_type_unknown;
				}
			}

			static AVCodecID convert_codec_type(codec_type_t type)
			{
				switch (type)
				{
				case codec_type_pcma:
					return AV_CODEC_ID_PCM_ALAW;
				case codec_type_pcmu:
					return AV_CODEC_ID_PCM_MULAW;
				case codec_type_opus:
					return AV_CODEC_ID_OPUS;
				case codec_type_mpeg4_generic:
					return AV_CODEC_ID_AAC;
				case codec_type_mp4a_latm:
					return AV_CODEC_ID_AAC_LATM;
				case codec_type_g722:
					return AV_CODEC_ID_ADPCM_G722;
				case codec_type_h264:
					return AV_CODEC_ID_H264;
				case codec_type_h265:
					return AV_CODEC_ID_HEVC;
				case codec_type_vp8:
					return AV_CODEC_ID_VP8;
				case codec_type_vp9:
					return AV_CODEC_ID_VP9;
				default:
					return AV_CODEC_ID_NONE;
				}
			}

			static AVSampleFormat convert_sample_format(sample_format_t fmt)
			{
				switch (fmt)
				{
				case sample_format_s16:
					return AVSampleFormat::AV_SAMPLE_FMT_S16;
				case sample_format_fltp:
					return AVSampleFormat::AV_SAMPLE_FMT_FLTP;
				default:
					return AVSampleFormat::AV_SAMPLE_FMT_NONE;
				}
			}

			static sample_format_t convert_sample_format(AVSampleFormat fmt)
			{
				switch (fmt)
				{
				case AVSampleFormat::AV_SAMPLE_FMT_S16:
					return sample_format_s16;
				case AVSampleFormat::AV_SAMPLE_FMT_FLTP:
					return sample_format_fltp;
				default:
					return sample_format_unknown;
				}
			}

			static uint8_t get_sample_format_bytes(sample_format_t fmt)
			{
				switch (fmt)
				{
				case sample_format_unknown:
					return 0;
				case sample_format_s16:
					return 2;
				case sample_format_fltp:
					return 4;
				default:
					return 0;
				}
			}
		};

		

	}
}