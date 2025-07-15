#include "any_user.h"
#include "any_room.h"
#include <cmath>

any_user::any_user(any_room_ptr room,int peer_id)
	:updated_at_(0)
{
	room_ = room;
	peer_id_ = peer_id;
	updated_at_ = time(nullptr);
}

any_user::~any_user()
{
}

void any_user::release()
{
	room_->remove_user(peer_id_);
}

bool any_user::is_timeout()const
{
	time_t now = time(nullptr);
	time_t ts = now - updated_at_;
	return ts >= 30;
}

void any_user::input_frame(any_frame_t* frame)
{
	invoke_frame(frame);
}

void any_user::add_track(int track, any_frame_event_t frame_event, void* ctx)
{
	track_t t;
	t.track = track;
	t.frame_event = frame_event;
	t.ctx = ctx;
	std::unique_lock<std::shared_mutex> lk(mutex_);

	auto itr = std::find_if(tracks_.begin(), tracks_.end(), [track, frame_event,ctx](const track_t& a) {
		return a.track == track && a.frame_event == frame_event && a.ctx == ctx;
	});

	if (itr == tracks_.end())
	{
		tracks_.push_back(t);
	}
	if (track >= 0) {
		_pick();
	}
}

size_t any_user::remove_track(int track, any_frame_event_t frame_event, void* ctx)
{
	std::unique_lock<std::shared_mutex> lk(mutex_);
	auto itr=std::find_if(tracks_.begin(), tracks_.end(), [track,frame_event,ctx](const track_t& a) {
		return a.track == track && a.frame_event == frame_event && a.ctx == ctx;
		});
	if (itr != tracks_.end())
	{
		tracks_.erase(itr);
	}
	if (track >= 0) {
		_pick();
	}
	return tracks_.size();
}

void any_user::clear_tracks()
{
	std::unique_lock<std::shared_mutex> lk(mutex_);
	tracks_.clear();
	_pick();
}

size_t any_user::count_tracks()
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	return tracks_.size();
}

void any_user::auto_pick()
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	_pick();
}



void any_user::invoke_frame(any_frame_t* frame)
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	for (auto itr = tracks_.begin(); itr != tracks_.end(); itr++)
	{
		if (itr->frame_event)
		{
			if (itr->track == frame->track)
			{
				if (frame->angle >= 0) {
					itr->angle_ = frame->angle;
				}
				else {
					((any_frame_t*)frame)->angle = itr->angle_;
				}

				if (itr->last_ts_ > 0)
				{
					frame->duration = (int)(frame->pts - itr->last_ts_);
				}
				itr->frame_event(itr->ctx,room_->room_id(), peer_id(), frame);
				itr->last_ts_ = frame->pts;
			}
		}
	}

	
}



void any_user::_pick()
{
	int mask = 0;
	if (!room_||!room_->any_)
	{
		return;
	}

	for (auto itr = tracks_.begin(); itr != tracks_.end(); itr++)
	{
		if (itr->track >= 0) {
			int tmp = (int)std::pow<int>(2, itr->track);
			mask |= tmp;
		}
	}
	if (mask > 0) {
		room_->any_->set_picker(peer_id_, 3);
		room_->any_->set_track(peer_id_, mask, true);
	}
	else
	{
		room_->any_->set_picker(peer_id_, 0);
	}
}

