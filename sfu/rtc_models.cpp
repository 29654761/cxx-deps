#include "rtc_models.h"
#include <json/reader.h>

rtc_track::rtc_track()
{
}

rtc_track::~rtc_track()
{
}

void rtc_track::from_struct(const rtc_track_info_t* info)
{
	id = info->track_id;
	uid = info->uid;
	desc = info->desc;
	kind = info->kind;
	codec = info->codec;
	width = info->width;
	height = info->height;
	fps = info->fps;
	angle = info->angle;
	bitrate = info->bitrate;
	sample_rate = info->sample_rate;
	channel_count = info->channel_count;

	if (info->props)
	{
		Json::CharReaderBuilder b;
		std::unique_ptr<Json::CharReader> r(b.newCharReader());
		r->parse(info->props, info->props + strlen(info->props), &props, nullptr);
	}
}



rtc_user::rtc_user()
{
}

rtc_user::~rtc_user()
{
}

void rtc_user::from_struct(const rtc_user_info_t* info)
{
	uid = info->uid;
	sid = info->sid;
	name = info->name;
	device_id = info->device_id;
	version = info->version;
	channel = info->channel;
	device_type = info->device_type;
	is_audience = info->is_audience;
	join_at = info->join_at;
	leave_at = info->leave_at;
	updated_at = info->updated_at;
	link_id = info->link_id;

	stream_tracks.clear();
	if (info->stream_tracks && info->stream_track_count > 0)
	{
		stream_tracks.reserve(info->stream_track_count);

		for (int i = 0; i < info->stream_track_count; i++)
		{
			rtc_track t;
			t.from_struct(info->stream_tracks+i);
			stream_tracks.push_back(t);
		}
	}

	if (info->props)
	{
		Json::CharReaderBuilder b;
		std::unique_ptr<Json::CharReader> r(b.newCharReader());
		r->parse(info->props, info->props + strlen(info->props), &props, nullptr);
	}
}

bool rtc_user::find_track(const std::string& desc, rtc_track& track)
{
	for (auto& t : stream_tracks)
	{
		if (t.desc == desc)
		{
			track = t;
			return true;
		}
	}
	return false;
}

bool rtc_user::get_track(const std::string& id, rtc_track& track)
{
	for (auto& t : stream_tracks)
	{
		if (t.id == id)
		{
			track = t;
			return true;
		}
	}
	return false;
}
