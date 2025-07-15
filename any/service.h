/*
	transcoder service interface
	
	ver 2.00 @ 2018/06/08
	    3.00 @ 2019/06/26
	
	2.00 
	  introduce 'task_info_c' for different g++ version compatible
	
	3.00
	  modify struct for more general usage  
 */

#ifndef __OOK_TASK_SERVICE_H__
#define __OOK_TASK_SERVICE_H__

#include <stdlib.h>
#include <string.h>
#include <string>

#include <ook/genpar.h>

#ifdef WIN32
#define SERVICE_STDCALL
///#define SERVICE_STDCALL __stdcall
#else
#define SERVICE_STDCALL
#endif


/////////////////////////////
// service callback interface
 
class ifservice_callback
{
public:
	enum service_cb
	{
		e_service_cb_unknow = -1,
		e_service_cb_null,
		
		e_service_cb_dura,
		e_service_cb_picxchg, // pic will be changed within so
		e_service_cb_charchg = e_service_cb_picxchg,
		
		e_service_cb_samplerate,
		e_service_cb_channels,
		e_service_cb_width,
		e_service_cb_height,
		
		e_service_cb_pause,
		e_service_cb_stamprevi,
		e_service_cb_stampcacu = e_service_cb_stamprevi,
		e_service_cb_playratio,
		
		e_service_cb_filestate, // will be aborted
		e_service_cb_version,
		e_service_cb_statinf,
		e_service_cb_miscinf,
		
		e_service_cb_pic  = 0x80,
		e_service_cb_pcm,
		e_service_cb_enc,
		e_service_cb_dec,
		e_service_cb_frm,
		e_service_cb_label,
		e_service_cb_recv,
		e_service_cb_plugin,
		
		e_service_cb_ctrl = 0x90,
		e_service_cb_appdata,
						
		e_service_cb_chr  = 0x100,
		e_service_cb_mediachr,
		e_service_cb_warnrept,
		e_service_cb_script,
		e_service_cb_json
	};
	
	virtual ~ifservice_callback() {}

	virtual int onservice_callback(int opt,
								   int chr,
								   int sub,
								   int wparam,
								   int lparam,
								   void * ptr) = 0;	
};


/////////////////////////////
// so interface

typedef int	(SERVICE_STDCALL * __service_libversion)();
typedef int	(SERVICE_STDCALL * __service_libchar)   ();
typedef int	(SERVICE_STDCALL * __service_libtrace)  (int);
typedef int	(SERVICE_STDCALL * __service_libinit)   (const char * xml_c, void * task, void ** ctx);
typedef int	(SERVICE_STDCALL * __service_liboper)   (int, int, int, void * ptr, void * ctx);
typedef int	(SERVICE_STDCALL * __service_libstop)   (void * ctx);

struct task_service_s
{
	int chr;
	int pos;
	
	__service_libinit init;
	__service_liboper oper;
	__service_libstop stop;
	
	void * ctx;
	void * pri; 		// used by transcoder, do NOT modify within so
	
	unsigned int ver; 	// API version add @ 2018/02/20
	unsigned int cid_f; // filter some cid add @ 2020/05/16
};

inline void init_task_service_s(task_service_s * s)
{
	s->chr   = -1;   
	s->pos   = -1;
	s->ver   = 0;
	s->cid_f = 0;
	s->init  = NULL;
	s->oper  = NULL;
	s->stop  = NULL;
	s->ctx   = NULL;
	s->pri   = NULL;	
}

typedef task_service_s service_contxt;

struct task_service_intf_oper
{
	__service_liboper oper;
	
	void * ctx;
	
	int task_label;
};


/////////////////////////////
//	macro

//
// data_type
//
#define SERVICE_DATATYPE_DEP		 	0x00 // misc type
#define SERVICE_DATATYPE_FRM   		 	0x01 // frame
#define SERVICE_DATATYPE_PIC   		 	0x02 // picture
#define SERVICE_DATATYPE_PCM   		 	0x03 // PCM data
#define SERVICE_DATATYPE_PKG   		 	0x04 // package
#define SERVICE_DATATYPE_TXT   		 	0x04 // text
#define SERVICE_DATATYPE_CHR			0x08 // so private

//
// opt
//

// will be aborted {
#define SERVICE_OPT_PREVSTOP		 	0xF0 
#define SERVICE_OPT_SESSION		 		0xF1
#define SERVICE_OPT_SESSNAME	 		0xF3
// }

#define SERVICE_OPT_ENCODEINFO 		 	0x01
#define SERVICE_OPT_DISPLAY			 	0x02
#define SERVICE_OPT_FILEOPER		 	0x04
#define SERVICE_OPT_GENERALCTRL		 	0x05 // general ctrl
#define SERVICE_OPT_TASKSRCINFO			0x06 // add @ 2017/12/13 for srcinfo update
#define SERVICE_OPT_COMMAND			 	0x08

#define SERVICE_OPT_ENCSTARTING			0x10
#define SERVICE_OPT_ENCINPTOVER			0x11
#define SERVICE_OPT_ENCCOMPLETED		0x12

#define SERVICE_OPT_PTZCTRL				0x20
#define SERVICE_OPT_CMDUPDATE	 		0x80
#define SERVICE_OPT_SERVICECB		 	0x81

#define SERVICE_OPT_INITED				0xE0 // call after init add @ 2023/04/30

#define SERVICE_OPT_TTID				0xF4

#define SERVICE_OPT_SPACERTYPE			0xF5
#define SERVICE_OPT_TASKSTAT		 	0xF6
#define SERVICE_OPT_TASKCONF		 	0xF7 // pass conf info to service
#define SERVICE_OPT_TASKBYCMD	 		0xF8
#define SERVICE_OPT_SERVOPER	 		0xF9

#define SERVICE_OPT_DECSTOP		 	    0xFA
#define SERVICE_OPT_ONTIME	 			0xFB
#define SERVICE_OPT_MISCOPER	 		0xFC
#define SERVICE_OPT_SESSNAMES	 		0xFD
#define SERVICE_OPT_SESSIONS	 		0xFE
#define SERVICE_OPT_PRVSTOPS		 	0xFF

//
// pos
//
#define SERVICE_POSITION_NULL     	 	 0x00

#define SERVICE_POSITION_VIDEO_BFDEC 	 0x01
#define SERVICE_POSITION_VIDEO_AFDEC 	 0x02
#define SERVICE_POSITION_VIDEO_BFENC 	 0x04
#define SERVICE_POSITION_VIDEO_AFENC 	 0x08

#define SERVICE_POSITION_AUDIO_BFDEC 	 0x10
#define SERVICE_POSITION_AUDIO_AFDEC 	 0x20
#define SERVICE_POSITION_AUDIO_BFENC 	 0x40
#define SERVICE_POSITION_AUDIO_AFENC 	 0x80

#define SERVICE_POSITION_MEDIA_AFPKG   0x0100 // packet after packing
#define SERVICE_POSITION_MEDIA_INFRM   0x0200 // input frame
#define SERVICE_POSITION_TEXT_INPUT    0x0400

#define SERVICE_POSITION_AUDIO_FMSVR   0x1000 // special for linkmic usage
#define SERVICE_POSITION_EMPTY_INPUT   0x2000
#define SERVICE_POSITION_VIDEO_DELAY   0x4000 // for delay encoding usage
#define SERVICE_POSITION_VIDEO_ASSRC   0x8000

#define SERVICE_POSITION_NO_CHECK     0x10000
#define SERVICE_POSITION_PICEXTRACT   0x20000 // pic extract
#define SERVICE_POSITION_PLUGINCB     0x40000 // for service call after plugin callback, add @ 2024/06/28
#define SERVICE_POSITION_PLUGINCB_CHR 0x80000

//
// callId
//
#define SERVICE_CALLID_VIDEO_PLUGIN    0x1000
#define SERVICE_CALLID_VIDEO_BACKUP    0x2000

//
// SERVICE_OPT_FILEOPER sub-opt 
//
#define SERVICE_FILEOPER_SAVING 		0x01
#define SERVICE_FILEOPER_COMPLETED 		0x02

#define SERVICE_FILEOPER_TOPINDEX		0x09

#define SERVICE_FILEOPER_PICFOLDER		0x0A
#define SERVICE_FILEOPER_FILENAME 		0x0B
#define SERVICE_FILEOPER_HLSMODE 		0x0C

#define SERVICE_FILEOPER_EXTRAXML		0x0D // for service callback
#define SERVICE_FILEOPER_ERRCODE 		0x0E

//
// SERVICE CHARACTER
//
#define SERVICE_CHARACTER_AUDIO			0x0001 // plugin cb-mode for audio
#define SERVICE_CHARACTER_VIDEO			0x0002 // plugin cb-mode for video

#define SERVICE_CHARACTER_ATFIRST		0x0004 // call plugin at first(before shareing)
#define SERVICE_CHARACTER_ASSRC			0x0008
#define SERVICE_CHARACTER_LABELBYCB		0x0010 // label function has ready in pluin
#define SERVICE_CHARACTER_PLUGINCB_CHR	0x0020

#define SERVICE_CHARACTER_NODEC			0x0100
#define SERVICE_CHARACTER_NOENC			0x0200

#define SERVICE_CHARACTER_KFDTS			0x1000 // send kframe' DTS

//
// service query
//
#define SERVICE_QUERY_OPT_FOURCC		0x1FFF


/////////////////////////////
// struct for service call

/*
	task_info_c
	
	main struct for so init
 */
#pragma pack (8)

struct task_info_c
{
	ifservice_callback * ifcb;
	
	void * extra;
	
	const char * src_label;
	const char * src_addr;
	const char * dst_label;
	const char * dst_addr;
	
	const char * TTID;
	const char * cur_path;
	const char * ext_info;
	
	unsigned int * session;
};

#pragma pack ()

struct task_info_s
{
	std::string src_label;
	std::string src_addr;
	std::string dst_label;
	std::string dst_addr;
	
	void * extra;
	ifservice_callback * ifcb;

	// for g++ version compatible
#pragma pack (1)
	char magic[8]; // 0x54494E4600000000
	void * tc;
#pragma pack ()
};

inline task_info_c * scan_task_inf_c(void * task)
{
	task_info_c   * c = NULL;
	unsigned char * p = (unsigned char *)task + sizeof(void *);
	int n = sizeof(task_info_s) - (int)sizeof(void *);
	int l = (int)sizeof(void *);
	while(l < n)
	{
		if(p[0] == 0x54 && p[1] == 0x49 && p[2] == 0x4E && p[3] == 0x46) // TINF
		{
			memcpy(&c, p + 8, sizeof(void *));
			///printf("[%p]\n", c);
			break;
		}
		p++;
		l++;
	}
	return c;
}

/*
	service call struct, using for all kinds of liboper call
 */
struct task_service_oper_s
{
	unsigned int session;
	int servid;
	
	int pos;
	int par;
	
	void * ptr;
	void * arg; // recommend pointing to serv_gen_param_s
	
	const void * ctx; // add @ 2022/09/11
};

inline void init_task_service_oper_s(task_service_oper_s * s)
{
	s->session = 0;
	s->servid  = 0;
	s->pos     = 0;
	s->par     = 0;
	s->ptr     = NULL;
	s->arg     = NULL;
	s->ctx     = NULL;
}

/*
	service callback struct
 */
struct av_picture;

struct task_service_return_s
{
	int oper;
	int stat;
	int code;
	
	int x[4];
	int y[4];
	
	av_picture * pic;
	void       * ptr; // recommend pointing to serv_gen_param_s
};

inline void init_task_service_return_s(task_service_return_s * s)
{
	s->oper = 0;
	s->stat = -1;
	s->code = 0;
	
	s->x[0] = s->x[1] = s->x[2] = s->x[3] = 0;
	s->y[0] = s->y[1] = s->y[2] = s->y[3] = 0;
	
	s->pic  = NULL;
	s->ptr  = NULL;
}

/*
	encode service ctrl
 */
#define SERV_ENC_CHR_NOFRAMECTRL 0x01

struct task_service_enc_s
{
	void * enc;
	void * pic; // also as PCM for audio encoding
	void * frm;
	
	struct enc_char_s
	{
		int streamtype;
		
		int w; // dst width,  also as samplerate
		int h; // dst height, also as channels
		
		int frames;
		int keyframes;
		int bitrate;
		int bitratectrl;
		int refframes;
		int bframes;
		int profile;
		int level;

		int w_src;
		int h_src;
		int p_src; // pixformat for src
		
		void * extra;
	} chr;
};

/*
	decode service ctrl
 */
struct task_service_dec_s
{
	void * dec;
	void * frm;
	void * pic;
	void * pcm;
	
	struct dec_char_s
	{
		int streamtype;
		int deinterlace;
		int syncdura; // audio sync framedura
	} chr;
	
	struct dec_ext_s
	{
		int fourcc;
		int param;
		void * ptr;
	} ext;
	
	void * ptr; // pointing to serv_gen_param_s add @ 2020/10/07
};

/*
	receiver service ctrl
 */
struct task_service_recv_s
{
	void * recv;
	void * ctrl;
		
	const char * label;
	const char * addr;
};

struct task_service_cb_s
{
	int opt;
	int chr;
	int sub;
	int wparam;
	int lparam;
	void * ptr;
};

///////////////////////////////
// inner usage

struct task_info_detail_s
{	
	std::string TTID;
	std::string cur_path;
	std::string extinfo;
};

#endif
