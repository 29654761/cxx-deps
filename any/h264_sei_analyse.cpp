#include "streamdef.h"
#include "h264_sei_analyse.h"

#define ANYLIVE_MAGIC 0x414C4555

int find_framesync_frombits(int streamtype, 
						    const unsigned char * bits, 
						    unsigned int bitslen,
						    unsigned int bitspos,
						    unsigned int misc_mask /* = 0 */)
{
	int r = -1;
	const unsigned char * p = bits + bitspos;
	///TRACE(3, "scan::bitslen=" << bitslen)
	///pbuf(p, 16);
	if(streamtype == STREAM_TYPE_VIDEO_H264 ||
	   streamtype == STREAM_TYPE_VIDEO_HEVC)
	{
		unsigned int l     = bitslen;
		unsigned int pos   = 0;
		unsigned int shift = 0;
		while(l > 0)
		{
			shift = ((shift << 8) | *p);
			pos++;
			if(pos > 3)
			{
				if(shift == 0x01)
				{
					r = (int)bitspos + pos - 4;
					break;
				}
				else if((misc_mask & 0x01) && (shift & 0xffffff00) == 0x0100) // short sync head
				{
					r = (int)bitspos + pos - 4;
					break;
				}
			}
			l--;
			p++;
		}
	}
	else if(streamtype == STREAM_TYPE_VIDEO_H263)
	{
		unsigned int l     = bitslen;
		unsigned int pos   = 0;
		unsigned int shift = 0;
		while(l > 0)
		{
			shift = ((shift << 8) | *p);
			pos++;
			if(pos > 3 && (shift & 0xfffffc00) == 0x8000)
			{
				r = (int)bitspos + pos - 4;
				break;
			}
			l--;
			p++;
		}
	}
	else if(streamtype == STREAM_TYPE_VIDEO_MPEG4)
	{
		unsigned int l     = bitslen;
		unsigned int pos   = 0;
		unsigned int shift = 0;
		while(l > 0)
		{
			shift = ((shift << 8) | *p);
			pos++;
			if(pos > 3 && (shift & 0xffffff00) == 0x0100)
			{
				r = (int)bitspos + pos - 4;
				break;
			}
			l--;
			p++;
		}
	}
	else if(streamtype == STREAM_TYPE_VIDEO_MPEG2)
	{
		unsigned int l     = bitslen;
		unsigned int pos   = 0;
		unsigned int shift = 0;
		while(l > 0)
		{
			shift = ((shift << 8) | *p);
			pos++;
			///printf("[%x]", shift);
			if(pos > 3 && (shift == 0x01b3 || shift == 0x01b8 || shift == 0x0100))
			{
				r = (int)bitspos + pos - 4;
				break;
			}
			l--;
			p++;
		}
	}
	return r;	
}

int find_h264_nal(const unsigned char * bits, 
				  unsigned int bitslen, 
				  unsigned int bitspos,
				  int * nal_size,
				  int chk_integrity,
				  int short_sync_hd /* = 0 */)
{
	int i, lastpos = -1, nalsize = 0, sizebak = (int)bitslen, pos_in = bitspos;
	unsigned int shift, misc_mask = short_sync_hd > 0 ? 0x01 : 0;
	///printf("h264::nal::len=%u @ %u\n", bitslen, bitspos);
	for(i = 0; i < 2; i++)
	{
		///printf("[%u, %u]\n", bitspos, bitslen);
		///printf("%u <\n", bitslen);
		int pos = find_framesync_frombits(STREAM_TYPE_VIDEO_H264, bits, bitslen, bitspos, misc_mask);
		///printf("> %d\n", pos);
		if(pos < (int)bitspos)
		{
			if(i == 1 && chk_integrity == 0)
			{
				///printf("lastpos=%d, pos_in=%d, sizebak=%d\n", lastpos, pos_in, sizebak);
				if(lastpos > -1)
				{
					if(nal_size && lastpos >= pos_in) // FIXED @ 2017/10/11
					{
						int d = lastpos - pos_in;
						if(sizebak >= d)
						{
							*nal_size = sizebak - d;
							///printf("nal_size=%d\n", *nal_size);
						}
					}
				}
				return lastpos;
			}			
			return -1;
		}
		if(i == 0)
		{
			lastpos = pos;
			pos    += 4;
			shift   = (unsigned int)pos - bitspos;
			if(bitslen < shift)
				return -2;
			bitslen -= shift;
			bitspos  = (unsigned int)pos;
			continue;
		}
		nalsize = pos - lastpos;
	}
	if(nal_size)
		*nal_size = nalsize;
	return lastpos;
}

int h264_sei_analyse(av_frame_s * frm)
{
	int sei_label = -1;
	
	const unsigned char * bits = frm->bits + frm->bitspos;
	unsigned int bitslen = frm->bitslen;
		
	//
	// SPS/SEI check
	//
	{
		unsigned int bitslen_in = bitslen;
		unsigned int bitspos = 0;
		while(bitslen > 3)
		{
			int nalsize = 0;
			int nal_pos = find_h264_nal(bits, bitslen, bitspos, &nalsize, 0, 1);
			if(nal_pos < (int)bitspos || nalsize < 4)
				break;
							
			//
			// got the NAL
			//
			const unsigned char * nal = bits + nal_pos; // point to nal buffer
			int pos = 4;
			if(nal[0] == 0 && nal[1] == 0 && nal[2] == 1) // short header
				pos = 3;
			unsigned char nal_type = nal[pos] & 0x1f;
			
		#if 0
			printf("nal[%u]=%6d (%02x %02x %02x %02x %02x %02x %02x %02x) %10u", nal_type, nalsize, nal[0], nal[1], nal[2], nal[3], nal[4], nal[5], nal[6], nal[7], stamp);
		#endif
		
			if(nal_type == 0x06)
			{
				//
				// check SEI
				//
				///TRACE(2, "lplayer::SEI=" << nalsize)
				if(nalsize >= 32)
				{
				/*
				                   |type
				                      |size
					00 00 00 01 06 05 18
					|guid
					58  8a  a9  d0  14  67  f8  48   5  21  73  28  45  f2  88  99
					|fourcc
					55 45 4c 41 xx  xx  xx  xx  80
				 */
					const unsigned char * p = nal + 5;
					if(*p == 0x05)
					{
						int sei_size = (int)(unsigned int)p[1];
						if(sei_size > 23)
						{
							p += 2;
							p += 16;
						#if 0
							{
								printf("SEI=(%02x %02x %02x %02x %02x %02x %02x %02x)", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
							}
						#endif
				
							if(*(int *)p == ANYLIVE_MAGIC) // 0x414C4555 (ook/apps/anylive/ex_def.h)
							{
								p += 4;
								sei_label = *(int *)p;

								///printf("-- lplayer::label[%u][%d]=%x\n", lkmicId, track, sei_label);
							}
						}
					}
				}
			}
			
			bitslen -= (unsigned int)nalsize;
			bitspos  = (unsigned int)(nal_pos + nalsize);				
		}
		bitslen = bitslen_in;
	}
	return sei_label;
}
