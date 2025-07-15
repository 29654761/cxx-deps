#pragma once

#include "any_user.h"
#include <sys2/mutex_callback.hpp>
#include <vector>
#include <map>

struct any_room_options_t:public any_options_t
{
	bool sending_video = false;
	bool sending_audio = false;
	bool recving_video = false;
	bool recving_audio = false;
	bool no_pick_audio = true;
};

class any_room:public any_client_event,public std::enable_shared_from_this<any_room>
{
	friend any_user;
public:
	any_room();
	~any_room();

	bool open(const any_room_options_t& opts);
	void close();

	bool is_timeout()const;

	int room_id()const;
	std::string key()const;
	size_t remove_user(int peer_id);
	any_user_ptr get_user(int peer_id);
	size_t user_count();
	void clear_users();
	void all_users(std::vector<any_user_ptr>& users);

	any_user_ptr add_track(int user_id, int track_id, any_frame_event_t onframe, void* ctx);
	size_t remove_track(int user_id, int track_id, any_frame_event_t onframe, void* ctx);
	size_t remove_user_tracks(int user_id);
	void remove_tracks();
	
	bool enable_send_video(bool enabled);
	bool enable_send_audio(bool enabled);
	bool enable_recv_video(bool enabled);
	bool enable_recv_audio(bool enabled);
	bool set_no_pick_audio(bool enabled);

	bool set_mcu_track_mask(int mask);

	bool send_frame(const any_frame_t* frame);

	static std::string make_key(int room_id, int peer_id);
public:
	sys::mutex_callback<any_user_join_event_t> user_join_event;
	sys::mutex_callback<any_user_leave_event_t> user_leave_event;
	sys::mutex_callback<any_connected_event_t> connected_event;
	sys::mutex_callback<any_disconnected_event_t> disconnected_event;
	sys::mutex_callback<any_frame_event_t> frame_event;
	sys::mutex_callback<any_pcm_event_t> pcm_event;
private:
	virtual void on_connected();
	virtual void on_disconnected();
	virtual void on_frame(int roomId, int peerId, const any_frame_t& frame);
	virtual void on_pcm(int roomId, int peerId, const any_pcm_t& pcm);
	virtual void on_member_in(int roomId, int peerId);
	virtual void on_member_out(int roomId, int peerId);
	virtual void on_down_level(int roomId, int peerId, int cc, int sc);
	virtual void on_up_level(int roomId, int peerId, int level);

private:
	mutable std::shared_mutex mutex_;

	std::atomic<time_t> updated_at_;
	std::shared_ptr<any_client> any_;
	int room_id_ = 0;
	int local_id_ = 0;

	std::map<int, any_user_ptr> users_;

	bool sending_video_ = false;
	bool sending_audio_ = false;
	bool recving_video_ = false;
	bool recving_audio_ = false;
	bool no_pick_audio_ = true;
};

