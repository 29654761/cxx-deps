#pragma once
#include <sys2/callback.hpp>
#include <mutex>
#include <array>
#include <memory>
#include <rtp/rtp.h>
#define PACKET_BUFFER_SIZE 512

class rtp_h264_recevier
{
public:
	typedef std::chrono::high_resolution_clock clock;
	typedef void (*frame_event_t)(void* ctx,const uint8_t* frame, size_t size,int64_t ts);

	rtp_h264_recevier();
	~rtp_h264_recevier();

	void set_max_rtp_packet_size(int max_rtp_packet_size) { max_rtp_packet_size_ = max_rtp_packet_size; }
	bool is_waiting_for_keyframe()const { return waiting_for_keyframe_; }
	virtual bool insert_packet(const RTP_DataFrame& pkt);
private:
	bool find_a_frame(std::vector<std::shared_ptr<RTP_DataFrame>>& pkts);

	//check and drop the broken frame;
	void check_for_drop();



	bool combin_frame(const std::vector< std::shared_ptr<RTP_DataFrame>>& pkts);
	bool is_timeout();

public:
	sys::callback<frame_event_t> frame_event;
private:
	std::recursive_mutex mutex_;
	int max_rtp_packet_size_ = 1200;
	bool reset_ = true;
	bool waiting_for_keyframe_ = false;
	int begin_seq_ = -1; //next frame begin seq
	int end_seq_ = -1;
	int delay_ = 1000;
	uint32_t first_ts_ = 0;
	double first_sec_ = 0.0;
	uint32_t timestamp_ = 0;
	uint32_t frame_timestamp_ = 0;
	clock::time_point frame_begin_ts_ = clock::time_point::min();
	std::array<std::shared_ptr<RTP_DataFrame>, PACKET_BUFFER_SIZE> recv_packs_;
};

