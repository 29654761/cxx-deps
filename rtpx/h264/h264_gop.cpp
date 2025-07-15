
/**
 * @file h264_gop.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#include "h264_gop.h"

h264_gop::h264_gop(size_t max_pkts)
{
	max_pkts_ = max_pkts;
	frames_.reserve(max_pkts_);

}

h264_gop::~h264_gop()
{
	clear();
}

bool h264_gop::add(const uint8_t* frame, uint32_t size, uint32_t duration)
{
	size_t offset = 0, nal_start = 0, nal_size = 0;
	bool islast = false;
	if (!h264::find_next_nal(frame, size, offset, nal_start, nal_size, islast))
	{
		return false;
	}
	h264::nal_type_t t=h264::get_nal_type(frame[nal_start]);
	bool iskey = h264::is_key_frame(t);
	std::lock_guard<std::recursive_mutex> lk(mutex_);



	if ((iskey&&!last_keyframe_)||frames_.size() > max_pkts_)
	{
		frames_.clear();
	}

	last_keyframe_ = iskey;
	if (frames_.size() == 0)
	{
		if (!iskey)
		{
			return false;
		}
	}

	frame_t st = {};
	st.frame.assign((const char*)frame, size);
	st.type = t;
	st.duration = duration;
	frames_.push_back(st);
	return true;
}

void h264_gop::clear()
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);
	frames_.clear();
	last_keyframe_ = true;
}

std::vector<h264_gop::frame_t> h264_gop::all()
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);
	std::vector<h264_gop::frame_t> vec = frames_;
	last_keyframe_ = true;
	return vec;
}

