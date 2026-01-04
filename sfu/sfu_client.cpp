#include "sfu_client.h"
#include <sys2/uri.h>
#include <http/http_client.h>
#include <sstream>

namespace sfu
{
	sfu_client::sfu_client(asio::io_service& ios, spdlogger_ptr log)
		:ios_(ios), log_(log), timer_(ios)
	{
		active_ = false;
	}
	sfu_client::~sfu_client()
	{
	}

	std::string sfu_client::channel() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return channel_;
	}

	void sfu_client::set_channel(const std::string& channel)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		channel_ = channel;
	}

	std::string sfu_client::url()const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return url_;
	}

	void sfu_client::set_url(const std::string& url)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		url_ = url;
	}

	bool sfu_client::open(const std::string& url, const std::string& channel)
	{
		bool expected = false;
		if (!active_.compare_exchange_strong(expected, true))
			return false;
		set_url(url);
		set_channel(channel);
		sub_rtp_ = std::make_shared<rtpx::rtp_session>(ios_, log_, true, true);
		sub_rtp_->on_connected = std::bind(&sfu_client::handle_connect, this);
		sub_rtp_->on_disconnected = std::bind(&sfu_client::handle_disconnect, this);
		sub_rtp_->on_frame = std::bind(&sfu_client::handle_frame, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);

		pub_rtp_ = std::make_shared<rtpx::rtp_session>(ios_, log_, true, true);
		pub_rtp_->on_connected = std::bind(&sfu_client::handle_connect, this);
		pub_rtp_->on_disconnected = std::bind(&sfu_client::handle_disconnect, this);
		pub_rtp_->on_receive_require_keyframe = std::bind(&sfu_client::handle_receive_require_keyframe, this, std::placeholders::_1);


		connect();

		auto self = std::dynamic_pointer_cast<sfu_client>(shared_from_this());
		timer_.expires_after(std::chrono::seconds(5));
		timer_.async_wait(std::bind(&sfu_client::handle_timer, self, std::placeholders::_1));

		return true;
	}

	void sfu_client::close()
	{
		active_ = false;
		std::error_code ec;
		timer_.cancel(ec);
		disconnect();
		clear_medias();
	}

	bool sfu_client::send_frame(const std::string& mid, uint8_t pt, const uint8_t* frame, size_t frame_size, int64_t duration)
	{
		if (!pub_rtp_)
			return false;

		auto ms = pub_rtp_->get_media_stream_by_mid(mid);
		if (!ms)
			return false;

		return ms->send_frame(pt, frame, frame_size, duration);
	}


	bool sfu_client::add_subscribe_track(const std::string& uid, const std::string& track_id, sfu_frame_t on_frame, void* ctx)
	{
		subscribe_item_t item;
		item.uid = uid;
		item.track_id = track_id;
		item.subscribe = true;
		std::vector<subscribe_item_t> items;
		items.push_back(item);
		rtpx::sdp offer;
		if (!sfu_subscribe(items, offer))
		{
			return false;
		}

		if (!sub_rtp_->set_remote_sdp(offer, sdp_type_offer))
		{
			return false;
		}

		rtpx::sdp sub_answer2;
		if (sub_rtp_->create_answer(sub_answer2))
		{
			sfu_answer_subscribe(sub_answer2);
		}

		{
			std::unique_lock <std::shared_mutex> lock(subscribe_tracks_mutex_);
			auto iter = std::find_if(subscribe_tracks_.begin(), subscribe_tracks_.end(), [uid, track_id, on_frame, ctx](const subscribe_user_track_t& it) {
				return it.uid == uid && it.track_id == track_id && it.on_frame == on_frame && it.ctx == ctx;
			});
			if (iter == subscribe_tracks_.end()) {
				subscribe_user_track_t item;
				item.uid = uid;
				item.track_id = track_id;
				item.on_frame = on_frame;
				item.ctx = ctx;
				subscribe_tracks_.push_back(item);
			}
		}

		return true;
	}

	bool sfu_client::remove_subscribe_track(const std::string& uid, const std::string& track_id, sfu_frame_t on_frame, void* ctx)
	{
		subscribe_item_t item;
		item.uid = uid;
		item.track_id = track_id;
		item.subscribe = false;
		std::vector<subscribe_item_t> items;
		items.push_back(item);
		rtpx::sdp offer;
		if (sfu_subscribe(items, offer))
		{
			if (!sub_rtp_->set_remote_sdp(offer, sdp_type_offer))
			{
				return false;
			}
		}


		{
			std::unique_lock <std::shared_mutex> lock(subscribe_tracks_mutex_);
			auto iter = std::find_if(subscribe_tracks_.begin(), subscribe_tracks_.end(), [uid, track_id, on_frame, ctx](const subscribe_user_track_t& it) {
				return it.uid == uid && it.track_id == track_id && it.on_frame == on_frame && it.ctx == ctx;
			});
			if (iter != subscribe_tracks_.end()) {
				subscribe_tracks_.erase(iter);
			}
		}

		return true;
	}

	void sfu_client::clear_subscribe_tracks()
	{

	}

	bool sfu_client::connect()
	{
		pub_rtp_->stop();

		auto ms= pub_rtp_->create_media_stream(media_type_application, "data", true);
		if (!ms)
			return false;

		ms->add_local_data_channel_track(5000);
		append_media_formats(pub_rtp_);

		rtpx::sdp offer;
		if (!pub_rtp_->create_offer(offer)) {
			disconnect();
			if (log_) {
				std::string chann = channel();
				log_->error("SFU create_offer failed. channel={}", chann)->flush();
			}
			return false;
		}

		rtpx::sdp publish_answer,subscribe_offer;
		if(!sfu_connect(offer, publish_answer, subscribe_offer))
		{
			disconnect();
			if (log_) {
				std::string chann = channel();
				log_->error("SFU sfu_connect failed. channel={}", chann)->flush();
			}
			return false;
		}

		if (!pub_rtp_->set_remote_sdp(publish_answer, sdp_type_answer))
		{
			disconnect();
			if (log_) {
				std::string chann = channel();
				log_->error("SFU set_remote_sdp answer failed. channel={}", chann)->flush();
			}
			return false;
		}

		sub_rtp_->stop();
		if (!sub_rtp_->set_remote_sdp(subscribe_offer, sdp_type_offer))
		{
			disconnect();
			if (log_) {
				std::string chann = channel();
				log_->error("SFU set_remote_sdp offer failed. channel={}", chann)->flush();
			}
			return false;
		}

		rtpx::sdp sub_answer;
		if (!sub_rtp_->create_answer(sub_answer))
		{
			disconnect();
			if (log_) {
				std::string chann = channel();
				log_->error("SFU sub create_answer failed. channel={}", chann)->flush();
			}
			return false;
		}
		
		if (!sfu_answer_subscribe(sub_answer))
		{
			disconnect();
			if (log_) {
				std::string chann = channel();
				log_->error("SFU sub sfu_answer_subscribe failed. channel={}", chann)->flush();
			}
			return false;
		}

		// Resubscribe all tracks
		auto tracks = all_subscribe_tracks();
		if(tracks.size()>0)
		{
			std::vector<subscribe_item_t> items;
			for (auto& track : tracks)
			{
				subscribe_item_t item;
				item.uid = track.uid;
				item.track_id = track.track_id;
				item.subscribe = true;
				items.push_back(item);
			}
			rtpx::sdp offer;
			if (sfu_subscribe(items, offer))
			{
				if (sub_rtp_->set_remote_sdp(offer, sdp_type_offer))
				{
					rtpx::sdp sub_answer2;
					if (sub_rtp_->create_answer(sub_answer2))
					{
						sfu_answer_subscribe(sub_answer2);
					}
				}
			}
		}

		return true;
	}

	void sfu_client::disconnect()
	{
		if(pub_rtp_)
		{
			pub_rtp_->stop();
		}
		if (sub_rtp_)
		{
			sub_rtp_->stop();
		}
	}

	void sfu_client::handle_connect()
	{
		on_connected.invoke(channel());
	}

	void sfu_client::handle_disconnect()
	{
		on_disconnected.invoke(channel());
	}

	void sfu_client::handle_receive_require_keyframe(const std::string& mid)
	{
		on_required_keyframe.invoke(channel(), mid);
	}

	void sfu_client::handle_frame(const std::string& mid, const std::string& sid, const std::string& tid, const rtpx::sdp_format& fmt, const av_frame_t& frame)
	{
		auto tracks=all_subscribe_tracks();
		for(auto& track:tracks)
		{
			if(track.track_id==tid)
			{
				if(track.on_frame)
				{
					track.on_frame(track.ctx, channel(), mid, sid, tid, fmt, frame);
				}
			}
		}
	}

	void sfu_client::handle_timer(const asio::error_code& ec)
	{
		if (ec || !active_)
			return;

		if (!pub_rtp_->is_available()||!sub_rtp_->is_available())
		{
			if (log_)
			{
				std::string chann = channel();
				log_->error("SFU session disconnect. channel={}", chann)->flush();
			}
			connect();
		}

		auto self = std::dynamic_pointer_cast<sfu_client>(shared_from_this());
		timer_.expires_after(std::chrono::seconds(5));
		timer_.async_wait(std::bind(&sfu_client::handle_timer, self, std::placeholders::_1));
	}

	void sfu_client::append_media_formats(rtpx::rtp_session_ptr rtp)
	{
		for (auto& mst : medias_)
		{
			auto ms = rtp->create_media_stream(mst.mt, mst.mid, true, mst.sid, mst.tid, mst.trans);
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

	std::vector<subscribe_user_track_t> sfu_client::all_subscribe_tracks() const
	{
		std::shared_lock lock(subscribe_tracks_mutex_);
		return subscribe_tracks_;
	}

	bool sfu_client::sfu_request(const std::string& path,const Json::Value& jreq,Json::Value& jrsp)
	{
		sys::uri uri;
		uri.parse(this->url());
		uri.append_path(path);

		std::string sbody = jreq.toStyledString();
		std::stringstream ss;
		ss << "Request: " << std::endl;
		ss << sbody << std::endl;
		http_request req;
		req.set_url(uri.to_string());
		req.set_method("POST");
		req.set_content_type("text/plain;charset=UTF-8");
		req.set_body(sbody);
		http_client http;
		http_response rsp;
		bool r = http.request(req, rsp);
		ss << "Response: " << std::endl;
		ss << rsp.body << std::endl;
		if (log_)
		{
			log_->debug(ss.str())->flush();
		}

		if (!r)
		{
			return false;
		}

		Json::CharReaderBuilder builder;
		std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		reader->parse(rsp.body.c_str(), rsp.body.c_str() + rsp.body.size(), &jrsp, nullptr);

		Json::Value jcode = jrsp["code"];
		if (!jcode.isInt())
		{
			if (log_) {
				std::string chann = channel();
				log_->error("SFU HTTP request failed: No code filed in response, channel={}", chann)->flush();
			}
			return false;
		}
		int code = jcode.asInt();
		if (code != 0)
		{
			if (log_) {
				std::string chann = channel();
				log_->error("SFU HTTP request failed,channel={},code={} ", chann, code)->flush();
			}
			return false;
		}

		return true;
	}

	bool sfu_client::sfu_connect(const rtpx::sdp& offer,rtpx::sdp& publish_answer,rtpx::sdp& subscribe_offer)
	{
		Json::Value jreq(Json::objectValue);
		Json::Value publish_offer;
		publish_offer["type"] = "offer";
		publish_offer["sdp"] = offer.to_string();
		jreq["publish_offer"] = publish_offer;

		Json::Value jrsp;
		if (!sfu_request("v1/sfu/connect", jreq, jrsp))
		{
			return false;
		}

		Json::Value jdata = jrsp["data"];
		if (jdata.isNull())
		{
			return false;
		}

		std::string publish_answer_sdp = jdata["publish_answer"]["sdp"].asString();
		std::string subscribe_offer_sdp = jdata["subscribe_offer"]["sdp"].asString();

		if(!publish_answer.parse(publish_answer_sdp)|| !subscribe_offer.parse(subscribe_offer_sdp))
		{
			return false;
		}

		return true;
	}

	bool sfu_client::sfu_publish(const rtpx::sdp& offer, rtpx::sdp& answer)
	{
		Json::Value jreq(Json::objectValue);
		Json::Value publish_offer;
		publish_offer["type"] = "offer";
		publish_offer["sdp"] = offer.to_string();
		jreq["offer"] = publish_offer;

		Json::Value jrsp;
		if (!sfu_request("v1/sfu/offer-publish", jreq, jrsp))
		{
			return false;
		}

		Json::Value jdata = jrsp["data"];
		if (jdata.isNull())
		{
			return false;
		}

		std::string anwser_sdp = jdata["answer"]["sdp"].asString();
		if (!answer.parse(anwser_sdp))
		{
			return false;
		}

		return true;
	}

	bool sfu_client::sfu_subscribe(const std::vector<subscribe_item_t>& items, rtpx::sdp& offer)
	{
		Json::Value jreq(Json::objectValue);
		Json::Value tracks;

		for (auto& item:items)
		{
			Json::Value track;
			track["pub_uid"] = item.uid;
			track["track_id"] = item.track_id;
			track["subscribe"] = item.subscribe;
			tracks.append(track);
		}

		jreq["tracks"] = tracks;

		Json::Value jrsp;
		if (!sfu_request("v1/sfu/subscribe", jreq, jrsp))
		{
			return false;
		}

		Json::Value jdata = jrsp["data"];
		if (jdata.isNull())
		{
			return false;
		}

		std::string offer_sdp = jdata["offer"]["sdp"].asString();
		if (!offer.parse(offer_sdp))
		{
			return false;
		}

		return true;
	}

	bool sfu_client::sfu_answer_subscribe(const rtpx::sdp& subscribe_answer)
	{
		Json::Value jreq(Json::objectValue);
		Json::Value answer;
		answer["type"] = "answer";
		answer["sdp"] = subscribe_answer.to_string();

		jreq["answer"] = answer;

		Json::Value jrsp;
		return sfu_request("v1/sfu/answer-subscribe", jreq,jrsp);
	}
}