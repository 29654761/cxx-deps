#pragma once

#ifdef _MSC_VER
#define _CRT_USE_C_COMPLEX_H
#endif
#include <librtc.h>
#include <mutex>
#include <memory>
#include <functional>
#include <string>


class rtc_publisher :public std::enable_shared_from_this<rtc_publisher>
{
public:
	// 关键帧请求处理函数：SFU 经 RTCP(PLI/FIR) 要求本轨道出关键帧时，由 rtc-go 在独立线程调用。
	// track_id 是 rtc-go 注册回调时捕获的轨道 id；MCU 合成流发布时 rtc-go 会把 id 改写成 __mcu__/__amcu__，
	// 而回调在发布前注册，所以这里拿到的仍是创建时的随机 id，只宜用于日志，不要拿它去匹配轨道
	typedef std::function<void(const std::string& track_id)> keyframe_request_handler;

	rtc_publisher();
	~rtc_publisher();

	const rtc_publish_options_t& options() const {
		return options_;
	}

	void* handle() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return handle_;
	}

	bool open(int codec);
	void close();

	// 设置关键帧请求处理函数，须在 publish / publish_mcu 之前调用：
	// rtc-go 要求对每条本地轨道在发布前注册（rtc_set_keyframe_request_callback 按 track handle 查找），
	// 本类在调用 rtc-go 发布函数之前统一注册。未设置则不注册，行为与之前一致
	void set_keyframe_request_callback(keyframe_request_handler handler);
	
	bool publish(void* channel_handler, const rtc_publish_options_t& options);

	//发布mcu流，kind=0 音频；kind=1视频；
	//options.desc 为空时按 kind 填 "mcu_audio" / "mcu_video"，非空时沿用调用方传入的值
	bool publish_mcu(void* channel_handler, const rtc_publish_options_t& options, int kind);
	void unpublish();
	bool write_sample(uint8_t* data, int length, uint32_t samples);
	// 按内容时刻写一帧（rtc_write_sample_ts，librtc 新接口）：capture_time_us 为该帧的内容时刻
	// （采集/合成时刻，std::chrono::steady_clock 微秒），RTP 时间戳与 RTCP SR 由 rtc-go 按它生成，
	// 订阅端按 SR 还原出内容时刻，编码/发送抖动不影响音画同步。与 write_sample 二选一，同一发布器不能混用
	bool write_sample_ts(uint8_t* data, int length, int64_t capture_time_us);
private:
	// 在 rtc-go 发布前对本轨道注册关键帧回调（调用方须已持有 mutex_）
	void register_keyframe_callback();
	static void s_rtc_keyframe_request_callback(void* context, const char* track_id);

private:
	rtc_publish_options_t options_;
	mutable std::mutex mutex_;
	void* handle_ = nullptr;
	void* channel_handler_ = nullptr;

	// 关键帧请求处理函数，由 rtc-go 回调线程读取，单独加锁，避免与 write_sample 等持有 mutex_ 的调用互等
	std::mutex keyframe_mutex_;
	keyframe_request_handler keyframe_handler_;
};

typedef std::shared_ptr<rtc_publisher> rtc_publisher_ptr;

