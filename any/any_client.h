#pragma once

#ifndef _WIN32
#include <dlfcn.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <map>
#include <vector>
#include <sstream>
#include <thread>
#include <memory>

#include "service.h"
#include "inc/ook/codecs/avdef.h"
#include "inc/ook/streamdef.h"
#include "inc/ook/fourcc.h"
#include "inc/ook/streamdef.h"
#include "inc/ook/genpar.h"

#include "any_def.h"
#include <sys2/signal.h>



#define UPLOAD_STATUS_DNSERROR		  -4
#define UPLOAD_STATUS_PACKERFAIL	  -5
#define UPLOAD_STATUS_NOMACFOUND	  -6

#define UPLOAD_STATUS_INITING	 	   1
#define UPLOAD_STATUS_CONNECT	 	   2
#define UPLOAD_STATUS_OVERFLOW		   3
#define UPLOAD_STATUS_NOTIFY		   4
#define UPLOAD_STATUS_NETREPT          6
#define UPLOAD_STATUS_RCVREPT          7
#define UPLOAD_STATUS_PROBE_INFO       8
#define UPLOAD_STATUS_ADAP_PROBE       9
#define UPLOAD_STATUS_RECONNECTED     11

#define UPLOAD_PEER_CONNECT			0x10
#define UPLOAD_PEER_DISCONN			0x11

#define UPLOAD_TRACK_REPT			0x16

#define UPLOAD_REPT_STATIST	 		0x20
#define UPLOAD_REPT_RECVINF	 		0x21

struct recv_report_t
{
	int stat[2];
};

struct adaptive_probe_t
{
	int result;
	int stage[2];
	int bitrate[2];
};

struct any_options_t
{
	std::string address;
	int port = 8006;
	std::string token;
	bool is_room_token = true;
	int room_id = 0;
	int local_user_id = 0;

	std::string agent_name = "miniLive/2.00";
	int def_mcu_track = 1;		//default track for mcu
	int def_recv_track = 1;   //default track for picking
	int retry_time = 100;  //retry time ms
	int retry_count = 1;   //retry count 0-close;1-once;3-twice;
	int stat_interval = 0;   //ms
	int sending = 0;
	int recving = 1;

	// mixed pcm peer==1; 
	// 0: packet
	// 1: mixed-pcm; 
	// 2: mix-pcm&packet; 
	// 17: per-pcm&mix-pcm; 
	// 18: per-pcm&mixed-pcm&packet
	int mix_recv=0;
	bool no_ext_deep = true;
	std::string audio_codec = "OPUS";
	int audio_samplerate = 48000;
	int audio_channels = 1;
	int audio_bitrate = 64;
};

class any_client_event
{
public:
	virtual void on_connected() = 0;
	virtual void on_disconnected() = 0;
	virtual void on_frame(int roomId,int peerId,const any_frame_t& frame) = 0;
	virtual void on_pcm(int roomId, int peerId, const any_pcm_t& pcm) = 0;
	virtual void on_member_in(int roomId, int peerId) = 0;
	virtual void on_member_out(int roomId, int peerId) = 0;
	virtual void on_down_level(int roomId, int peerId, int cc, int sc) = 0;
	virtual void on_up_level(int roomId, int peerId, int level) = 0;
};


class any_client:public ifservice_callback
{
public:
	any_client();
	~any_client();

	void set_event(any_client_event* e) {
		event_ = e;
	}

	const any_options_t& options()const { return options_; }
	int room_id()const { return options_.room_id; }
	int local_user_id()const { return options_.local_user_id; }

	bool init(const any_options_t& opts);
	void close();

	bool enable_send_video(bool enabled);
	bool enable_send_audio(bool enabled);
	bool enable_recv_video(bool enabled);
	bool enable_recv_audio(bool enabled);
	bool set_x_bitrate(int sec);
	bool set_x_delay(bool enabled);
	bool set_no_pick_audio(bool enabled);
	bool set_mcu_track(int mask);
	bool get_up_status();
	bool get_down_status();
	bool set_track(int peer_id, int mask, bool sync = true);
	bool set_picker(int peer_id, int mask);
	bool set_filter(int peer_id, int mask);
	bool input_frame(const any_frame_t& frame);
	
private:
	bool load_module();
	void work(int interval);
	static int __db__(int volume);
	static void reset_av_frame_s(av_frame_s* frm);

	virtual int onservice_callback(int opt, int chr, int sub, int wparam, int lparam, void* ptr);

	std::string make_media_session();

private:
	std::mutex mutex_;
	bool active_ = false;
	void* module_ = nullptr;
	task_service_s service_;
	any_client_event* event_ = nullptr;

	any_options_t options_;
	std::shared_ptr<std::thread> work_;
	sys::signal signal_;
};