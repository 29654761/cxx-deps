#pragma once
#include "psh2.h"
#include "sys_header.h"
#include "psm2.h"
#include "pes2.h"
#include "ps_block.h"




namespace rtpx {
	namespace mpeg {



#define PS_STREAM_ID_END_STREAM         0xB9
#define PS_STREAM_ID_PACK_HEADER        0xBA
#define PS_STREAM_ID_SYSTEM_HEADER      0xBB
#define PS_STREAM_ID_MAP                0xBC
#define PS_STREAM_ID_PRIVATE_STREAM1    0xBD
#define PS_STREAM_ID_PADDING            0xBE
#define PS_STREAM_ID_PRIVATE_STREAM2    0xBF
#define PS_STREAM_ID_ECM                0xF0
#define PS_STREAM_ID_EMM                0xF1
#define PS_STREAM_ID_DSMCC              0xF2
#define PS_STREAM_ID_ITU_T_H222_1       0xF8  /* ITU-T H.222.1 type E stream */
#define PS_STREAM_ID_EXTENDED           0xFD
#define PS_STREAM_ID_DIRECTORY          0xFF

#define PS_STREAM_ID_VIDEO_START        0xE0
#define PS_STREAM_ID_VIDEO_END          0xEF

#define PS_STREAM_ID_AUDIO_START        0xC0
#define PS_STREAM_ID_AUDIO_END          0xDF

#define TRICK_MODE_CONTROL_FAST_FORWARD     0x00
#define TRICK_MODE_CONTROL_SLOW_MOTION      0x01
#define TRICK_MODE_CONTROL_FREEZE_FRAME     0x02
#define TRICK_MODE_CONTROL_FAST_REVERSE     0x03
#define TRICK_MODE_CONTROL_SLOW_REVERSE     0x04

        enum EPSI_STREAM_TYPE
        {
            PSI_STREAM_RESERVED = 0x00, // ITU-T | ISO/IEC Reserved
            PSI_STREAM_MPEG1 = 0x01, // ISO/IEC 11172-2 Video
            PSI_STREAM_MPEG2 = 0x02, // Rec. ITU-T H.262 | ISO/IEC 13818-2 Video or ISO/IEC 11172-2 constrained parameter video stream(see Note 2)
            PSI_STREAM_AUDIO_MPEG1 = 0x03, // ISO/IEC 11172-3 Audio
            PSI_STREAM_MP3 = 0x04, // ISO/IEC 13818-3 Audio
            PSI_STREAM_PRIVATE_SECTION = 0x05, // Rec. ITU-T H.222.0 | ISO/IEC 13818-1 private_sections
            PSI_STREAM_PRIVATE_DATA = 0x06, // Rec. ITU-T H.222.0 | ISO/IEC 13818-1 PES packets containing private data
            PSI_STREAM_MHEG = 0x07, // ISO/IEC 13522 MHEG
            PSI_STREAM_DSMCC = 0x08, // Rec. ITU-T H.222.0 | ISO/IEC 13818-1 Annex A DSM-CC
            PSI_STREAM_H222_ATM = 0x09, // Rec. ITU-T H.222.1
            PSI_STREAM_DSMCC_A = 0x0a, // ISO/IEC 13818-6(Extensions for DSM-CC) type A
            PSI_STREAM_DSMCC_B = 0x0b, // ISO/IEC 13818-6(Extensions for DSM-CC) type B
            PSI_STREAM_DSMCC_C = 0x0c, // ISO/IEC 13818-6(Extensions for DSM-CC) type C
            PSI_STREAM_DSMCC_D = 0x0d, // ISO/IEC 13818-6(Extensions for DSM-CC) type D
            PSI_STREAM_H222_Aux = 0x0e, // Rec. ITU-T H.222.0 | ISO/IEC 13818-1 auxiliary
            PSI_STREAM_AAC = 0x0f, // ISO/IEC 13818-7 Audio with ADTS transport syntax
            PSI_STREAM_MPEG4 = 0x10, // ISO/IEC 14496-2 Visual
            PSI_STREAM_MPEG4_AAC_LATM = 0x11, // ISO/IEC 14496-3 Audio with the LATM transport syntax as defined in ISO/IEC 14496-3
            PSI_STREAM_MPEG4_PES = 0x12, // ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets
            PSI_STREAM_MPEG4_SECTIONS = 0x13, // ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC 14496_sections
            PSI_STREAM_MPEG2_SDP = 0x14, // ISO/IEC 13818-6 Synchronized Download Protocol
            PSI_STREAM_PES_META = 0x15, // Metadata carried in PES packets
            PSI_STREAM_SECTION_META = 0x16, // Metadata carried in metadata_sections
            PSI_STREAM_DSMCC_DATA = 0x17, // Metadata carried in ISO/IEC 13818-6 Data Carousel
            PSI_STREAM_DSMCC_OBJECT = 0x18, // Metadata carried in ISO/IEC 13818-6 Object Carousel
            PSI_STREAM_DSMCC_SDP = 0x19, // Metadata carried in ISO/IEC 13818-6 Synchronized Download Protocol
            PSI_STREAM_MPEG2_IPMP = 0x1a, // IPMP stream (defined in ISO/IEC 13818-11, MPEG-2 IPMP)
            PSI_STREAM_H264 = 0x1b, // H.264
            PSI_STREAM_MPEG4_AAC = 0x1c, // ISO/IEC 14496-3 Audio, without using any additional transport syntax, such as DST, ALS and SLS
            PSI_STREAM_MPEG4_TEXT = 0x1d, // ISO/IEC 14496-17 Text
            PSI_STREAM_AUX_VIDEO = 0x1e, // Auxiliary video stream as defined in ISO/IEC 23002-3
            PSI_STREAM_H264_SVC = 0x1f, // SVC video sub-bitstream of an AVC video stream conforming to one or more profiles defined in Annex G of Rec. ITU-T H.264 | ISO/IEC 14496-10
            PSI_STREAM_H264_MVC = 0x20, // MVC video sub-bitstream of an AVC video stream conforming to one or more profiles defined in Annex H of Rec. ITU-T H.264 | ISO/IEC 14496-10
            PSI_STREAM_JPEG_2000 = 0x21, // Video stream conforming to one or more profiles as defined in Rec. ITU-T T.800 | ISO/IEC 15444-1
            PSI_STREAM_MPEG2_3D = 0x22, // Additional view Rec. ITU-T H.262 | ISO/IEC 13818-2 video stream for service-compatible stereoscopic 3D services
            PSI_STREAM_MPEG4_3D = 0x23, // Additional view Rec. ITU-T H.264 | ISO/IEC 14496-10 video stream conforming to one or more profiles defined in Annex A for service-compatible stereoscopic 3D services
            PSI_STREAM_H265 = 0x24, // Rec. ITU-T H.265 | ISO/IEC 23008-2 video stream or an HEVC temporal video sub-bitstream
            PSI_STREAM_H265_subset = 0x25, // HEVC temporal video subset of an HEVC video stream conforming to one or more profiles defined in Annex A of Rec. ITU-T H.265 | ISO/IEC 23008-2
            PSI_STREAM_H264_MVCD = 0x26, // MVCD video sub-bitstream of an AVC video stream conforming to one or more profiles defined in Annex I of Rec. ITU-T H.264 | ISO/IEC 14496-10
            // 0x27-0x7E Rec. ITU-T H.222.0 | ISO/IEC 13818-1 Reserved
            PSI_STREAM_IPMP = 0x7F, // IPMP stream
            // 0x80-0xFF User Private
            PSI_STREAM_VIDEO_CAVS = 0x42, // ffmpeg/libavformat/mpegts.h
            PSI_STREAM_AUDIO_AC3 = 0x81, // ffmpeg/libavformat/mpegts.h
            PSI_STREAM_AUDIO_DTS = 0x8a, // ffmpeg/libavformat/mpegts.h
            PSI_STREAM_VIDEO_DIRAC = 0xd1, // ffmpeg/libavformat/mpegts.h
            PSI_STREAM_VIDEO_VC1 = 0xea, // ffmpeg/libavformat/mpegts.h
            PSI_STREAM_VIDEO_SVAC = 0x80, // GBT 25724-2010 SVAC(2014)
            PSI_STREAM_AUDIO_SVAC = 0x9B, // GBT 25724-2010 SVAC(2014)
            PSI_STREAM_AUDIO_G711A = 0x90,  // audio g711 alaw
            PSI_STREAM_AUDIO_G711U = 0x91,  // audio g711 ulaw
            PSI_STREAM_AUDIO_G722 = 0x92,
            PSI_STREAM_AUDIO_G723 = 0x93,
            PSI_STREAM_AUDIO_G729 = 0x99,
        };

        enum VideoCodecType
        {
            VIDEO_CODEC_TYPE_H264 = 1,
            VIDEO_CODEC_TYPE_H265
        };

		class ps_frame
		{
		public:
			ps_frame();
			~ps_frame();

			bool deserialize(const uint8_t* buffer, size_t size);
			std::string serialize();
			void reset();

            bool has_header(uint8_t start_code);
		private:
			size_t deserialize_blocks(const uint8_t* buffer, size_t size);
		public:
			psh2 psh;
			std::vector<ps_block> blocks;
		};



	}
}

