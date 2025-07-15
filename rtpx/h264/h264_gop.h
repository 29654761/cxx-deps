/**
 * @file h264_gop.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#pragma once

#include "h264.h"
#include <string>
#include <vector>
#include <mutex>

class h264_gop
{
public:

	struct frame_t
	{
		std::string frame;
		h264::nal_type_t type = h264::nal_type_t::reserved;
		uint32_t duration = 0;
	};

	h264_gop(size_t max_pkts=10000);
	~h264_gop();

	bool add(const uint8_t* frame, uint32_t size,uint32_t duration);
	void clear();
	std::vector<frame_t> all();
private:
	size_t max_pkts_ = 10000;
	bool last_keyframe_ = true;
	std::recursive_mutex mutex_;
	std::vector<frame_t> frames_;

};
