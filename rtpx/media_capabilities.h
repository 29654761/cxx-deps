/**
 * @file media_capabilities.h
 * @brief Local media capability set (the codecs and parameters supported by the local end).
 * @author Shijie Zhou
 * @copyright 2025 Shijie Zhou
 */

#pragma once

#include <map>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "sdp/sdp_format.h"

namespace rtpx {

	// 本地支持的媒体格式集合（能力集）。
	// RFC 3264 §6.1：offer 从本端能力生成；answer 必须是 offer 与本端能力的交集。
	// - offer：媒体流创建时从能力集取格式（含 fmtp/rtcp-fb）；
	// - answer：media_stream::set_remote_sdp 用能力集过滤对端 rtpmap，仅保留交集，pt 沿用对端编号。
	// 未注册任何能力时，set_remote_sdp 保持旧行为（镜像对端 rtpmap），不影响既有调用方（如 sfu）。
	class media_capabilities
	{
	public:
		// 进程级全局能力集（编解码能力通常由库决定，全局注册一次即可）
		static media_capabilities& global();

		// 注册支持的格式。pt 为建议 payload type（生成 offer 时使用；应答时以对端 pt 为准）
		bool add_format(media_type_t mt, const sdp_format& fmt);
		void clear();

		bool empty()const;

		// 判断对端格式是否在本端能力范围内（codec、采样率、packetization-mode、profile、level 兼容）
		bool match(media_type_t mt, const sdp_format& fmt)const;

		// 取某媒体类型的能力格式拷贝（生成 offer 用）
		std::vector<sdp_format> formats(media_type_t mt)const;

	private:
		mutable std::shared_mutex mutex_;
		std::map<int, std::vector<sdp_format>> formats_;
	};

}
