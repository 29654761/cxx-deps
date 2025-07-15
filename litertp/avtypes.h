/**
 * @file avtype.h
 * @brief Media struct defined.
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#ifndef __AV_TYPES_H__
#define __AV_TYPES_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif




typedef enum media_type_t
{
	media_type_unknown = 0,
	media_type_audio = 1,
	media_type_video = 2,
	media_type_application=3,
}media_type_t;

typedef enum codec_type_t
{
	codec_type_unknown = 0,
	codec_type_pcma=1,
	codec_type_pcmu = 2,
	codec_type_g722 = 3,
	codec_type_opus = 4,
	codec_type_mpeg4_generic = 5,   // aac rfc-3640
	codec_type_mp4a_latm = 6,		// aac rfc-3016
	codec_type_telephone_event=7,
	codec_type_cn=8,
	codec_type_mp2p=9,
	codec_type_mp2s=10,

	codec_type_h264 = 101,
	codec_type_h265 = 102,
	codec_type_vp8 = 103,
	codec_type_vp9 = 104,
	codec_type_av1=105,
	codec_type_ps=106,

	codec_type_rtx = 200,
	codec_type_red = 201,
	codec_type_ulpfec = 202,
}codec_type_t;

typedef struct _codec_def_t
{
	codec_type_t type;
	const char* def;
}codec_def_t;

const codec_def_t codec_def[] = {
	codec_type_telephone_event,	"telephone-event",
	codec_type_mpeg4_generic,	"mpeg4-generic",
	codec_type_mp4a_latm,		"MP4A-LATM",
	codec_type_h264,			"H264",
	codec_type_h265,			"H265",
	codec_type_opus,			"OPUS",
	codec_type_pcma,			"PCMA",
	codec_type_g722,			"G722",
	codec_type_cn,				"CN",
	codec_type_vp8,				"VP8",
	codec_type_vp9,				"VP9",
	codec_type_av1,				"AV1",
	codec_type_ps,				"PS",
	codec_type_rtx,				"rtx",
	codec_type_red,				"red",
	codec_type_ulpfec,			"ulpfec",
	codec_type_mp2p,			"MP2P",
	codec_type_mp2s,			"MP2S",
};


typedef enum color_format_t
{
	color_format_unknown,
	color_format_rgb24,
	color_format_bgr24,
	color_format_argb,
	color_format_rgba,
	color_format_abgr,
	color_format_bgra,
	color_format_yuv444,
	color_format_yuv422,
	color_format_yuv420,
	color_format_nv12,
	color_format_nv16,
	color_format_nv21,
	color_format_nv24,
	color_format_nv42,
	color_format_mjpeg,
}color_format_t;

typedef enum sample_format_t
{
	sample_format_unknown,
	sample_format_s16,
	sample_format_fltp,
}sample_format_t;

typedef struct av_rect_t
{
	int x;
	int y;
	int cx;
	int cy;
}av_rect_t;

typedef struct av_frame_t
{
	int64_t pts;
	int64_t dts;
	uint8_t* data;
	uint32_t data_size;
	media_type_t mt;
	codec_type_t ct;
	uint32_t duration;
	int fec;
}av_frame_t;


typedef enum transform_order_t {
	rtp_transform_rotate_scale = 1,		// rotate before scale
	rtp_transform_scale_rotate = 2,		// scale before rotate
}transform_order_t;

typedef enum transform_scale_t {
	rtp_transform_scale_none = 0,			// no scaling
	rtp_transform_scale_x = 1,			// reverse on horizontal
	rtp_transform_scale_y = 2,			// reverse on vertical
}transform_scale_t;

typedef enum transform_angle_t {
	rtp_transform_rotate_0 = 0,			// no rotating
	rtp_transform_rotate_90 = 1,		// rotate clockwise 90
	rtp_transform_rotate_180 = 2,		// rotate clockwise 180
	rtp_transform_rotate_270 = 3,		// rotate clockwise 270
}transform_angle_t;

typedef union transform_t
{
	uint32_t v;
	struct {
		transform_order_t order : 2;
		transform_scale_t scale : 3;
		transform_angle_t angle : 3;
	} s;
}transform_t;


typedef struct av_picture_t
{
	color_format_t fmt;
	int width;
	int height;
	uint8_t* buffer[4];
	size_t stride[4];
	uint32_t transform;
	int64_t pts;
	int64_t duration;
}av_picture_t;

typedef struct av_pcm_t
{
	sample_format_t fmt;
	int sample_rate;
	int channel;
	int samples;
	uint8_t* buffer[4];
	int64_t pts;
}av_pcm_t;


typedef void (*av_picture_event)(void* ctx,const av_picture_t& picture);
typedef void (*av_pcm_event)(void* ctx, const av_pcm_t& pcm);
typedef void (*av_frame_event)(void* ctx, const av_frame_t& frame);

#ifdef __cplusplus
}
#endif

#endif // !__AV_TYPES_H__