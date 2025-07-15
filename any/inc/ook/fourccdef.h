#ifndef __OOK_FOURCCDEF_H__
#define __OOK_FOURCCDEF_H__

#include "fourcc.h"

//
// file format
//
#define OOKFOURCC_FILE OOK_FOURCC('F', 'I', 'L', 'E') // file

#define OOKFOURCC_BLK_ OOK_FOURCC('b', 'l', 'k', ' ')
#define OOKFOURCC_FLV_ OOK_FOURCC('f', 'l', 'v', ' ')
#define OOKFOURCC_MP4_ OOK_FOURCC('m', 'p', '4', ' ')
#define OOKFOURCC_MP3_ OOK_FOURCC('m', 'p', '3', ' ')
#define OOKFOURCC_AAC_ OOK_FOURCC('a', 'a', 'c', ' ')
#define OOKFOURCC_MPG_ OOK_FOURCC('m', 'p', 'g', ' ')
#define OOKFOURCC_MKV_ OOK_FOURCC('m', 'k', 'v', ' ')
#define OOKFOURCC_WMV_ OOK_FOURCC('w', 'm', 'v', ' ')
#define OOKFOURCC_AVI_ OOK_FOURCC('a', 'v', 'i', ' ')
#define OOKFOURCC_RMF_ OOK_FOURCC('r', 'm', 'f', ' ')
#define OOKFOURCC_Y4M_ OOK_FOURCC('y', '4', 'm', ' ')
#define OOKFOURCC_HIK_ OOK_FOURCC('h', 'i', 'k', ' ')
#define OOKFOURCC_MXF_ OOK_FOURCC('m', 'x', 'f', ' ')
#define OOKFOURCC_WAV_ OOK_FOURCC('w', 'a', 'v', ' ')
#define OOKFOURCC_OGG_ OOK_FOURCC('o', 'g', 'g', ' ')

#define OOKFOURCC_TMF  OOK_FOURCC('T', 'M', 'F', ' ')
#define OOKFOURCC_BLK  OOK_FOURCC('B', 'L', 'K', ' ')
#define OOKFOURCC_FLV  OOK_FOURCC('F', 'L', 'V', ' ')
#define OOKFOURCC_MP4  OOK_FOURCC('M', 'P', '4', ' ')
#define OOKFOURCC_MP3  OOK_FOURCC('M', 'P', '3', ' ')
#define OOKFOURCC_AAC  OOK_FOURCC('A', 'A', 'C', ' ')
#define OOKFOURCC_MPG  OOK_FOURCC('M', 'P', 'G', ' ')
#define OOKFOURCC_MKV  OOK_FOURCC('M', 'K', 'V', ' ')
#define OOKFOURCC_WMV  OOK_FOURCC('W', 'M', 'V', ' ')
#define OOKFOURCC_AVI  OOK_FOURCC('A', 'V', 'I', ' ')
#define OOKFOURCC_RMF  OOK_FOURCC('R', 'M', 'F', ' ')
#define OOKFOURCC_Y4M  OOK_FOURCC('Y', '4', 'M', ' ')
#define OOKFOURCC_HIK  OOK_FOURCC('H', 'I', 'K', ' ')
#define OOKFOURCC_MXF  OOK_FOURCC('M', 'X', 'F', ' ')
#define OOKFOURCC_WAV  OOK_FOURCC('W', 'A', 'V', ' ')
#define OOKFOURCC_OGG  OOK_FOURCC('O', 'G', 'G', ' ')

//
// video codec
//
#define OOKFOURCC_DVDI OOK_FOURCC('D', 'V', 'D', 'I')
#define OOKFOURCC_MP42 OOK_FOURCC('M', 'P', '4', '2')
#define OOKFOURCC_MP43 OOK_FOURCC('M', 'P', '4', '3')
#define OOKFOURCC_MSS2 OOK_FOURCC('M', 'S', 'S', '2')
#define OOKFOURCC_RV30 OOK_FOURCC('R', 'V', '3', '0')
#define OOKFOURCC_RV40 OOK_FOURCC('R', 'V', '4', '0')
#define OOKFOURCC_VP6  OOK_FOURCC('V', 'P', '6', ' ')
#define OOKFOURCC_VP6F OOK_FOURCC('V', 'P', '6', 'F')
#define OOKFOURCC_VP6A OOK_FOURCC('V', 'P', '6', 'A')
#define OOKFOURCC_VP7  OOK_FOURCC('V', 'P', '7', ' ')
#define OOKFOURCC_VP8  OOK_FOURCC('V', 'P', '8', ' ')
#define OOKFOURCC_VP9  OOK_FOURCC('V', 'P', '9', ' ')
#define OOKFOURCC_TGA  OOK_FOURCC('T', 'G', 'A', ' ')

#define OOKFOURCC_AV1  OOK_FOURCC('A', 'V', '0', '1')
#define OOKFOURCC_AVS2 OOK_FOURCC('A', 'V', 'S', '2')

#define OOKFOURCC_YUY2 OOK_FOURCC('Y', 'U', 'Y', '2')
#define OOKFOURCC_UYVY OOK_FOURCC('U', 'Y', 'V', 'Y')
#define OOKFOURCC_IYUV OOK_FOURCC('I', 'Y', 'U', 'V')
#define OOKFOURCC_I420 OOK_FOURCC('I', '4', '2', '0')
#define OOKFOURCC_RGB  OOK_FOURCC('R', 'G', 'B', ' ')
#define OOKFOURCC_HFYU OOK_FOURCC('H', 'F', 'Y', 'U')

#define OOKFOURCC_MJPG OOK_FOURCC('M', 'J', 'P', 'G')
#define OOKFOURCC_TSCC OOK_FOURCC('T', 'S', 'C', 'C')
#define OOKFOURCC_TSC2 OOK_FOURCC('T', 'S', 'C', '2')
#define OOKFOURCC_CDVC OOK_FOURCC('C', 'D', 'V', 'C')
#define OOKFOURCC_CDVH OOK_FOURCC('C', 'D', 'V', 'H') // Canopus DV Video
#define OOKFOURCC_CLLC OOK_FOURCC('C', 'L', 'L', 'C')
#define OOKFOURCC_CRAM OOK_FOURCC('C', 'R', 'A', 'M')
#define OOKFOURCC_CSCD OOK_FOURCC('C', 'S', 'C', 'D')
#define OOKFOURCC_CVID OOK_FOURCC('C', 'V', 'I', 'D')
#define OOKFOURCC_CUVC OOK_FOURCC('C', 'U', 'V', 'C')
#define OOKFOURCC_DVCP OOK_FOURCC('D', 'V', 'C', 'P')
#define OOKFOURCC_FPS1 OOK_FOURCC('F', 'P', 'S', '1')
#define OOKFOURCC_IV21 OOK_FOURCC('I', 'V', '2', '1') // Intel Indeo Video
#define OOKFOURCC_IV31 OOK_FOURCC('I', 'V', '3', '1')
#define OOKFOURCC_IV32 OOK_FOURCC('I', 'V', '3', '2')
#define OOKFOURCC_IV41 OOK_FOURCC('I', 'V', '4', '1')
#define OOKFOURCC_IV50 OOK_FOURCC('I', 'V', '5', '0')
#define OOKFOURCC_LAGS OOK_FOURCC('L', 'A', 'G', 'S')
#define OOKFOURCC_MRLE OOK_FOURCC('M', 'R', 'L', 'E') // Microsoft RLE
#define OOKFOURCC_QRLE OOK_FOURCC('Q', 'R', 'L', 'E') // QuickTime Animation (RLE) video
#define OOKFOURCC_SVQ1 OOK_FOURCC('S', 'V', 'Q', '1') // Sorenson Video 1
#define OOKFOURCC_SVQ3 OOK_FOURCC('S', 'V', 'Q', '3') // Sorenson Video 3
#define OOKFOURCC_TDSC OOK_FOURCC('T', 'D', 'S', 'C')
#define OOKFOURCC_AVdn OOK_FOURCC('A', 'V', 'd', 'n') // DNxHR

//
// audio codec
//
#define OOKFOURCC_COOK OOK_FOURCC('C', 'O', 'O', 'K')
#define OOKFOURCC_TRHD OOK_FOURCC('T', 'R', 'H', 'D')
#define OOKFOURCC_NELM OOK_FOURCC('N', 'E', 'L', 'M')
#define OOKFOURCC_FLAC OOK_FOURCC('F', 'L', 'A', 'C')
#define OOKFOURCC_MGSM OOK_FOURCC('M', 'G', 'S', 'M')		// GSM Microsoft variant
#define OOKFOURCC_PCMU OOK_FOURCC('P', 'C', 'M', 'U')		// PCM mu-law
#define OOKFOURCC_PCMA OOK_FOURCC('P', 'C', 'M', 'A') 		// PCM A-law
#define OOKFOURCC_PCMB OOK_FOURCC('P', 'C', 'M', 'B')		// pcm_bluray
#define OOKFOURCC_QDM2 OOK_FOURCC('Q', 'D', 'M', '2')
#define OOKFOURCC_SIPR OOK_FOURCC('S', 'I', 'P', 'R')		// RealAudio SIPR / ACELP.NET
#define OOKFOURCC_729A OOK_FOURCC('7', '2', '9', 'A')
#define OOKFOURCC_OPUS OOK_FOURCC('O', 'P', 'U', 'S')		// opus

#define OOKFOURCC_ADPCM_IW OOK_FOURCC('A', 'D', 'I', 'W') 	// ADPCM IMA WAV
#define OOKFOURCC_ADPCM_MS OOK_FOURCC('A', 'D', 'M', 'S') 	// ADPCM Microsoft

//
// codec relative
//
#define OOKFOURCC_PTS  OOK_FOURCC('P', 'T', 'S', ' ')
#define OOKFOURCC_DTS  OOK_FOURCC('D', 'T', 'S', ' ')
#define OOKFOURCC_KPTS OOK_FOURCC('K', 'P', 'T', 'S')
#define OOKFOURCC_KDTS OOK_FOURCC('K', 'D', 'T', 'S')

//
// text format
//
#define OOKFOURCC_UTF8 OOK_FOURCC('U', 'T', 'F', '8')
#define OOKFOURCC_ASS  OOK_FOURCC('A', 'S', 'S', ' ')
#define OOKFOURCC_PGS  OOK_FOURCC('P', 'G', 'S', ' ')
#define OOKFOURCC_DVBS OOK_FOURCC('D', 'V', 'B', 'S')
#define OOKFOURCC_SC27 OOK_FOURCC('S', 'C', '2', '7')
#define OOKFOURCC_WVTT OOK_FOURCC('W', 'V', 'T', 'T')

#define OOKFOURCC_XML  OOK_FOURCC('X', 'M', 'L', ' ')
#define OOKFOURCC_JSON OOK_FOURCC('J', 'S', 'O', 'N')
#define OOKFOURCC_HTML OOK_FOURCC('H', 'T', 'M', 'L')

//
// protocol
//
#define OOKFOURCC_ONVF OOK_FOURCC('O', 'N', 'V', 'F')
#define OOKFOURCC_LMIC OOK_FOURCC('L', 'M', 'I', 'C')
#define OOKFOURCC_SMIL OOK_FOURCC('S', 'M', 'I', 'L')
#define OOKFOURCC_STUN OOK_FOURCC('S', 'T', 'U', 'N')
#define OOKFOURCC_DTLS OOK_FOURCC('D', 'T', 'L', 'S')
#define OOKFOURCC_MCU  OOK_FOURCC('M', 'C', 'U', ' ')

//
// RTP
//
#define OOKFOURCC_RTP  OOK_FOURCC('R', 'T', 'P', ' ') // RTP
#define OOKFOURCC_RTCP OOK_FOURCC('R', 'T', 'C', 'P') // RTCP
#define OOKFOURCC_RTPP OOK_FOURCC('R', 'T', 'P', 'P') // RTP port
#define OOKFOURCC_RTFB OOK_FOURCC('R', 'T', 'F', 'B') // RTCPFB
#define OOKFOURCC_SSRC OOK_FOURCC('S', 'S', 'R', 'C') // SSRC

//
// language
//
#define OOKFOURCC_ENG  OOK_FOURCC('E', 'N', 'G', ' ')
#define OOKFOURCC_CHS  OOK_FOURCC('C', 'H', 'S', ' ')

//
// label operation for transcoder
//
#define OOKFOURCC_SUBT OOK_FOURCC('s', 'u', 'b', 't') // subtitle 
#define OOKFOURCC_TEXT OOK_FOURCC('t', 'e', 'x', 't') // text
#define OOKFOURCC_TIME OOK_FOURCC('t', 'i', 'm', 'e') // timer
#define OOKFOURCC_STMP OOK_FOURCC('s', 't', 'm', 'p') // stamp
#define OOKFOURCC_WTCH OOK_FOURCC('w', 't', 'c', 'h') // watcher
#define OOKFOURCC_TICK OOK_FOURCC('t', 'i', 'c', 'k') // tick watcher
#define OOKFOURCC_COUT OOK_FOURCC('c', 'o', 'u', 't') // counter

#define OOKFOURCC_LABS OOK_FOURCC('L', 'A', 'B', 'S') // label setting
#define OOKFOURCC_LABM OOK_FOURCC('L', 'A', 'B', 'M') // label masking
#define OOKFOURCC_LABC OOK_FOURCC('L', 'A', 'B', 'C') // label ctrl

#define OOKFOURCC_CMPX OOK_FOURCC('C', 'M', 'P', 'X') // complex
#define OOKFOURCC_EFFE OOK_FOURCC('E', 'F', 'F', 'E') // effect

//
// special data struct
//
#define OOKFOURCC_BMIH OOK_FOURCC('B', 'M', 'I', 'H')
#define OOKFOURCC_MVIH OOK_FOURCC('M', 'V', 'I', 'H') // MOV STSD info
#define OOKFOURCC_WAVE OOK_FOURCC('W', 'A', 'V', 'E')

#define OOKFOURCC_ASFA OOK_FOURCC('A', 'S', 'F', 'A') // asf_audio_media_s
#define OOKFOURCC_ASFV OOK_FOURCC('A', 'S', 'F', 'V') // asf_video_media_s

#define OOKFOURCC_RMFA OOK_FOURCC('R', 'M', 'F', 'A') // for rmvb audio
#define OOKFOURCC_SDP  OOK_FOURCC('S', 'D', 'P', ' ') // SDP
#define OOKFOURCC_CNTX OOK_FOURCC('C', 'N', 'T', 'X') // contxt
#define OOKFOURCC_INST OOK_FOURCC('I', 'N', 'S', 'T') // instance

#define OOKFOURCC_GPAR OOK_FOURCC('G', 'P', 'A', 'R') // general param array struct
#define OOKFOURCC_NPIC OOK_FOURCC('N', 'P', 'I', 'C') // new picture attached

//
// state
//
#define OOKFOURCC_PAUS OOK_FOURCC('P', 'A', 'U', 'S') // pause
#define OOKFOURCC_RESU OOK_FOURCC('R', 'E', 'S', 'U') // resume
#define OOKFOURCC_OPEN OOK_FOURCC('O', 'P', 'E', 'N') // open
#define OOKFOURCC_STOP OOK_FOURCC('S', 'T', 'O', 'P') // stop
#define OOKFOURCC_TRSF OOK_FOURCC('T', 'R', 'S', 'F') // transfer
#define OOKFOURCC_ALVE OOK_FOURCC('A', 'L', 'V', 'E') // alive
#define OOKFOURCC_ENUM OOK_FOURCC('E', 'N', 'U', 'M') // enum

//
// general
//
#define OOKFOURCC_FORW OOK_FOURCC('F', 'O', 'R', 'W') // forward
#define OOKFOURCC_BCKW OOK_FOURCC('B', 'C', 'K', 'W') // backward
#define OOKFOURCC_MODE OOK_FOURCC('M', 'O', 'D', 'E') // mode
#define OOKFOURCC_UNIQ OOK_FOURCC('U', 'N', 'I', 'Q') // unique
#define OOKFOURCC_ZORD OOK_FOURCC('Z', 'O', 'R', 'D') // z order
#define OOKFOURCC_SIZE OOK_FOURCC('S', 'I', 'Z', 'E') // size
#define OOKFOURCC_FMAT OOK_FOURCC('F', 'M', 'A', 'T') // format
#define OOKFOURCC_COLO OOK_FOURCC('C', 'O', 'L', 'O') // color
#define OOKFOURCC_SPAC OOK_FOURCC('S', 'P', 'A', 'C') // space
#define OOKFOURCC_CONT OOK_FOURCC('C', 'O', 'N', 'T') // content
#define OOKFOURCC_SPLT OOK_FOURCC('S', 'P', 'L', 'T') // split
#define OOKFOURCC_LABG OOK_FOURCC('L', 'A', 'B', 'G') // label_glyph_s
#define OOKFOURCC_INTV OOK_FOURCC('I', 'N', 'T', 'V') // interval
#define OOKFOURCC_DURA OOK_FOURCC('D', 'U', 'R', 'A') // duration
#define OOKFOURCC_PATH OOK_FOURCC('P', 'A', 'C', 'H') // path
#define OOKFOURCC_BANW OOK_FOURCC('B', 'A', 'N', 'W') // bandwidth
#define OOKFOURCC_OFFL OOK_FOURCC('O', 'F', 'F', 'L') // offline
#define OOKFOURCC_REPT OOK_FOURCC('R', 'E', 'P', 'T') // report
#define OOKFOURCC_MISC OOK_FOURCC('M', 'I', 'S', 'C') // miscellaneous
#define OOKFOURCC_ADDR OOK_FOURCC('A', 'D', 'D', 'R') // address
#define OOKFOURCC_NAME OOK_FOURCC('N', 'A', 'M', 'E') // name
#define OOKFOURCC_PWD  OOK_FOURCC('P', 'W', 'D', ' ') // password
#define OOKFOURCC_PARA OOK_FOURCC('P', 'A', 'R', 'A') // parameter
#define OOKFOURCC_ID   OOK_FOURCC('I', 'D', ' ', ' ') // ID
#define OOKFOURCC_TYPE OOK_FOURCC('T', 'Y', 'P', 'E') // type
#define OOKFOURCC_CONF OOK_FOURCC('C', 'O', 'N', 'F') // conf
#define OOKFOURCC_RECV OOK_FOURCC('R', 'E', 'C', 'V') // recv
#define OOKFOURCC_SEND OOK_FOURCC('S', 'E', 'N', 'D') // send
#define OOKFOURCC_DISP OOK_FOURCC('D', 'I', 'S', 'P') // disp
#define OOKFOURCC_LOAD OOK_FOURCC('L', 'O', 'A', 'D') // load
#define OOKFOURCC_MAGI OOK_FOURCC('M', 'A', 'G', 'I') // magic
#define OOKFOURCC_PAIR OOK_FOURCC('P', 'A', 'I', 'R') // pair
#define OOKFOURCC_QUEY OOK_FOURCC('Q', 'U', 'E', 'Y') // query
#define OOKFOURCC_PEER OOK_FOURCC('P', 'E', 'E', 'R') // peer
#define OOKFOURCC_EXCT OOK_FOURCC('E', 'X', 'C', 'T') // except
#define OOKFOURCC_COPY OOK_FOURCC('C', 'O', 'P', 'Y') // copy
#define OOKFOURCC_CTRL OOK_FOURCC('C', 'T', 'R', 'L') // ctrl
#define OOKFOURCC_NTFY OOK_FOURCC('N', 'T', 'F', 'Y') // notify
#define OOKFOURCC_POWE OOK_FOURCC('P', 'O', 'W', 'E') // power
#define OOKFOURCC_VADS OOK_FOURCC('V', 'A', 'D', 'S') // VAD state
#define OOKFOURCC_GPU  OOK_FOURCC('G', 'P', 'U', ' ') // conf

#define OOKFOURCC_AND  OOK_FOURCC('A', 'N', 'D', ' ') // and
#define OOKFOURCC_OR   OOK_FOURCC('O', 'R', ' ', ' ') // or
#define OOKFOURCC_XOR  OOK_FOURCC('X', 'O', 'R', ' ') // xor

#define OOKFOURCC_TRAK OOK_FOURCC('T', 'R', 'A', 'K') // track

#define OOKFOURCC_CERT OOK_FOURCC('C', 'E', 'R', 'T') // certification
#define OOKFOURCC_PKEY OOK_FOURCC('P', 'K', 'E', 'Y') // privatekey
#define OOKFOURCC_LOG  OOK_FOURCC('L', 'O', 'G', ' ') // log
#define OOKFOURCC_OVFL OOK_FOURCC('O', 'V', 'F', 'L') // overflow
#define OOKFOURCC_STAT OOK_FOURCC('S', 'T', 'A', 'T') // status
#define OOKFOURCC_RTAT OOK_FOURCC('R', 'T', 'A', 'T') // rotate

//
// interface
//
#define OOKFOURCC_SVCB OOK_FOURCC('S', 'V', 'C', 'B') // ifservice_callback interface
#define OOKFOURCC_SMCB OOK_FOURCC('S', 'M', 'C', 'B') // ifstream_callback interface

//
// type
//
#define OOKFOURCC_DRME OOK_FOURCC('D', 'R', 'M', 'E') // DRM.encoder
#define OOKFOURCC_CBCK OOK_FOURCC('C', 'B', 'C', 'K') // callback
#define OOKFOURCC_FRMP OOK_FOURCC('F', 'R', 'M', 'P') // frame pointer
#define OOKFOURCC_TRSP OOK_FOURCC('T', 'R', 'S', 'P') // transparent
#define OOKFOURCC_PIXU OOK_FOURCC('P', 'I', 'X', 'U') // pix unit size
#define OOKFOURCC_PPIC OOK_FOURCC('P', 'P', 'I', 'C') // picture pointer
#define OOKFOURCC_M3U8 OOK_FOURCC('M', '3', 'U', '8') // M3U8
#define OOKFOURCC_TIDX OOK_FOURCC('T', 'I', 'D', 'X') // top index
#define OOKFOURCC_SERV OOK_FOURCC('S', 'E', 'R', 'V') // service
#define OOKFOURCC_PIXF OOK_FOURCC('P', 'I', 'X', 'F') // pix format

//
// CTRL
//
#define OOKFOURCC_PTZ  OOK_FOURCC('P', 'T', 'Z', ' ') // PTZ
#define OOKFOURCC_PTZC OOK_FOURCC('P', 'T', 'Z', 'C') // PTZC

#define OOKFOURCC_VCRL OOK_FOURCC('V', 'C', 'T', 'L') // VR ctrl

#define OOKFOURCC_COMD OOK_FOURCC('C', 'O', 'M', 'D') // command
#define OOKFOURCC_READ OOK_FOURCC('R', 'E', 'A', 'D') // READ

//
// ERROR
//
#define OOKFOURCC_CCER OOK_FOURCC('C', 'C', 'E', 'R') // CC error

//
// misc
//
#define OOKFOURCC_ALL  OOK_FOURCC('a', 'l', 'l', ' ')
#define OOKFOURCC_KEEP OOK_FOURCC('K', 'E', 'E', 'P')

#define OOKFOURCC_SET  OOK_FOURCC('S', 'E', 'T', ' ')
#define OOKFOURCC_GET  OOK_FOURCC('G', 'E', 'T', ' ')

#define OOKFOURCC_MAIN OOK_FOURCC('m', 'a', 'i', 'n') // main
#define OOKFOURCC_BKUP OOK_FOURCC('b', 'k', 'u', 'p') // backup
#define OOKFOURCC_ASSI OOK_FOURCC('a', 's', 's', 'i') // assi

#define OOKFOURCC_RCOV OOK_FOURCC('R', 'C', 'O', 'V') // recover

#define OOKFOURCC_IPUT OOK_FOURCC('I', 'P', 'U', 'T') // input
#define OOKFOURCC_OUTS OOK_FOURCC('O', 'U', 'T', 'S') // outside

#define OOKFOURCC_META OOK_FOURCC('M', 'E', 'T', 'A')
#define OOKFOURCC_CAHR OOK_FOURCC('C', 'H', 'A', 'R') // character
#define OOKFOURCC_UKNW OOK_FOURCC('U', 'K', 'N', 'W') // unknow
#define OOKFOURCC_USUP OOK_FOURCC('U', 'S', 'U', 'P') // unsupported
#define OOKFOURCC_CLOS OOK_FOURCC('C', 'L', 'O', 'S')
#define OOKFOURCC_SFIT OOK_FOURCC('S', 'F', 'I', 'T') // smil shift mode
#define OOKFOURCC_NULL OOK_FOURCC('N', 'U', 'L', 'L')

#define OOKFOURCC_INIT OOK_FOURCC('I', 'N', 'I', 'T')
#define OOKFOURCC_FREE OOK_FOURCC('F', 'R', 'E', 'E')
#define OOKFOURCC_ENCO OOK_FOURCC('E', 'N', 'C', 'O') // encode
#define OOKFOURCC_DECO OOK_FOURCC('D', 'E', 'C', 'O') // decode

#define OOKFOURCC_LHS  OOK_FOURCC('L', 'H', 'S', ' ')
#define OOKFOURCC_MOSA OOK_FOURCC('M', 'O', 'S', 'A')
#define OOKFOURCC_BLUR OOK_FOURCC('B', 'L', 'U', 'R')
#define OOKFOURCC_REPL OOK_FOURCC('R', 'E', 'P', 'L')

#define OOKFOURCC_MESS OOK_FOURCC('M', 'E', 'S', 'S') // message
#define OOKFOURCC_CONN OOK_FOURCC('C', 'O', 'N', 'N') // connect
#define OOKFOURCC_FILT OOK_FOURCC('F', 'I', 'L', 'T') // filter
#define OOKFOURCC_INTF OOK_FOURCC('I', 'N', 'T', 'F') // interface
#define OOKFOURCC_OPER OOK_FOURCC('o', 'p', 'e', 'r') // oper interface
#define OOKFOURCC_IFST OOK_FOURCC('I', 'F', 'S', 'T') // set  interface
#define OOKFOURCC_IFGT OOK_FOURCC('I', 'F', 'G', 'T') // get  interface
#define OOKFOURCC_NETU OOK_FOURCC('n', 'e', 't', 'u')
#define OOKFOURCC_NETD OOK_FOURCC('n', 'e', 't', 'd')

#define OOKFOURCC_PLYR OOK_FOURCC('P', 'L', 'Y', 'R') // play ratio
#define OOKFOURCC_LOUT OOK_FOURCC('L', 'O', 'U', 'T') // layout
#define OOKFOURCC_SMIX OOK_FOURCC('S', 'M', 'I', 'X') // sound mixer
#define OOKFOURCC_SRCL OOK_FOURCC('S', 'R', 'C', 'L') // source label
#define OOKFOURCC_SVCB OOK_FOURCC('S', 'V', 'C', 'B') // service callback
#define OOKFOURCC_TSKN OOK_FOURCC('T', 'S', 'K', 'N') // task name

#define OOKFOURCC_AUTO OOK_FOURCC('A', 'U', 'T', 'O') // auto
#define OOKFOURCC_PRED OOK_FOURCC('P', 'R', 'E', 'D') // pred
#define OOKFOURCC_STUF OOK_FOURCC('S', 'T', 'U', 'F')
#define OOKFOURCC_HOOK OOK_FOURCC('H', 'O', 'O', 'K')

#define OOKFOURCC_SYNC OOK_FOURCC('S', 'Y', 'N', 'C') // sync   operation
#define OOKFOURCC_ASYN OOK_FOURCC('A', 'S', 'Y', 'N') // async  operation
#define OOKFOURCC_BYPS OOK_FOURCC('B', 'Y', 'P', 'S') // bypass operation

#define TRCFOURCC_RSTD OOK_FOURCC('R', 'S', 'T', 'D') // reset decoder
#define TRCFOURCC_RSTE OOK_FOURCC('R', 'S', 'T', 'E') // reset encoder

#define OOKFOURCC_REST OOK_FOURCC('R', 'E', 'S', 'T') // reset operation
#define OOKFOURCC_PACK OOK_FOURCC('P', 'A', 'C', 'K') // packer

#define OOKFOURCC_MMOV OOK_FOURCC('M', 'M', 'O', 'V') // mouse moving
#define OOKFOURCC_MSTA OOK_FOURCC('M', 'S', 'T', 'A') // media status

#define OOKFOURCC_PKGS OOK_FOURCC('P', 'K', 'G', 'S') // package
#define OOKFOURCC_SPEE OOK_FOURCC('S', 'P', 'E', 'E') // speed

#define OOKFOURCC_PNTM OOK_FOURCC('P', 'N', 'T', 'M') // printmask
#define OOKFOURCC_PCTG OOK_FOURCC('P', 'C', 'T', 'G') // percentage

#define OOKFOURCC_CISCO_CDNINDEX OOK_FOURCC('c', 'c', 'd', 'n')

//
// class
//
#define OOKFOURCC_AUDP OOK_FOURCC('A', 'U', 'D', 'P') // audio_procedure 

#endif
