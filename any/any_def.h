#pragma once
#ifndef __ANY_DEF_H__
#define __ANY_DEF_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum any_media_type_t
	{
		any_media_type_data = 0,
		any_media_type_video = 1,
		any_media_type_audio = 2,
	}any_media_type_t;



	typedef enum any_frame_type_t
	{
		any_frame_type_unknown = -1,
		any_frame_type_idr = 0,
		any_frame_type_i_frame = 1,
		any_frame_type_frame = 2,
	}any_frame_type_t;

	typedef enum any_color_format_t
	{
		any_color_format_unknown = 0,
		any_color_format_rgb = 1,             ///< rgb color formats
		any_color_format_rgba = 2,
		any_color_format_yuv444p = 3,
		any_color_format_yuv422p = 4,
		any_color_format_yuv420p = 5,
		any_color_format_mjpeg = 100,
	}any_color_format_t;

	typedef enum any_codec_type_t
	{
		any_codec_type_unknown = 0,
		any_codec_type_h264 = 1,
		any_codec_type_h265 = 2,
		any_codec_type_aac = 3,
		any_codec_type_opus = 4,
	}any_codec_type_t;

	typedef struct any_frame_t
	{
		const uint8_t* data;
		int data_size;
		long long pts;
		long long dts;
		int duration;
		int track;
		any_media_type_t mt;
		any_codec_type_t ct;
		any_frame_type_t ft;
		int volume;    //PCM average valume(0 ~ 32787),or -1 for ignore
		unsigned int tmscale;	// time scale for pts and dts
		unsigned int frmsequ;	// frame's sequence
		int angle;
	}any_frame_t;

	typedef struct any_picture_t
	{
		any_color_format_t fmt;
		int width;
		int height;
		int track;
		unsigned char* buffer[4];
		int stride[4];
	}any_picture_t;

	typedef struct any_pcm_t
	{
		int channels;
		int samplerate;
		int trackId;
		unsigned int stamp;
		unsigned int flag;
		const uint8_t* data;
		int data_size;
		int bits;
	}any_pcm_t;

	typedef void(*any_user_join_event_t)(void* ctx, int room_id, int peer_id);
	typedef void(*any_user_leave_event_t)(void* ctx, int room_id, int peer_id);
	typedef void(*any_connected_event_t)(void* ctx);
	typedef void(*any_disconnected_event_t)(void* ctx);
	typedef void(*any_frame_event_t)(void* ctx, int room_id, int peer_id, const any_frame_t* frame);
	typedef void(*any_pcm_event_t)(void* ctx, int room_id, int peer_id, const any_pcm_t* pcm);

#ifdef __cplusplus
}
#endif

#endif //__ANY_DEF_H__