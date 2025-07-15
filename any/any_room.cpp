#include "any_room.h"

any_room::any_room()
	:updated_at_(0)
{
	updated_at_ = time(nullptr);
}

any_room::~any_room()
{
	close();
}

bool any_room::open(const any_room_options_t& opts)
{
	std::unique_lock<std::shared_mutex> lk(mutex_);
	room_id_ = opts.room_id;
	local_id_ = opts.local_user_id;
	sending_video_ = opts.sending_video;
	sending_audio_ = opts.sending_audio;
	recving_video_ = opts.recving_video;
	recving_audio_ = opts.recving_audio;
	no_pick_audio_ = opts.no_pick_audio;

	updated_at_ = time(nullptr);

	any_ = std::make_shared<any_client>();
	any_->set_event(this);


	if (!any_->init(opts))
	{
		any_.reset();
		return false;
	}

	return true;
}

void any_room::close()
{
	std::shared_ptr<any_client> any2;
	{
		std::unique_lock<std::shared_mutex> lk(mutex_);
		users_.clear();
		any2 = any_;
		any_.reset();
	}
	if (any2)
	{
		any2->close();
	}
}



bool any_room::is_timeout()const
{
	time_t now = time(nullptr);
	time_t ts = now - updated_at_;
	return ts >= 30;
}

bool any_room::enable_send_video(bool enabled)
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	sending_video_ = enabled;
	if (!any_)
	{
		return false;
	}
	return any_->enable_send_video(enabled);
}

bool any_room::enable_send_audio(bool enabled)
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	sending_audio_ = enabled;
	if (!any_)
	{
		return false;
	}
	return any_->enable_send_audio(enabled);
}

bool any_room::enable_recv_video(bool enabled)
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	recving_video_ = enabled;
	if (!any_)
	{
		return false;
	}
	return any_->enable_recv_video(enabled);
}

bool any_room::enable_recv_audio(bool enabled)
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	recving_audio_ = enabled;
	if (!any_)
	{
		return false;
	}
	return any_->enable_recv_audio(enabled);
}


bool any_room::set_no_pick_audio(bool enabled)
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	no_pick_audio_ = enabled;
	if (!any_)
	{
		return false;
	}
	return any_->set_no_pick_audio(enabled);
}

bool any_room::set_mcu_track_mask(int mask)
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	if (!any_)
	{
		return false;
	}
	return any_->set_mcu_track(mask);
}

bool any_room::send_frame(const any_frame_t* frame)
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	if (!any_)
	{
		return false;
	}
	updated_at_ = time(nullptr);
	return any_->input_frame(*frame);
}

std::string any_room::make_key(int room_id, int peer_id)
{
	return std::to_string(room_id) + "_" + std::to_string(peer_id);
}

int any_room::room_id()const
{
	return room_id_;
}

std::string any_room::key()const 
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	return make_key(room_id_, local_id_);
}


any_user_ptr any_room::get_user(int peer_id)
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	auto itr = users_.find(peer_id);
	if (itr == users_.end())
	{
		return nullptr;
	}
	return itr->second;
}

size_t any_room::remove_user(int peer_id)
{
	std::unique_lock<std::shared_mutex> lk(mutex_);
	users_.erase(peer_id);
	return users_.size();
}

size_t any_room::user_count()
{
	std::shared_lock<std::shared_mutex> lk(mutex_);
	return users_.size();
}

void any_room::clear_users()
{
	std::unique_lock<std::shared_mutex> lk(mutex_);
	users_.clear();
}

void any_room::all_users(std::vector<any_user_ptr>& users)
{
	users.clear();
	std::shared_lock<std::shared_mutex> lk(mutex_);
	users.reserve(users_.size());
	for (auto itr = users_.begin(); itr != users_.end(); itr++)
	{
		users.push_back(itr->second);
	}
}

any_user_ptr any_room::add_track(int user_id, int track_id, any_frame_event_t onframe, void* ctx)
{
	std::unique_lock<std::shared_mutex> lk(mutex_);
	auto itr = users_.find(user_id);
	if (itr != users_.end())
	{
		itr->second->add_track(track_id, onframe, ctx);
		return itr->second;
	}
	auto user = std::make_shared<any_user>(shared_from_this(), user_id);
	users_[user->peer_id()] = user;

	user->add_track(track_id, onframe, ctx);
	return user;
}

size_t any_room::remove_track(int user_id, int track_id, any_frame_event_t onframe, void* ctx)
{
	std::unique_lock<std::shared_mutex> lk(mutex_);
	auto itr = users_.find(user_id);
	if (itr == users_.end())
	{
		return users_.size();
	}
	size_t c=itr->second->remove_track(track_id, onframe, ctx);
	if (c == 0) {
		users_.erase(itr);
	}
	return users_.size();
}

size_t any_room::remove_user_tracks(int user_id)
{
	std::unique_lock<std::shared_mutex> lk(mutex_);
	auto itr = users_.find(user_id);
	if (itr == users_.end())
	{
		return users_.size();
	}

	itr->second->clear_tracks();
	users_.erase(itr);
	return users_.size();
}

void any_room::remove_tracks()
{
	std::unique_lock<std::shared_mutex> lk(mutex_);
	for (auto itr = users_.begin(); itr != users_.end(); itr++)
	{
		itr->second->clear_tracks();
	}
	users_.clear();
}

void any_room::on_connected()
{
	if (any_) {
		any_->set_no_pick_audio(no_pick_audio_);
		any_->enable_recv_audio(recving_audio_);
		any_->enable_recv_video(recving_video_);
		any_->enable_send_audio(sending_audio_);
		any_->enable_send_video(sending_video_);
	}
	updated_at_ = time(nullptr);
	std::vector<any_user_ptr> users;
	all_users(users);
	for (auto itr = users.begin(); itr != users.end(); itr++)
	{
		(*itr)->auto_pick();
	}
	connected_event.invoke();
}

void any_room::on_disconnected()
{
	disconnected_event.invoke();
}

void any_room::on_frame(int roomId, int peerId, const any_frame_t& frame)
{
	updated_at_ = time(nullptr);
	if (frame.mt == any_media_type_video)
	{
		std::vector<any_user_ptr> users;
		all_users(users);
		for (auto itr = users.begin(); itr != users.end(); itr++)
		{
			if ((*itr)->peer_id() == peerId)
			{
				(*itr)->input_frame((any_frame_t*)&frame);
			}
		}
	}
	else 
	{
		frame_event.invoke(roomId, peerId, &frame);
	}
}

void any_room::on_pcm(int roomId, int peerId, const any_pcm_t& pcm)
{
	if (!pcm.data || pcm.channels == 0 || pcm.bits == 0)
		return;

	updated_at_ = time(nullptr);
	pcm_event.invoke(roomId, peerId, &pcm);
}

void any_room::on_member_in(int roomId, int peerId)
{
	user_join_event.invoke(roomId, peerId);
}
void any_room::on_member_out(int roomId, int peerId)
{
	user_leave_event.invoke(roomId, peerId);
}

void any_room::on_down_level(int roomId, int peerId, int cc, int sc)
{
}

void any_room::on_up_level(int roomId, int peerId, int level)
{

}
