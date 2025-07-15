
/**
 * @file h265_gop.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#include "h265_gop.h"

h265_gop::h265_gop(size_t max_pkts)
{
	max_pkts_ = max_pkts;
	frames_.reserve(max_pkts_);

}

h265_gop::~h265_gop()
{
	clear();
}

bool h265_gop::add(const uint8_t* frame, uint32_t size, uint32_t duration)
{
	size_t offset = 0, nal_start = 0, nal_size = 0;
	bool islast = false;
	if (!h265::find_next_nal(frame, size, offset, nal_start, nal_size, islast))
	{
		return false;
	}
	h265::nal_type_t t=h265::get_nal_type(frame+nal_start, nal_size);
	bool iskey = h265::is_key_frame(t);
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

void h265_gop::clear()
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);
	frames_.clear();
	last_keyframe_ = true;
}

std::vector<h265_gop::frame_t> h265_gop::all()
{
	std::lock_guard<std::recursive_mutex> lk(mutex_);
	std::vector<h265_gop::frame_t> vec = frames_;
	last_keyframe_ = true;
	return vec;
}

