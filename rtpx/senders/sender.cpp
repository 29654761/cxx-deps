/**
 * @file sender.cpp
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */


#include "sender.h"

#include <sys2/util.h>
#include <string.h>

namespace rtpx
{
	sender::sender(uint8_t pt,uint32_t ssrc, media_type_t mt,const sdp_format& fmt, spdlogger_ptr log)
	{
		log_ = log;
		pt_ = pt;
		ssrc_ = ssrc;
		format_ = fmt;
		media_type_ = mt;
		seq_ = sys::util::random_number<uint16_t>(0, 0xFF);

		memset(&stats_, 0, sizeof(stats_));
		stats_.ssrc = ssrc_;
		stats_.pt = pt;
		stats_.ct = format_.codec;
	}

	sender::~sender()
	{
	}

	bool sender::send_packet(packet_ptr pkt)
	{
		sender_(pkt);
		
		stats_.packets_sent_period++;
		stats_.packets_sent++;
		stats_.bytes_sent_period += (uint32_t)pkt->payload_size();
		stats_.bytes_sent += pkt->payload_size();

		seq_ = pkt->header()->seq + 1;

		double now = sys::util::cur_time_db();
		timestamp_now_= ms_to_ts(now * 1000);

		set_history(pkt);
		
		return true;
	}

	uint16_t sender::last_rtp_seq()
	{
		std::shared_lock<std::shared_mutex>lk(mutex_);
		return this->seq_;
	}

	uint32_t sender::last_rtp_timestamp()
	{
		std::shared_lock<std::shared_mutex>lk(mutex_);
		return this->timestamp_;
	}

	double sender::last_timestamp()
	{
		std::shared_lock<std::shared_mutex>lk(mutex_);
		return timestamp_now_;
	}

	uint32_t sender::last_rtp_timestamp_ms()
	{
		std::shared_lock<std::shared_mutex>lk(mutex_);
		return (uint32_t)ts_to_ms(this->timestamp_);
	}
	void sender::set_last_rtp_timestamp_ms(uint32_t ms)
	{
		std::shared_lock<std::shared_mutex>lk(mutex_);
		this->timestamp_ = (uint32_t)ms_to_ts(ms);
	}


	void sender::set_history(packet_ptr packet)
	{
		std::unique_lock<std::shared_mutex>lk(history_packets_mutex_);
		int idx = packet->header()->seq % PACKET_BUFFER_SIZE;
		history_packets_[idx] = packet;
	}

	packet_ptr sender::get_history(uint16_t seq)
	{
		std::shared_lock<std::shared_mutex> lk(history_packets_mutex_);
		int idx = seq % PACKET_BUFFER_SIZE;

		return history_packets_[idx];
	}

	void sender::update_remote_report(const rtcp_report& report)
	{
		std::unique_lock<std::shared_mutex>lk(mutex_);
		
		stats_.lost = report.lost;
		stats_.lost_period = report.fraction;
		stats_.jitter = report.jitter;
	}

	void sender::prepare_sr(rtcp_sr& sr)
	{
		std::shared_lock<std::shared_mutex>lk(mutex_);
		memset(&sr, 0, sizeof(sr));
		rtcp_sr_init(&sr);

		sr.ssrc = ssrc_;
		sr.rtp_ts = now_timestamp();

		double now = sys::util::cur_time_db();
		ntp_tv ntp = ntp_from_unix(now);
		sr.ntp_frac = ntp.frac;
		sr.ntp_sec = ntp.sec;
		
		sr.byte_count = stats_.bytes_sent_period;
		sr.pkt_count = stats_.packets_sent_period;
		stats_.bytes_sent_period = 0;
		stats_.packets_sent_period = 0;
		stats_.lsr_unix = now;
	}



	void sender::get_stats(rtp_sender_stats_t& stats)
	{
		std::shared_lock<std::shared_mutex>lk(mutex_);

		stats_.last_seq = seq_;
		stats_.last_timestamp = timestamp_;
		stats_.pli = pli_count_;
		stats_.fir = fir_count_;
		stats_.nack = nack_count_;
		stats = stats_;
	}

	void sender::increase_fir()
	{
		fir_count_++;
	}
	void sender::increase_pli()
	{
		pli_count_++;
	}
	void sender::increase_nack()
	{
		nack_count_++;
	}
	
	uint32_t sender::now_timestamp()
	{
		double now = sys::util::cur_time_db();
		double now_ts = ms_to_ts(now * 1000);
		uint32_t ts = (uint32_t)(now_ts - timestamp_now_);

		return timestamp_ + ts;
	}
	

	double sender::ms_to_ts(double ms)
	{
		return ms * (format_.frequency / 1000);
	}

	double sender::ts_to_ms(double ts)
	{
		return ts / (format_.frequency / 1000);
	}
}
