#pragma once

#include <memory>
#include <mutex>
#include <atomic>
#include <any/any_client.h>
#include <shared_mutex>

class any_room;
typedef std::shared_ptr<any_room> any_room_ptr;

class any_user
{
public:
	struct track_t
	{
		int track = 0;
		int angle_ = 0;
		int64_t last_ts_ = 0;
		any_frame_event_t frame_event = nullptr;
		void* ctx = nullptr;
	};

	any_user(any_room_ptr room,int peer_id);
	~any_user();
	int peer_id()const { return peer_id_; }
	bool is_timeout()const;

	void input_frame(any_frame_t* frame);

	void add_track(int track, any_frame_event_t frame_event,void* ctx);
	size_t remove_track(int track, any_frame_event_t frame_event, void* ctx);
	void clear_tracks();
	size_t count_tracks();
	void auto_pick();

	void release();

private:
	void invoke_frame(any_frame_t* frame);
	void _pick();
private:
	mutable std::shared_mutex mutex_;
	any_room_ptr room_;
	std::atomic<time_t> updated_at_;
	int peer_id_ = 0;
	std::vector<track_t> tracks_;
};

typedef std::shared_ptr<any_user> any_user_ptr;
