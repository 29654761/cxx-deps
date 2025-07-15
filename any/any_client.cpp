#include "any_client.h"

#include <math.h>
#include <sys2/security/md5.h>
#include "h264_sei_analyse.h"
#include "any_module.h"


any_client::any_client()
{
	init_task_service_s(&service_);
}

any_client::~any_client()
{
	close();
}

bool any_client::init(const any_options_t& opts)
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (active_) {
		return false;
	}
	active_ = true;

	options_ = opts;



	if (!load_module()) {
		active_ = false;
		return false;
	}

	std::string session;
	if (options_.is_room_token)
	{
		session = this->make_media_session();
	}
	else
	{
		session = options_.token;
	}

	std::stringstream xml;
	xml << "<complex type=\"linkmic_service\">" << std::endl;
	xml << "<param n=\"rtpsink_printmask\" v=\"0x00\"></param>" << std::endl;
	xml << "<param n=\"trace\" v=\"0\" />" << std::endl;
	xml << "<param n=\"picking\" v=\"1\" />" << std::endl;
	//xml << "<param n=\"pos\" v=\"0x2a\" />" << std::endl;
	//xml << "<param n=\"max_bitrate\" v=\"3000\" />" << std::endl;
	xml << "<def_nosend>0</def_nosend>" << std::endl;
	xml << "<ctrlmode>0</ctrlmode>" << std::endl;
	xml << "<compound>0</compound>" << std::endl;
	xml << "<amixing>9</amixing>" << std::endl;
	xml << "<dcmode>1</dcmode>" << std::endl;
	xml << "<maxclt>32</maxclt>" << std::endl;
	if(options_.mix_recv>0)
	{
		xml << "<mix_recv>"<< options_.mix_recv<<"</mix_recv>" << std::endl;
		xml << "<mixer_no_waiting>1</mixer_no_waiting>" << std::endl;
	}
	xml << "<recvcb>1</recvcb>" << std::endl;
	xml << "<snding>"<<opts.sending<<"</snding>" << std::endl;
	xml << "<rcving>" << opts.recving << "</rcving>" << std::endl;
	xml << "<autojoin>1</autojoin>" << std::endl;
	xml << "<autorun>1</autorun>" << std::endl;
	xml << "<ext_version>1</ext_version>" << std::endl;
	xml << "<upldver>2</upldver>" << std::endl;
	xml << "<video_track_mask>7</video_track_mask>" << std::endl;
	xml << "<deftrack>" << options_.def_mcu_track << "</deftrack>" << std::endl;
	xml << "<deftrack_recv>" << options_.def_recv_track << "</deftrack_recv>" << std::endl;
	xml << "<server>" << options_.address << ":" << options_.port << "</server>" << std::endl;
	xml << "<channel>" << options_.room_id << "</channel>" << std::endl;
	xml << "<lmicId>" << options_.local_user_id << "</lmicId>" << std::endl;
	xml << "<token>" << session << "</token>" << std::endl;
	xml << "<rtcpfb>" << std::endl;
	xml << "	<deep>" << options_.retry_time << "</deep>" << std::endl;
	xml << "	<ratio>0</ratio>" << std::endl;
	xml << "	<prof>" << options_.retry_count << "</prof>" << std::endl;
	if (options_.no_ext_deep) 
	{
		xml << "	<no_ext_deep>1</no_ext_deep>" << std::endl;
	}
	xml << "	<print>0x100</print>" << std::endl;
	xml << "</rtcpfb>" << std::endl;
	xml << "<audio>" << std::endl;
	xml << "	<codec>"<< options_.audio_codec <<"</codec>" << std::endl;
	xml << "	<samplerate>" << options_.audio_samplerate << "</samplerate>" << std::endl;
	xml << "	<channels>" << options_.audio_channels << "</channels>" << std::endl;
	xml << "	<bitrate>" << options_.audio_bitrate << "</bitrate>" << std::endl;
	xml << "</audio>" << std::endl;
	xml << "<system>" << std::endl;
	xml << "	<socket_rb>1024</socket_rb>" << std::endl;
	xml << "	<socket_sb>1024</socket_sb>" << std::endl;
	xml << "	<thread_prio>2</thread_prio>" << std::endl;
	xml << "	<socket_prio>6</socket_prio>" << std::endl;
	xml << "</system>" << std::endl;
	xml << "</complex>" << std::endl;
	std::string sxml = xml.str();

	int r=service_.init(sxml.c_str(), NULL, &service_.ctx);
	std::string agent_name = "LMIC.agent=";
	if (options_.agent_name.size() > 0) {
		agent_name += options_.agent_name;
	}
	else {
		agent_name += "miniLive/2.00";
	}
	
	r = service_.oper(MediaTypeUnknown, SERVICE_DATATYPE_DEP, SERVICE_OPT_COMMAND, (void*)agent_name.c_str(), service_.ctx);
	task_service_oper_s os;
	init_task_service_oper_s(&os);
	os.ptr = (ifservice_callback*)this;
	r = service_.oper(MediaTypeUnknown, SERVICE_DATATYPE_DEP, SERVICE_OPT_SESSIONS, &os, service_.ctx);
	r = service_.oper(MediaTypeUnknown, SERVICE_DATATYPE_DEP, SERVICE_OPT_INITED, NULL, service_.ctx);
	signal_.reset();
	if (options_.stat_interval > 0) {
		//m_work.reset(new std::thread(&any_client::work, this, m_options.stat_interval));
	}

	return true;
}

void any_client::close()
{
	std::lock_guard<std::mutex> lk(mutex_);
	active_ = false;
	signal_.notify();
	if (work_ != nullptr) {
		work_->join();
		work_.reset();
	}

	if (service_.stop) {
		service_.stop(service_.ctx);
	}
	
	init_task_service_s(&service_);
}

bool any_client::load_module()
{
	init_task_service_s(&service_);

	module_ = any_module::get_instance()->handle();
#ifdef _WIN32
	if (!module_) {
		return false;
	}
	__service_libchar chr = (__service_libchar)GetProcAddress((HINSTANCE)module_, "libchar");
	service_.chr = chr();
	
	__service_libtrace trace = (__service_libtrace)GetProcAddress((HINSTANCE)module_, "libtrace");
	service_.init = (__service_libinit)GetProcAddress((HINSTANCE)module_, "libinit");
	service_.stop = (__service_libstop)GetProcAddress((HINSTANCE)module_, "libstop");
	service_.oper = (__service_liboper)GetProcAddress((HINSTANCE)module_, "liboper");
#else
	if (!module_) {
		//char err[256] = { 0 };
		//snprintf(err,256,"%s", dlerror());
		//FILE* f = fopen("/opt/vcs/pic/log/c.txt", "at+");
		//fwrite(err, 256, 1, f);
		//fclose(f);

		return false;
	}
	__service_libchar chr = (__service_libchar)dlsym(module_, "libchar");
	service_.chr = chr();

	__service_libtrace trace = (__service_libtrace)dlsym(module_, "libtrace");
	service_.init = (__service_libinit)dlsym(module_, "libinit");
	service_.stop = (__service_libstop)dlsym(module_, "libstop");
	service_.oper = (__service_liboper)dlsym(module_, "liboper");
#endif

	trace(1);
	return true;
}


void any_client::work(int interval)
{
	if (interval <= 0) {
		return;
	}
	while (active_)
	{
		signal_.wait(interval);
		if (!active_) {
			break;
		}
		//getUpStatus();
		//getUpStatus();
	}


	
}



int any_client::onservice_callback(int opt, int chr, int sub, int wparam, int lparam, void* ptr)
{
	if (opt != e_service_cb_frm&&opt!= e_service_cb_pcm)
	{
		printf("upload callback opt=%d chr=%d sub=%d wp=%d lp=%d\n", opt, chr, sub, wparam, lparam);
	}
	if (opt == e_service_cb_frm)
	{
		if (ptr)
		{
			av_frame_s* f = (av_frame_s*)ptr;
			unsigned int peer = (unsigned int)wparam;
			unsigned int sess = (unsigned int)lparam;
			if (event_) {


				any_frame_t frame = {};
				if (f->medtype == MediaTypeAudio) {
					frame.mt = any_media_type_audio;
					if (f->stmtype == STREAM_TYPE_AUDIO_AAC) {
						frame.ct = any_codec_type_aac;
					}
					else {
						frame.ct = any_codec_type_opus;
					}

					//audio power
					frame.volume = -1;
					serv_gen_param_s* ps = (serv_gen_param_s*)f->arg;
					if (ps && ps->magic == OOKFOURCC_GPAR)
					{
						for (int j = 0; j < ps->count; j++)
						{
							switch (ps->n[j])
							{
							case OOKFOURCC_POWE:
								frame.volume = *(int*)ps->p[j];
								break;
							default:
								break;
							}
						}
					}

				}
				else if (f->medtype == MediaTypeVideo) {
					frame.mt = any_media_type_video;
					if (f->stmtype == STREAM_TYPE_VIDEO_H265) {
						frame.ct = any_codec_type_h265;
					}
					else {
						frame.ct = any_codec_type_h264;
					}
					frame.angle = h264_sei_analyse(f);
				}
				else if (f->medtype == MediaTypeData) {
					frame.mt = any_media_type_data;
				}

				frame.data = (unsigned char*)f->bits;
				frame.data_size = f->bitslen;
				frame.dts = f->dts;
				frame.pts = f->pts;
				frame.ft = f->frmtype ? any_frame_type_i_frame : any_frame_type_frame;
				frame.track = f->track;
				frame.tmscale = f->tmscale;
				frame.frmsequ = f->frmsequ;
				
				if (event_) {
					event_->on_frame(options_.room_id,peer, frame);
				}

			}
		}
	}
	else if (opt == e_service_cb_pcm)
	{
		int peer = (int)wparam;
		av_pcmbuff* pcm = (av_pcmbuff*)ptr;

		any_pcm_t frame;
		frame.channels = pcm->channels;
		frame.samplerate = pcm->samplerate;
		frame.trackId = pcm->track;
		frame.stamp = pcm->stamp;
		frame.flag = pcm->flag;
		frame.data = pcm->pcmbuf;
		frame.data_size = pcm->pcmlen;
		frame.bits = 16;
		//printf("anycb: pcm peerId=%d size=%d \n", peer, frame.dataSize);
		if (event_) {
			event_->on_pcm(options_.room_id, peer, frame);
		}
	}
	else if (opt == e_service_cb_appdata)
	{
		if (sub == 0x4E4E4F43) // Connect status
		{
			printf("Connecting status :wp=%d  lp=%d\n", wparam,lparam);
			if (wparam == UPLOAD_STATUS_NOMACFOUND)  //No mac address
			{

			}
			else if (wparam == UPLOAD_STATUS_PACKERFAIL)  //Pack failed
			{

			}
			else if (wparam == UPLOAD_STATUS_DNSERROR)  // DNS Error
			{

			}
			else if (wparam == UPLOAD_STATUS_INITING)    //Begin connecting
			{

			}
			else if (wparam == UPLOAD_STATUS_CONNECT)  // Connect result.
			{
				if (lparam == -1) // Disconnect
				{
					if (event_) {
						event_->on_disconnected();
					}
				}
				else if (lparam == 0) // Connecting
				{

				}
				else if (lparam == 1 || lparam == 3) //Connected
				{
					if (event_) {
						event_->on_connected();
					}
				}
				else if (lparam == 2) // Connect failed
				{

				}
			}
			else if (wparam == UPLOAD_PEER_CONNECT) // New peer connected
			{
				unsigned int peer = (unsigned int)lparam;
				if (event_) {
					event_->on_member_in(options_.room_id,peer);
				}
			}
			else if (wparam == UPLOAD_PEER_DISCONN) // Peer disconnected
			{
				unsigned int peer = (unsigned int)lparam;
				if (event_) {
					event_->on_member_out(options_.room_id, peer);
				}
			}
			else if (wparam == UPLOAD_STATUS_NOTIFY) // Error message
			{
				// 101:peer overflow
				// 102:auth fail
			}
			else if (wparam == UPLOAD_TRACK_REPT) // Track lost report
			{
				// loss::peer = 12345678 track = 0
			}
			else if (wparam == UPLOAD_REPT_STATIST) // Send status report
			{
				const char* str = (const char*)ptr;
				if (event_) {
				}
			}
			else if (wparam == UPLOAD_REPT_RECVINF) // Recv status report
			{
				const char* str = (const char*)ptr;
				if (event_) {
				}
			}
			else if (wparam == UPLOAD_STATUS_RCVREPT)  // Net Level for each peer
			{
				unsigned int peer = lparam;
				recv_report_t* rs = (recv_report_t*)ptr;

				if (event_) {
					event_->on_down_level(options_.room_id,peer, rs->stat[0], rs->stat[1]);
				}
			}
			else if (wparam == UPLOAD_STATUS_NETREPT) // Net Level for self
			{
				int level = lparam;
				if (level == 1000)
					level = 0;

				if (event_) {
					event_->on_up_level(options_.room_id, options_.local_user_id,level);
				}
			}
			if (wparam == UPLOAD_STATUS_ADAP_PROBE)
			{
				//Upload probe
			}
		}
		else if (sub == 0x424F5250)
		{
			if (wparam == UPLOAD_STATUS_PROBE_INFO)
			{
				if (ptr)
				{
					const char* inf = (const char*)ptr;
					// upld::recv=191 miss=10 losf=18 speed=2029127 delay=20
					// down::recv=1164 miss=41 losf=67 speed=2078873 delay=22

				}
				else
				{
					//Test finished
				}

			}
		}
	}
	else if (sub == OOK_FOURCC('L', 'O', 'G', ' '))
	{
		if (ptr)
		{
			const char* log = (const char*)ptr;
			if (lparam == OOK_FOURCC('X', 'D', 'L', 'Y'))
			{
			}
		}

	}
	return 0;
}



bool any_client::enable_send_video(bool enabled)
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (!active_)
	{
		return false;
	}
	char cmd[128] = { 0 };
	snprintf(cmd, 128, "LMIC.send_novideo=%d", enabled ? 0 : 1);
	int r=service_.oper(MediaTypeUnknown, SERVICE_DATATYPE_DEP, SERVICE_OPT_COMMAND, (void*)cmd, service_.ctx);
	return r==0;
}

bool any_client::enable_send_audio(bool enabled)
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (!active_)
	{
		return false;
	}

	char cmd[128] = { 0 };
	snprintf(cmd, 128, "LMIC.send_noaudio=%d", enabled ? 0 : 1);
	int r=service_.oper(MediaTypeUnknown, SERVICE_DATATYPE_DEP, SERVICE_OPT_COMMAND, (void*)cmd, service_.ctx);
	return r == 0;
}

bool any_client::enable_recv_video(bool enabled)
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (!active_)
	{
		return false;
	}
	char cmd[128] = { 0 };
	snprintf(cmd, 128, "LMIC.recv_novideo=%d", enabled ? 0 : 1);
	int r=service_.oper(MediaTypeUnknown, SERVICE_DATATYPE_DEP, SERVICE_OPT_COMMAND, (void*)cmd, service_.ctx);
	return r == 0;
}

bool any_client::enable_recv_audio(bool enabled)
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (!active_)
	{
		return false;
	}

	char cmd[128] = { 0 };
	snprintf(cmd, 128, "LMIC.recv_noaudio=%d", enabled ? 0 : 1);
	int r=service_.oper(MediaTypeUnknown, SERVICE_DATATYPE_DEP, SERVICE_OPT_COMMAND, (void*)cmd, service_.ctx);
	return r == 0;
}

bool any_client::set_x_bitrate(int sec)
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (!active_)
	{
		return false;
	}
	char cmd[128] = { 0 };
	snprintf(cmd, 128, "LMIC.xbitrate=%d", sec);
	int r=service_.oper(MediaTypeUnknown, SERVICE_DATATYPE_DEP, SERVICE_OPT_COMMAND, (void*)cmd, service_.ctx);
	return r == 0;
}

bool any_client::set_x_delay(bool enabled)
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (!active_)
	{
		return false;
	}

	char cmd[128] = { 0 };
	snprintf(cmd, 128, "LMIC.xdelay=%d", 100);
	int r=service_.oper(MediaTypeUnknown, SERVICE_DATATYPE_DEP, SERVICE_OPT_COMMAND, (void*)cmd, service_.ctx);
	return r == 0;
}
bool any_client::set_no_pick_audio(bool enabled)
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (!active_)
	{
		return false;
	}
	char cmd[128] = { 0 };
	snprintf(cmd, 128, "LMIC.nopickaudio=%d", enabled ? 1 : 0);
	int r=service_.oper(MediaTypeUnknown, SERVICE_DATATYPE_DEP, SERVICE_OPT_COMMAND, (void*)cmd, service_.ctx);
	return r == 0;
}

bool any_client::set_mcu_track(int mask)
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (!active_)
	{
		return false;
	}
	char cmd[128] = { 0 };
	snprintf(cmd, 128, "LMIC.deftrack=%d", mask);
	int r=service_.oper(MediaTypeUnknown, SERVICE_DATATYPE_DEP, SERVICE_OPT_COMMAND, (void*)cmd, service_.ctx);
	return r == 0;
}

bool any_client::get_up_status()
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (!active_)
	{
		return false;
	}

	int r=service_.oper(MediaTypeUnknown, SERVICE_DATATYPE_DEP, SERVICE_OPT_COMMAND,
		(void*)"LMIC.upld.inf", service_.ctx);
	return r == 0;
}

bool any_client::get_down_status()
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (!active_)
	{
		return false;
	}

	int r=service_.oper(MediaTypeUnknown, SERVICE_DATATYPE_DEP, SERVICE_OPT_COMMAND,
		(void*)"LMIC.recv.inf", service_.ctx);
	return r == 0;
}


bool any_client::set_track(int userId, int mask, bool sync)
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (!active_)
	{
		return false;
	}

	if (sync) {
		mask |= 4096;
	}
	char cmd[128] = { 0 };
	snprintf(cmd, 128, "LMIC.track=%d:%d", mask, userId);
	int r=service_.oper(MediaTypeUnknown, SERVICE_DATATYPE_DEP, SERVICE_OPT_COMMAND, (void*)cmd, service_.ctx);
	return r == 0;
}

bool any_client::set_picker(int userId, int mask)
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (!active_)
	{
		return false;
	}

	char cmd[128] = { 0 };
	snprintf(cmd, 128, "LMIC.picker=%d:%d", mask, userId);
	int r=service_.oper(MediaTypeUnknown, SERVICE_DATATYPE_DEP, SERVICE_OPT_COMMAND, (void*)cmd, service_.ctx);
	return r == 0;
}

bool any_client::set_filter(int userId, int mask)
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (!active_)
	{
		return false;
	}
	char cmd[128] = { 0 };
	snprintf(cmd, 128, "LMIC.filter=%d:%d", mask, userId);
	int r=service_.oper(MediaTypeUnknown, SERVICE_DATATYPE_DEP, SERVICE_OPT_COMMAND, (void*)cmd, service_.ctx);
	return r == 0;
}

bool any_client::input_frame(const any_frame_t& frame)
{
	std::lock_guard<std::mutex> lk(mutex_);
	if (!active_)
	{
		return false;
	}

	int frametype = 0;
	if (frame.ft == any_frame_type_idr || frame.ft == any_frame_type_i_frame)
		frametype = 1;


	av_frame_s f;
	reset_av_frame_s(&f);

	static uint32_t idx = 0;
	if (frame.mt == any_media_type_video)
	{
		f.medtype = MediaTypeVideo;
		if (frame.ct == any_codec_type_h265)
			f.stmtype = STREAM_TYPE_VIDEO_H265;
		else
			f.stmtype = STREAM_TYPE_VIDEO_H264;
		f.frmtype = frametype;
		f.tmscale = frame.tmscale;
		f.pts = (int64_t)frame.pts;
		f.dts = (int64_t)frame.dts;
		f.dur = frame.duration;
		f.bits = (unsigned char*)frame.data;
		f.bitslen = frame.data_size;
		f.track = frame.track;
	}
	else if (frame.mt == any_media_type_audio)
	{
		f.medtype = MediaTypeAudio;
		if (frame.ct == any_codec_type_aac)
			f.stmtype = STREAM_TYPE_AUDIO_AAC;
		else
			f.stmtype = 0x5355504F;
		f.frmtype = frametype;
		f.tmscale = frame.tmscale;
		f.pts = frame.pts;
		f.dts = frame.dts;
		f.dur = (int)frame.duration;
		f.bits = (unsigned char*)frame.data;
		f.bitslen = frame.data_size;
		f.track = frame.track;
		f.frmmisc = 0x41;

		if (frame.volume >= 0) {
			//calc the audio power
			int power[2] = { 0 };
			int p_n[2];
			void* p_v[2];
			serv_gen_param_s gps;
			init_serv_gen_param_s(&gps);
			gps.n = p_n;
			gps.p = p_v;
			gps.n[0] = OOKFOURCC_POWE;
			gps.p[0] = (void*)power;
			gps.count = 1;
			power[0] = __db__(frame.volume);
			f.arg = &gps;
		}
	}
	else if (frame.mt == any_media_type_data)
	{
		f.medtype = MediaTypeData;
		f.bits = (unsigned char*)frame.data;
		f.bitslen = frame.data_size;
		f.track = frame.track;
	}
	else {
		return false;
	}

	service_.oper(f.medtype, SERVICE_DATATYPE_FRM, 0, (void*)&f, service_.ctx);
	return true;
}

int any_client::__db__(int volume)
{
	int db = -99;
	volume = volume > 0 ? volume : -volume;
	if (volume > 32768)
		volume = 32768;

	float f = (float)volume / 32768;
	if (f > 0.001)
	{
		db = (int)(10 * log10(f * f));
		if (db > 0)
			db = 0;
	}
	else
	{
		db = -60;
	}
	return db;

}


void any_client::reset_av_frame_s(av_frame_s* frm)
{
	if (frm)
	{
		frm->bits = NULL;
		frm->bitslen = 0;
		frm->bitspos = 0;
		frm->medtype = 0;
		frm->stmtype = 0;
		frm->frmtype = 0;
		frm->frmmisc = 0;
		frm->tmscale = 90;
		frm->frmsequ = 0;
		frm->pcr = 0;
		frm->pts = 0;
		frm->dts = 0;
		frm->dur = 0;
		frm->track = 0;
		frm->arg = NULL;
		frm->language = 0;
	}
}

std::string any_client::make_media_session()
{
	std::string s = std::to_string(options_.local_user_id) + options_.token;

	MD5 m = { 0 };
	md5((const unsigned char*)s.data(), s.size(), &m);

	char str[256] = { 0 };
	int size = md5_string(&m, str, 256);

	return std::string(str, size);
}
