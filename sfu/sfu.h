#pragma once
#include <rtpx/avtypes.h>
#include <rtpx/sdp/sdp_format.h>




struct audio_track_t
{
	codec_type_t codec = codec_type_opus;
	uint8_t payload_type = 100;
	uint8_t rtx_playload_type = 101;
	int sample_rate = 48000;
	int channels = 2;
	int bitrate = 64000;
};

struct video_track_t
{
	codec_type_t codec = codec_type_h264;
	uint8_t payload_type = 96;
	uint8_t rtx_playload_type = 97;
	bool asymmetry_allowed = true;
	int packetization_mode = 1;
	uint32_t profile_level_id = 0x42e01f;
};

struct media_stream_t
{
	std::string mid;
	std::string sid;
	std::string tid;
	media_type_t mt = media_type_unknown;
	rtp_trans_mode_t trans = rtp_trans_mode_inactive;
	std::vector<audio_track_t> audio_tracks;
	std::vector<video_track_t> video_tracks;
};

typedef void (*sfu_connected_t)(void* ctx, const std::string& id);
typedef void (*sfu_disconnected_t)(void* ctx, const std::string& id);
typedef void (*sfu_frame_t)(void* ctx, const std::string& id,const std::string& mid, const std::string& sid, const std::string& tid, const rtpx::sdp_format& fmt, const av_frame_t& frame);
typedef void (*sfu_required_keyframe_t)(void* ctx, const std::string& id, const std::string& mid);