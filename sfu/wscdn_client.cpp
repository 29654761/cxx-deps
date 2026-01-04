#include "wscdn_client.h"
#include <sys2/uri.h>
#include <sys2/util.h>
#include <sys2/security/md5.h>
#include <http/http_client.h>
#include <json/value.h>
#include <json/reader.h>

namespace sfu
{
	wscdn_client::wscdn_client(asio::io_service& ios, spdlogger_ptr log)
		:ios_(ios), log_(log), timer_(ios)
	{
		active_ = false;
	}
	wscdn_client::~wscdn_client()
	{
	}

	std::string wscdn_client::host()const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return host_;
	}
	void wscdn_client::set_host(const std::string& host)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		host_ = host;
	}

	std::string wscdn_client::stream()const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return stream_;
	}

	void wscdn_client::set_stream(const std::string& stream)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		stream_ = stream;
	}

	std::string wscdn_client::key()const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return key_;
	}
	void wscdn_client::set_key(const std::string& key)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		key_ = key;
	}

	bool wscdn_client::open(const std::string& host, const std::string& stream, const std::string& key)
	{
		bool expected = false;
		if (!active_.compare_exchange_strong(expected, true))
			return false;

		set_host(host);
		set_stream(stream);
		set_key(key);
		sess_ = std::make_shared<rtpx::rtp_session>(ios_, log_, true, true);
		sess_->on_connected = std::bind(&wscdn_client::handle_connect, this);
		sess_->on_disconnected = std::bind(&wscdn_client::handle_disconnect, this);
		sess_->on_receive_require_keyframe = std::bind(&wscdn_client::handle_receive_require_keyframe, this, std::placeholders::_1);
		sess_->on_frame = std::bind(&wscdn_client::handle_frame, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);

		connect();

		auto self = std::dynamic_pointer_cast<wscdn_client>(shared_from_this());
		timer_.expires_after(std::chrono::seconds(5));
		timer_.async_wait(std::bind(&wscdn_client::handle_timer,self,std::placeholders::_1));

		return true;
	}

	void wscdn_client::close()
	{
		active_ = false;
		std::error_code ec;
		timer_.cancel(ec);
		disconnect();
		clear_medias();
	}

	bool wscdn_client::send_frame(const std::string& mid, uint8_t pt, const uint8_t* frame, size_t frame_size, int64_t duration)
	{
		if (!sess_)
			return false;

		auto ms = sess_->get_media_stream_by_mid(mid);
		if (!ms)
			return false;

		return ms->send_frame(pt, frame, frame_size, duration);
	}


	bool wscdn_client::connect()
	{
		sess_->stop();
		append_media_formats();

		rtpx::sdp offer;
		if (!sess_->create_offer(offer)) {
			sess_->stop();
			if (log_)
			{
				std::string stm = stream();
				log_->error("WSCND Create offer failed. stream={}", stm)->flush();
			}
			return false;
		}
		std::string url = make_url();


		Json::Value jbody;
		jbody["version"] = "v1.0";
		jbody["sessionId"] = "sidtest";
		Json::Value localSdp;
		localSdp["type"] = "offer";
		localSdp["sdp"] = offer.to_string();
		jbody["localSdp"] = localSdp;
		http_request req;
		req.set_url(url);
		req.set_method("POST");
		req.set_content_type("text/plain;charset=UTF-8");
		req.set_body(jbody.toStyledString());
		http_client http;
		http_response rsp;
		if (!http.request(req, rsp))
		{
			sess_->stop();
			if (log_) {
				std::string stm = stream();
				log_->error("WSCND HTTP request failed. stream={}",  stm)->flush();
			}
			return false;
		}

		Json::Value rsp_body;
		Json::CharReaderBuilder builder;
		std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		reader->parse(rsp.body.c_str(),rsp.body.c_str() + rsp.body.size(), &rsp_body, nullptr);
		std::string answerSdp = rsp_body["remoteSdp"]["sdp"].asString();
		if (answerSdp.empty())
		{
			sess_->stop();
			if (log_) {
				std::string stm = stream();
				log_->error("WSCND HTTP request failed: No answer sdp. stream={}, rsp={}", stm, rsp.body)->flush();
			}
			return false;
		}
		rtpx::sdp answer;
		if (!answer.parse(answerSdp))
		{
			sess_->stop();
			if (log_) {
				std::string stm = stream();
				log_->error("WSCND Parse answerSdp failed. stream={},  rsp={}",stm, rsp.body)->flush();
			}
			return false;
		}
		if (!sess_->set_remote_sdp(answer, sdp_type_answer))
		{
			sess_->stop();
			if (log_) {
				std::string stm = stream();
				log_->error("WSCND Set remote sdp failed. stream={}", stm)->flush();
			}
			return false;
		}
		if (log_) {
			std::string stm = stream();
			log_->debug("WSCND Connect CDN Session ok. stream={}", stm)->flush();
		}

		return true;
	}

	void wscdn_client::disconnect()
	{
		if (sess_)
		{
			sess_->stop();
		}
	}

	void wscdn_client::handle_connect()
	{
		on_connected.invoke(stream());
	}

	void wscdn_client::handle_disconnect()
	{
		on_disconnected.invoke(stream());
	}

	void wscdn_client::handle_receive_require_keyframe(const std::string& mid)
	{
		on_required_keyframe.invoke(stream(), mid);
	}

	void wscdn_client::handle_frame(const std::string& mid, const std::string& sid, const std::string& tid, const rtpx::sdp_format& fmt, const av_frame_t& frame)
	{
		on_frame.invoke(stream(), mid, sid, tid, fmt, frame);
	}

	void wscdn_client::handle_timer(const asio::error_code& ec)
	{
		if(ec||!active_)
			return;

		if (!sess_->is_available())
		{
			if (log_)
			{
				std::string stm = stream();
				log_->error("WSCND session disconnect. stream={}", stm)->flush();
			}
			connect();
		}

		auto self = std::dynamic_pointer_cast<wscdn_client>(shared_from_this());
		timer_.expires_after(std::chrono::seconds(5));
		timer_.async_wait(std::bind(&wscdn_client::handle_timer, self, std::placeholders::_1));
	}

	void wscdn_client::append_media_formats()
	{
		for (auto& mst : medias_)
		{
			auto ms = sess_->create_media_stream(mst.mt, mst.mid, true, mst.sid, mst.tid, mst.trans);
			if (ms)
			{
				if (mst.mt == media_type_audio)
				{
					for (auto& track : mst.audio_tracks)
					{
						ms->add_local_audio_track(track.codec, track.payload_type, track.sample_rate);
						if (track.rtx_playload_type > 0) {
							ms->add_local_rtx_track(track.rtx_playload_type, track.payload_type, track.sample_rate);
						}
					}
				}
				else if (mst.mt == media_type_video)
				{
					for (auto& track : mst.video_tracks)
					{
						ms->add_local_video_track(track.codec, track.payload_type);
						if (track.rtx_playload_type > 0) {
							ms->add_local_rtx_track(track.rtx_playload_type, track.payload_type, 90000);
						}

						rtpx::fmtp fmtp;
						fmtp.set_level_asymmetry_allowed(track.asymmetry_allowed ? 1 : 0);
						fmtp.set_packetization_mode(track.packetization_mode);
						fmtp.set_profile_level_id(track.profile_level_id);
						ms->set_local_rtpmap_fmtp(track.payload_type, fmtp);
					}
				}
			}
		}
	}

	std::string wscdn_client::signature(const std::string& stream,int64_t ts)const
	{
		std::string k = key();
		if (k.empty())
			return "";
		std::string plain = k + "/" + stream + std::to_string(ts);
		MD5 m5 = {};
		md5((const uint8_t*)plain.data(), plain.size(), &m5);
		char secret[64] = {};
		md5_string(&m5, secret, 64);
		return secret;
	}

	std::string wscdn_client::make_url()const
	{
		int64_t ts = sys::util::cur_time_ms() / 1000;
		std::string stm = stream()+".sdp";
		std::string sign = signature(stm,ts);

		sys::uri uri;
		uri.parse(host());

		uri.append_path(stm);
		if (!sign.empty())
		{
			uri.add("wsSecret", sign);
			uri.add("wsTime", std::to_string(ts));
		}
		return uri.to_string();

	}
}