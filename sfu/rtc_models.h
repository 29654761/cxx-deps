#pragma once
#include <librtc.h>
#include <string>
#include <json/value.h>

class rtc_track
{
public:
	rtc_track();
	~rtc_track();

	void from_struct(const rtc_track_info_t* info);

public:
	std::string id;
	std::string uid;
	std::string desc;
	int kind = 0;   // 0=audio, 1=video
	int codec = 0;
	int width = 0;
	int height = 0;
	int fps = 0;
	int angle = 0;
	int bitrate = 0;
	int sample_rate = 0;
	int channel_count = 0;
	Json::Value props;
};



class rtc_user
{
public:
	rtc_user();
	~rtc_user();

	void from_struct(const rtc_user_info_t* info);
	bool find_track(const std::string& desc, rtc_track& track);
	bool get_track(const std::string& id, rtc_track& track);
public:
	std::string uid;
	std::string sid;
	std::string name;
	std::string device_id;
	std::string version;
	std::string channel;
	Json::Value props;
	int device_type = 0;
	bool is_audience = false;
	int64_t join_at = 0;
	int64_t leave_at = 0;
	int64_t updated_at = 0;
	int64_t link_id = 0;
	std::vector<rtc_track> stream_tracks;
};

