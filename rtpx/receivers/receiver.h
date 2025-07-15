/**
 * @file receiver.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */

#pragma once

#include "../avtypes.h"
#include "../packet.h"
#include "../rtpx_def.h"
#include "../rtp/rtp_source.h"
#include "../rtp/rtcp_sr.h"
#include "../rtp/rtcp_rr.h"
#include "../sdp/sdp_format.h"
#include <asio.hpp>
#include <shared_mutex>
#include <sys2/mutex_function.hpp>
#include <atomic>
#include <array>
#include <thread>
#include <chrono>
#include <spdlog/spdlogger.hpp>


namespace rtpx
{

	typedef struct _nack_pkt_t
	{
		int ssrc = 0;
		int seq = 0;
		int count = 1;
	}nack_pkt_t;


	typedef std::chrono::high_resolution_clock clock;

	class receiver:public std::enable_shared_from_this<receiver>
	{
	public:
		typedef sys::mutex_function<void(uint32_t ssrc, const sdp_format& fmt, const av_frame_t& frame)> frame_handler;
		typedef sys::mutex_function<void(uint32_t ssrc, const sdp_format& fmt, uint16_t pid, uint16_t bld)> nack_handler;
		typedef sys::mutex_function<void(uint32_t ssrc, const sdp_format& fmt)> keyframe_handler;

		receiver(asio::io_context& ioc,int32_t ssrc,media_type_t mt,const sdp_format& fmt, spdlogger_ptr log);
		virtual ~receiver();

		void set_max_nack_count(int num);

		virtual bool start();
		virtual void stop();
		virtual bool insert_packet(packet_ptr pkt) = 0;

		void update_remote_sr(const rtcp_sr& sr);
		void prepare_rr(rtcp_report& rr);
		void get_stats(rtp_receiver_stats_t& stats);
		void increase_fir();
		void increase_pli();
		void increase_nack();
		uint32_t fir_count()const { return fir_count_; }
		uint32_t pli_count()const { return pli_count_; }
		uint32_t nack_count()const { return nack_count_; }

		uint32_t ssrc()const {
			return ssrc_;
		}

		const sdp_format& format()const {
			return format_;
		}

		media_type_t media_type()const { 
			return media_type_; 
		}

		uint16_t last_rtp_seq();
		uint32_t last_rtp_timestamp();

	protected:
		//get the now ntp convert to rtp timestamp.
		uint32_t now_timestamp();
		double ms_to_ts(double ms);
		double ts_to_ms(double ts);

		//Test whether the broken frame can be removed.
		bool is_timeout();

		void add_nack(uint16_t begin, uint16_t end);
		void remove_nack(uint16_t seq);
		void clear_nack();
		std::vector<nack_pkt_t> nack_list();


	protected:
		void handle_timer(const std::error_code& ec);
		size_t process_nack();

		void invoke_rtp_frame(av_frame_t& frame);

	public:
		frame_handler on_frame;
		nack_handler on_nack;
		keyframe_handler on_keyframe;
	protected:
		asio::io_context& ioc_;
		spdlogger_ptr log_;
		asio::steady_timer timer_;
		std::mutex timer_mutex_;

		std::shared_mutex mutex_;
		bool active_ = false;
		std::array<packet_ptr, PACKET_BUFFER_SIZE> recv_packs_;

		std::shared_mutex nack_packs_mutex_;
		std::map<uint16_t,nack_pkt_t> nack_packs_;
		int nack_max_count_ = 1;
		bool reset_ = true;
		bool waiting_for_keyframe_=false;
		double last_keyframe_required_ = 0;
		rtp_source rtp_source_;

		uint32_t first_ts_ = 0;
		double first_sec_ = 0.0;


		int begin_seq_ = -1; //next frame begin seq
		int end_seq_ = -1;

		uint32_t timestamp_ = 0;
		uint32_t frame_timestamp_ = 0;

		uint32_t ssrc_ = 0; //remote ssrc
		media_type_t media_type_ = media_type_t::media_type_audio;
		sdp_format format_;

		int delay_ = 1000;
		clock::time_point frame_begin_ts_ = clock::time_point::min();

		rtp_receiver_stats_t stats_;
		std::atomic<uint32_t> pli_count_ = 0;
		std::atomic<uint32_t> fir_count_ = 0;
		std::atomic<uint32_t> nack_count_ = 0;
	};

	typedef std::shared_ptr<receiver> receiver_ptr;
}