/**
 * @file media_capabilities.cpp
 * @brief Local media capability set (the codecs and parameters supported by the local end).
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#include "media_capabilities.h"

namespace rtpx {

	media_capabilities& media_capabilities::global()
	{
		static media_capabilities s_instance;
		return s_instance;
	}

	bool media_capabilities::add_format(media_type_t mt, const sdp_format& fmt)
	{
		if (mt != media_type_audio && mt != media_type_video)
		{
			return false;
		}
		std::unique_lock<std::shared_mutex> lk(mutex_);
		formats_[mt].push_back(fmt);
		return true;
	}

	void media_capabilities::clear()
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		formats_.clear();
	}

	bool media_capabilities::empty()const
	{
		std::shared_lock<std::shared_mutex> lk(mutex_);
		return formats_.empty();
	}

	bool media_capabilities::match(media_type_t mt, const sdp_format& fmt)const
	{
		std::shared_lock<std::shared_mutex> lk(mutex_);
		auto itr = formats_.find(mt);
		if (itr == formats_.end())
		{
			return false;
		}
		for (const auto& cap : itr->second)
		{
			if (cap.is_compatible_with(fmt))
			{
				return true;
			}
		}
		return false;
	}

	std::vector<sdp_format> media_capabilities::formats(media_type_t mt)const
	{
		std::shared_lock<std::shared_mutex> lk(mutex_);
		auto itr = formats_.find(mt);
		if (itr == formats_.end())
		{
			return std::vector<sdp_format>();
		}
		return itr->second;
	}

}
