#pragma once

#include <inc/ook/codecs/avdef.h>

int find_framesync_frombits(int streamtype,
	const unsigned char* bits,
	unsigned int bitslen,
	unsigned int bitspos,
	unsigned int misc_mask /* = 0 */);

int find_h264_nal(const unsigned char* bits,
	unsigned int bitslen,
	unsigned int bitspos,
	int* nal_size,
	int chk_integrity,
	int short_sync_hd /* = 0 */);

int h264_sei_analyse(av_frame_s* frm);
