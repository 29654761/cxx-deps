#include <stdio.h>
#include <stdint.h>

#include <ook/predef.h>
#include <ook/printbuf.h>
#include "analyse.h"

typedef struct __GetBitContext 
{
    const unsigned char * buffer;
    const unsigned char * buffer_end;
    int index;
    int size_in_bits;

} __GetBitContext;

#define MIN_CACHE_BITS 25

#ifndef AV_RB32
	#define AV_RB32(x)                             \
	    ((((const unsigned char *)(x))[0] << 24) | \
	     (((const unsigned char *)(x))[1] << 16) | \
	     (((const unsigned char *)(x))[2] <<  8) | \
	      ((const unsigned char *)(x))[3])
#endif

#ifndef NEG_SSR32
	#define NEG_SSR32(a, s) ((( int32_t)(a)) >> (32 - (s)))
#endif

#ifndef NEG_USR32
	#define NEG_USR32(a, s) (((uint32_t)(a)) >> (32 - (s)))
#endif

#define OPEN_READER(name, gb)\
        int name##_index= (gb)->index;\
        int name##_cache= 0;\
		
#define UPDATE_CACHE(name, gb)\
	    name##_cache= AV_RB32( ((const uint8_t *)(gb)->buffer) + (name##_index >> 3) ) << (name##_index & 0x07);\

#define CLOSE_READER(name, gb) (gb)->index= name##_index;

#define GET_CACHE(name, gb) ((uint32_t)name##_cache)
	        
#define SKIP_COUNTER(name, gb, num)   name##_index += (num);
#define LAST_SKIP_BITS(name, gb, num) SKIP_COUNTER(name, gb, num)
	        
#define SHOW_UBITS(name, gb, num)\
        NEG_USR32(name##_cache, num)

#define SHOW_SBITS(name, gb, num)\
        NEG_SSR32(name##_cache, num)

static const uint8_t __log2_tab[256] = 
{
	0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};
static int __log2__(unsigned int v)
{
    int n = 0;
    if (v & 0xffff0000) {
        v >>= 16;
        n += 16;
    }
    if (v & 0xff00) {
        v >>= 8;
        n += 8;
    }
    n += __log2_tab[v];
    return n;
}

#ifdef WIN32
static void print_status(__GetBitContext * s)
{
	printf("buffer_beg: %p\n", s->buffer);
	printf("buffer_end: %p\n", s->buffer_end);
	printf("index     : %d\n", s->index);
	printf("bits_size : %d\n", s->size_in_bits);
}
#endif

static void bs_init_(__GetBitContext * s, unsigned char * p_data, int i_data)
{
    s->buffer       = p_data;
    s->buffer_end   = s->buffer + i_data;
    s->index        = 0;
    s->size_in_bits = i_data;
}

static unsigned int get_bits(__GetBitContext * s, int n)
{
#if defined(__APPLE__) || defined(__ANDROID__)
	int r;
#else
    register int r;
#endif

    OPEN_READER(re, s)
    //int re_index = (s)->index;
    //int re_cache = 0;
        
    UPDATE_CACHE(re, s)
    //re_cache = AV_RB32( (const uint8_t *)(s)->buffer + (re_index >> 3) ) << (re_index & 0x07);
    
    r = SHOW_UBITS(re, s, n);
    LAST_SKIP_BITS(re, s, n)
    CLOSE_READER(re, s)
    return r;
}

static unsigned int get_bits1(__GetBitContext * s)
{
    unsigned int index = s->index;
    uint8_t result = s->buffer[index >> 3];
    ///printf("a[%x]/%d/%d\n", result, (index & 0x07), (index >> 3));
    result <<= (index & 0x07);
    result >>= 8 - 1;    
    ///printf("c[%x]\n", result);
    index++;
    s->index = index;
    return result;
}

static unsigned int get_bits_long(__GetBitContext * s, int n)
{
    if(n <= MIN_CACHE_BITS) 
    {
    	return get_bits(s, n);
    }
    else
    {
        int ret= get_bits(s, 16) << (n - 16);
        return ret | get_bits(s, n - 16);        
    }
}

static const uint8_t __golomb_len[512] = 
{
	14,13,12,12,11,11,11,11,10,10,10,10,10,10,10,10,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
};

static const int8_t __se_golomb_code[512] = 
{
	16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,  8, -8,  9, -9, 10,-10, 11,-11, 12,-12, 13,-13, 14,-14, 15,-15,
	 4,  4,  4,  4, -4, -4, -4, -4,  5,  5,  5,  5, -5, -5, -5, -5,  6,  6,  6,  6, -6, -6, -6, -6,  7,  7,  7,  7, -7, -7, -7, -7,
	 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
	 3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3,
	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static const uint8_t __ue_golomb_code[512] =
{
	31,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,
	 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9,10,10,10,10,11,11,11,11,12,12,12,12,13,13,13,13,14,14,14,14,
	 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int get_ue_golomb(__GetBitContext * s)
{
    unsigned int buf;
    int log;

    OPEN_READER(re, s);
    UPDATE_CACHE(re, s);
    buf = GET_CACHE(re, s);
	///printf("get_ue_golomb::buf=0x%x\n", buf);

    if(buf >= (1 << 27))
    {
        buf >>= 32 - 9;
        LAST_SKIP_BITS(re, s, __golomb_len[buf]);
        CLOSE_READER(re, s);
        return __ue_golomb_code[buf];
    }
    else
    {
        log = 2 * __log2__(buf) - 31;
        buf >>= log;
        buf--;
        LAST_SKIP_BITS(re, s, 32 - log);
        CLOSE_READER(re, s);
        return buf;
    }
}

static int get_ue_golomb_31(__GetBitContext * s)
{
    unsigned int buf;

    OPEN_READER(re, s);
    UPDATE_CACHE(re, s);
    buf = GET_CACHE(re, s);
	///printf("get_ue_golomb_31::buf=0x%x\n", buf);
	
    buf >>= 32 - 9;
    LAST_SKIP_BITS(re, s, __golomb_len[buf]);
    CLOSE_READER(re, s);
    return __ue_golomb_code[buf];
}

static int get_se_golomb(__GetBitContext * s)
{
    int buf;
    int log;

    OPEN_READER(re, s);
    UPDATE_CACHE(re, s);
    buf = GET_CACHE(re, s);

    if(buf >= (1 << 27))
    {
        buf >>= 32 - 9;
        LAST_SKIP_BITS(re, s, __golomb_len[buf]);
        CLOSE_READER(re, s);
        return __se_golomb_code[buf];
    }
    else
    {
        log = 2 * __log2__(buf) - 31;
        buf >>= log;

        LAST_SKIP_BITS(re, s, 32 - log);
        CLOSE_READER(re, s);

        if(buf & 1) 
        	buf = -(buf >> 1);
        else      
        	buf = (buf >> 1);
        return buf;
    }
}

#if 0
static const AVRational pixel_aspect[17] =
{
	{0, 1},
	{1, 1},
	{12, 11},
	{10, 11},
	{16, 11},
	{40, 33},
	{24, 11},
	{20, 11},
	{32, 11},
	{80, 33},
	{18, 11},
	{15, 11},
	{64, 33},
	{160,99},
	{4, 3},
	{3, 2},
	{2, 1},
};
#endif

/////////////////////////////////////////////////
//

#if 1 // for scaling matrices

static const uint8_t zigzag_scan[16 + 1] = 
{
    0 + 0 * 4, 1 + 0 * 4, 0 + 1 * 4, 0 + 2 * 4,
    1 + 1 * 4, 2 + 0 * 4, 3 + 0 * 4, 2 + 1 * 4,
    1 + 2 * 4, 0 + 3 * 4, 1 + 3 * 4, 2 + 2 * 4,
    3 + 1 * 4, 3 + 2 * 4, 2 + 3 * 4, 3 + 3 * 4,
};

static const uint8_t ff_zigzag_direct[64] = 
{
    0,   1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

static int decode_scaling_list(__GetBitContext * s, uint8_t * factors, int size)
{
	int i, last = 8, next = 8;
	const uint8_t * scan = (size == 16) ? zigzag_scan : ff_zigzag_direct;
	///printf("decode_scaling_list\n");
	if(get_bits1(s))
	{
        for(i = 0; i < size; i++) 
        {
            if(next)
                next = (last + get_se_golomb(s)) & 0xff;
            ///printf("next[%d]=%d\n", i, next);
            if(!i && !next) 
            {
                break;
            }
            last = factors[scan[i]] = next ? next : last;
        }
        return 1;		
	}
	return 0;
}

#endif // scaling matrices

int h264_analyse_sps(const unsigned char * bits, 
					 unsigned int bitslen, 
					 h264_sps_info * info,
					 unsigned int printmask)
{
	int i, j, sync_l = 0;
    int b_constraint_set0;
    int b_constraint_set1;
    int b_constraint_set2;
	int b_constraint_set3;

	int b_gaps_in_frame_num_value_allowed;

	__GetBitContext     * s = NULL;
	const unsigned char * p = bits;
	unsigned char * buffer  = NULL;
	
#if 0	
	FILE * fp = fopen("sps.dat", "wb");
	if(fp)
	{
		fwrite(bits, bitslen, 1, fp);
		fclose(fp);
	}
#endif

	__GetBitContext bs;
	
	if(bitslen < 6)
		return -1;
	
	///pbuf(bits, bitslen);
	///printmask = 0x7fff;
	
	if(p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x01)
 		sync_l = 4;
 	else if(p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01)
 		sync_l = 3;
 	
 	if(sync_l < 3)
 		return -2;

	if((p[sync_l] & 0x0F) != 0x07)
		return -3;
	sync_l++;
	
	p       += sync_l;
	bitslen -= sync_l;
	
	buffer = (unsigned char *)malloc(bitslen + 32);
	memset(buffer, 0, bitslen + 32);
	buffer[0] = p[0];
	buffer[1] = p[1];
	for(i = 2, j = 2; i < (int)bitslen; i++)
	{
		///if(p[i] == 0x03 && buffer[j - 2] == 0 && buffer[j - 1] == 0)
		if(p[i] == 0x03 && p[i - 2] == 0 && p[i - 1] == 0) // modify @ 20120226
		{
			if(printmask & 0x04)
				printf("Esp @ %d\n", i);
		}
		else
		{
			buffer[j] = p[i];
			j++;
		}
	}
	
	bs_init_(&bs, (unsigned char *)buffer, j);
	s = &bs;
	p = s->buffer;
	
	if(printmask & 0x02)
		pbuf(s->buffer, s->size_in_bits);

/*
	     0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
	0:  64   0  28  ac  34  c5   1  e0  11  1f  78   a  10  10  10  14
	1:   0   0   0   4   0   0   0   6  50                        			 
 */

    info->profile_idc = get_bits(s, 8);
    if(printmask & 0x01)
    	printf("sps::profile_idc=%d\n", info->profile_idc);
 
	b_constraint_set0 = get_bits1(s);
    b_constraint_set1 = get_bits1(s);
    b_constraint_set2 = get_bits1(s);
    b_constraint_set3 = get_bits1(s);
    get_bits(s, 4);    // reserved

	OOK_UNUSED(b_constraint_set0)
	OOK_UNUSED(b_constraint_set1)
	OOK_UNUSED(b_constraint_set2)
	OOK_UNUSED(b_constraint_set3)
	
    info->level_idc   = get_bits(s, 8);
    if(printmask & 0x01)
		printf("sps::level_idc=%d\n", info->level_idc);

	info->sps_id      = get_ue_golomb_31(s);
	if(printmask & 0x01)
    	printf("sps::sps_id=%d\n", info->sps_id);

	if(info->sps_id >= 32)
    {
        // the sps is invalid, no need to corrupt sps_array[0]
        free(buffer);
        return -4;
    }

	if(info->profile_idc >= 100) // PROFILE_HIGH
	{
        info->chroma_format_idc = get_ue_golomb_31(s); // chroma_format_idc = 4:2:0
		if(info->chroma_format_idc == 3)
			get_bits1(s);
		info->bit_depth_luma   = get_ue_golomb(s) + 8; // bit_depth_luma_minus8
        info->bit_depth_chroma = get_ue_golomb(s) + 8; // bit_depth_chroma_minus8
        info->transform_bypass = get_bits1(s);
        if(get_bits1(s))
        {
        	if(printmask & 0x01)
        		printf("sps::scaling matrices present\n");
        	// add @ 2016/12/27
			{
				uint8_t scaling_matrix4[6][16];
    			uint8_t scaling_matrix8[6][64];
    			
			    memset(scaling_matrix4, 16, sizeof(scaling_matrix4));
			    memset(scaling_matrix8, 16, sizeof(scaling_matrix8));    			

				decode_scaling_list(s, scaling_matrix4[0], 16); 		// Intra, Y
				decode_scaling_list(s, scaling_matrix4[1], 16);			// Intra, Cr
				decode_scaling_list(s, scaling_matrix4[2], 16);			// Intra, Cb
				decode_scaling_list(s, scaling_matrix4[3], 16);			// Inter, Y
				decode_scaling_list(s, scaling_matrix4[4], 16);			// Inter, Cr
				decode_scaling_list(s, scaling_matrix4[5], 16);			// Inter, Cb
				{
					decode_scaling_list(s, scaling_matrix8[0], 64);		// Intra, Y
					decode_scaling_list(s, scaling_matrix8[3], 64);		// Inter, Y
					
					if(info->chroma_format_idc == 3)
					{
						decode_scaling_list(s, scaling_matrix8[1], 64);	// Intra, Cr
						decode_scaling_list(s, scaling_matrix8[4], 64);	// Inter, Cr
						decode_scaling_list(s, scaling_matrix8[2], 64);	// Intra, Cb
						decode_scaling_list(s, scaling_matrix8[5], 64);	// Inter, Cb
					}
				}
		        ///free(buffer);
		        ///return -5;				
			}
        }
	}
	else
	{
		info->chroma_format_idc = 1;
		info->bit_depth_luma    = 8;
		info->bit_depth_chroma  = 8;
		info->transform_bypass  = 0;
	}
	if(printmask & 0x01)
	{
		printf("sps::chroma_format_idc =%d\n", info->chroma_format_idc);
		printf("sps::bit_depth_luma    =%d\n", info->bit_depth_luma);
		printf("sps::bit_depth_chroma  =%d\n", info->bit_depth_chroma);
		printf("sps::transform_bypass  =%d\n", info->transform_bypass);
	}
	
    info->log2_max_frame_num = get_ue_golomb(s) + 4;
    if(printmask & 0x01)
		printf("sps::log2_max_frame_num=%d\n", info->log2_max_frame_num);

    info->poc_type = get_ue_golomb_31(s);
    if(printmask & 0x01)
		printf("sps::poc_type=%d\n", info->poc_type);

    if(info->poc_type == 0)
    {
        info->log2_max_poc_lsb = get_ue_golomb(s) + 4;
        if(printmask & 0x01)
        	printf("sps::log2_max_poc_lsb  =%d\n", info->log2_max_poc_lsb);
    }
    else if(info->poc_type == 1)
    {
		int b_delta_pic_order_always_zero;
        int i_offset_for_non_ref_pic;
        int i_offset_for_top_to_bottom_field;
        int i_num_ref_frames_in_poc_cycle;
		///int i_offset_for_ref_frame [256];
		
		OOK_UNUSED(b_delta_pic_order_always_zero)
		OOK_UNUSED(i_offset_for_non_ref_pic)
		OOK_UNUSED(i_offset_for_top_to_bottom_field)
		OOK_UNUSED(i_num_ref_frames_in_poc_cycle)
		
        b_delta_pic_order_always_zero    = get_bits1(s);
        i_offset_for_non_ref_pic         = get_se_golomb(s);
        i_offset_for_top_to_bottom_field = get_se_golomb(s);
        i_num_ref_frames_in_poc_cycle    = get_se_golomb(s);
        if(i_num_ref_frames_in_poc_cycle > 256)
        {
            // FIXME what to do
            i_num_ref_frames_in_poc_cycle = 256;
        }

        for(i = 0; i < i_num_ref_frames_in_poc_cycle; i++)
        {
            ///i_offset_for_ref_frame[i] = get_se_golomb(s);
        }
    }
    else if(info->poc_type > 2)
    {
    	printf("sps::unpporting poc_type[%d]\n", info->poc_type);
    	free(buffer);
        return -6;
    }
    
    info->ref_frame_count = get_ue_golomb_31(s);
    if(printmask & 0x01)
    	printf("sps::num_ref_frames    =%d\n", info->ref_frame_count);
    
    b_gaps_in_frame_num_value_allowed = get_bits1(s);
    if(printmask & 0x01)
    	printf("sps::gaps_in_frame_num_value_allowed=%d\n", b_gaps_in_frame_num_value_allowed);

	info->mb_width  = get_ue_golomb(s) + 1;
    info->mb_height = get_ue_golomb(s) + 1;
    if(printmask & 0x01)
		printf("sps::mb_width=%d, mb_height=%d\n", info->mb_width, info->mb_height);

	///printf("index=%d\n", s->index);
	///pbuf(s->buffer + (s->index >> 3), s->buffer_end - s->buffer - (s->index >> 3));
	
	info->frame_mbs_only_flag = get_bits1(s);
    if(!info->frame_mbs_only_flag)
        info->mb_aff = get_bits1(s);
	else
	    info->mb_aff = 0;
	if(printmask & 0x01)
		printf("sps::frame_mbs_only_flag=%d, mb_aff=%d\n", info->frame_mbs_only_flag, info->mb_aff);

	info->width  = info->mb_width  * 16;
	info->height = (info->mb_height * 16) * (2 - info->frame_mbs_only_flag); // FIXED @ 2019/03/25
	if(printmask & 0x01)
		printf("sps::width=%d, height=%d\n", info->width, info->height);
		   	
	info->direct_8x8_inference_flag = get_bits1(s);
	if(printmask & 0x01)
		printf("sps::direct_8x8_inference_flag=%d\n", info->direct_8x8_inference_flag);

	info->crop = get_bits1(s);
	if(info->crop)
	{
		info->crop_l = get_ue_golomb(s);
		info->crop_r = get_ue_golomb(s);
		info->crop_t = get_ue_golomb(s);
		info->crop_b = get_ue_golomb(s);
		
        int vsub   = (info->chroma_format_idc == 1) ? 1 : 0;
        int hsub   = (info->chroma_format_idc == 1 || info->chroma_format_idc == 2) ? 1 : 0;
        int step_x = 1 << hsub;
        int step_y = (2 - info->frame_mbs_only_flag) << vsub;

        if(info->crop_l > 0x7fffffff / 4 / step_x ||
           info->crop_r > 0x7fffffff / 4 / step_x ||
           info->crop_t > 0x7fffffff / 4 / step_y ||
           info->crop_b > 0x7fffffff / 4 / step_y ||
           (info->crop_l + info->crop_r) * step_x >= info->width ||
           (info->crop_t + info->crop_b) * step_y >= info->height) 
        {
			info->crop_l = 0;
			info->crop_r = 0;
			info->crop_t = 0;
			info->crop_b = 0;
        }
        else
        {
	        info->crop_l = info->crop_l * step_x;
	        info->crop_r = info->crop_r * step_x;
	        info->crop_t = info->crop_t * step_y;
	        info->crop_b = info->crop_b * step_y;
        }
               		
		if(printmask & 0x01)
			printf("sps::crop=%d/%d/%d/%d\n", info->crop_l, info->crop_r, info->crop_t, info->crop_b);
	}
	
	info->vui_parameters_present_flag = get_bits1(s);
	if(printmask & 0x01)
		printf("sps::vui=%d\n", info->vui_parameters_present_flag);

	if(info->vui_parameters_present_flag > 0)
	{
		int aspect_ratio_info_present_flag;
		unsigned int aspect_ratio_idc;
		
		if(printmask & 0x02)
		{
			printf("sps::index=%d\n", s->index);
			pbuf(s->buffer + (s->index >> 3), (int)(s->buffer_end - s->buffer - (s->index >> 3)));
		}
		
		aspect_ratio_info_present_flag = get_bits1(s);
		if(printmask & 0x01)
			printf("sps::vui::aspect_ratio_info_present_flag =%d\n", aspect_ratio_info_present_flag);
		if(aspect_ratio_info_present_flag)
		{
			aspect_ratio_idc = get_bits(s, 8);
			if(printmask & 0x01)
				printf("sps::vui::aspect_ratio_idc=%u\n", aspect_ratio_idc);
			if(aspect_ratio_idc == 255)
			{
				int num = get_bits(s, 16);
				int den = get_bits(s, 16);
				if(printmask & 0x01)
					printf("sps::vui::aspect_ratio_idc=%u/%u\n", num, den);				
			}
			else
			{
			}
		}

    	if(get_bits1(s))   /* overscan_info_present_flag */
    	{
    		if(printmask & 0x01)
    			printf("overscan_info_present_flag\n");
        	get_bits1(s);  /* overscan_appropriate_flag  */
        }

		info->vui.video_signal_type_present_flag = get_bits1(s);
		if(printmask & 0x01)
			printf("sps::vui::video_signal_type_present_flag =%d\n", info->vui.video_signal_type_present_flag);
		if(info->vui.video_signal_type_present_flag)
		{
        	get_bits(s, 3);    			   /* video_format */
        	/* int full_range = */ get_bits1(s); /* video_full_range_flag */
        	int colour_description_present_flag = get_bits1(s);
        	if(printmask & 0x01)
				printf("sps::vui::colour_description_present_flag=%d\n", colour_description_present_flag);
			if(colour_description_present_flag)
			{
				int color_primaries = get_bits(s, 8); /* colour_primaries */
            	int color_trc       = get_bits(s, 8); /* transfer_characteristics */
            	int colorspace      = get_bits(s, 8); /* matrix_coefficients */
            	if(printmask & 0x01)
            		printf("sps::vui::color=%d/%d/%d\n", color_primaries, color_trc, colorspace);
			}
		}

		if(get_bits1(s))	/* chroma_location_info_present_flag */
		{      
			if(printmask & 0x01)
				printf("chroma_location_info_present_flag\n");
		    int chroma_sample_location = get_ue_golomb(s) + 1;  /* chroma_sample_location_type_top_field */
			if(printmask & 0x01)
				printf("sps::vui::chroma_sample_location=%d\n", chroma_sample_location);    
		    get_ue_golomb(s);  	/* chroma_sample_location_type_bottom_field */
		}


		info->timing_info_present_flag = get_bits1(s);
		if(printmask & 0x01)
			printf("sps::vui::timing_info_present_flag=%d\n", info->timing_info_present_flag);
		if(info->timing_info_present_flag)
		{
			if(printmask & 0x02)
			{
				int pos = (s->index % 8);
				printf("index=%d, pos=%d\n", s->index, pos);
				pbuf(s->buffer + (s->index >> 3), (int)(s->buffer_end - s->buffer - (s->index >> 3)));
			}
		/*
			index=126
			14        0         0         0         4         0         0         0         ca        50   0   0   0   0   0   0		
			0001,0100 0000,0000 0000,0000 0000,0000 0000,0100 0000,0000 0000,0000 0000,0000 1100,1010
			       [         |         |         |        ][         |         |         |        ]
			                                           0x01                                    0x32
                                                                                            52
			0001,0100 0000,0000 0000,0000 0000,0000 0000,0100 0000,0000 0000,0000 0000,0000 0101,0010
			                                                                                   0x14
			                                                                                   
			index=76 pos=4
			10        0         0         0         10        0         0         3         28        f1
			0001,0000 0000,0000 0000,0000 0000,0000 0001,0000 0000,0000 0000,0000 0000,0011 0010,1000 1111,0001
			     [                                     ] [                                     ]
			                                        0x01                                    0x32
		 */
			info->num_units_in_tick     = get_bits_long(s, 32);
			info->time_scale            = get_bits_long(s, 32);
			info->fixed_frame_rate_flag = get_bits1(s);
			if(printmask & 0x01)
				printf("sps::vui::time_scale=%u, num_units_in_tick=%u, fixed_frame_rate_flag=%d\n", info->time_scale, info->num_units_in_tick, info->fixed_frame_rate_flag);
			
			if(info->num_units_in_tick > 0)
			{
				unsigned int frms = (info->time_scale / info->num_units_in_tick) >> 1; // modify @ 2016/01/16
				if(frms > 0)
				{
					unsigned int intv = 1000 / frms;
					if(printmask & 0x01)
						printf("sps::vui::intv=%u\n", intv);
				}
			}
		}
	}
	free(buffer);
    return info->sps_id;
}

int h264_analyse_pps(const unsigned char * bits, 
					 unsigned int bitslen, 
					 h264_pps_info * info, 
					 unsigned int printmask)
{
	int i, j;

	__GetBitContext     * s = NULL;
	const unsigned char * p = bits;
	unsigned char * buffer  = NULL;
	
	__GetBitContext bs;
	
	if(bitslen < 6)
		return -1;
	
 	if(p[0] != 0x00 || p[1] != 0x00 || p[2] != 0x00 || p[3] != 0x01)
 		return -2;

	if((p[4] & 0x0F) != 0x08)
		return -3;
	
	p       += 5;
	bitslen -= 5;
	
	buffer = (unsigned char *)malloc(bitslen + 32);
	memset(buffer, 0, bitslen + 32);
	buffer[0] = p[0];
	buffer[1] = p[1];
	for(i = 2, j = 2; i < (int)bitslen; i++)
	{
		if(p[i] == 0x03 && p[i - 2] == 0 && p[i - 1] == 0) // modify @ 20120226
		{
			if(printmask & 0x04)
				printf("Esp @ %d\n", i);
		}
		else
		{
			buffer[j] = p[i];
			j++;
		}
	}
	
	bs_init_(&bs, (unsigned char *)buffer, j);
	s = &bs;
	p = s->buffer;
	
	if(printmask & 0x02)
		pbuf(s->buffer, s->size_in_bits);
/*
	eb  e1  52  c8
 */
	
	info->id = get_ue_golomb_31(s);
	if(printmask & 0x01)
    	printf("pps::id        =%d, left=%d\n", info->id, s->size_in_bits);
    
	info->sps_id = get_ue_golomb_31(s);
	if(printmask & 0x01)
    	printf("pps::sps_id    =%d\n", info->sps_id);
    	
	info->cabac     = get_bits1(s);
	info->pic_order = get_bits1(s);
	if(printmask & 0x01)
	{
    	printf("pps::cabac     =%d\n", info->cabac);	
    	printf("pps::pic_order =%d\n", info->pic_order);
    }
    
    info->num_slice_groups              = get_ue_golomb_31(s) + 1;
    info->num_ref_idx_l0_default_active = get_ue_golomb_31(s) + 1;
    info->num_ref_idx_l1_default_active = get_ue_golomb_31(s) + 1;
    if(printmask & 0x01)
    {	
    	printf("pps::num_slice_groups=%d\n", info->num_slice_groups);	
    	printf("pps::num_ref_idx_l0  =%d\n", info->num_ref_idx_l0_default_active);
    	printf("pps::num_ref_idx_l1  =%d\n", info->num_ref_idx_l1_default_active);
    }
    
    info->weighted_pred   = get_bits(s, 1);
    info->weighted_bipred = get_bits(s, 2);
    info->pic_init_qp     = get_se_golomb(s) + 26;
    info->pic_init_qs     = get_se_golomb(s) + 26;
    if(printmask & 0x01)
    {
    	printf("pps::weighted_pred   =%d\n", info->weighted_pred);
    	printf("pps::weighted_bipred =%d\n", info->weighted_bipred);
    	printf("pps::pic_init_qp     =%d\n", info->pic_init_qp);
    	printf("pps::pic_init_qs     =%d\n", info->pic_init_qs);
    }
    
    info->chroma_qp_index_offset    = get_se_golomb(s);
	info->deblocking_filter_control = get_bits1(s);
	info->constrained_intra_pred    = get_bits1(s);
	info->redundant_pic_cnt         = get_bits1(s);
    if(printmask & 0x01)
    {
    	printf("pps::chroma_qp_index_offset   =%d\n", info->chroma_qp_index_offset);    	
    	printf("pps::deblocking_filter_control=%d\n", info->deblocking_filter_control);
    	printf("pps::constrained_intra_pred   =%d\n", info->constrained_intra_pred);
    	printf("pps::redundant_pic_cnt        =%d\n", info->redundant_pic_cnt);    	
    }
    
	free(buffer);
	return info->id;
}

#define _I_TYPE  1 /// Intra
#define _P_TYPE  2 /// Predicted
#define _B_TYPE  3 /// Bi-dir predicted
#define _S_TYPE  4 /// S(GMC)-VOP MPEG4
#define _SI_TYPE 5 /// Switching Intra
#define _SP_TYPE 6 /// Switching Predicted
#define _BI_TYPE 7

static const uint8_t __golomb_to_pict_type[5] =
{
	_P_TYPE, _B_TYPE, _I_TYPE, _SP_TYPE, _SI_TYPE
};

int h264_analyse_slice(const unsigned char * bits,
					   unsigned int bitslen,
					   h264_slice_info * info,
					   h264_sps_info   * sps,
					   unsigned int printmask)
{
	__GetBitContext * s = NULL;
	__GetBitContext bs;
	unsigned char nal_type = bits[0] & 0x1f;

	bs_init_(&bs, (unsigned char *)bits + 1, bitslen - 1);
	s = &bs;

	///pbuf(bits, 32);
	
	if(printmask & 0x01)
		printf("\nslice::nal_type=%x\n", nal_type);

	// first_mb_in_slice				
	info->first_mb_in_slice = get_ue_golomb(s);
	if(printmask & 0x01)
		printf("slice::first_mb_in_slice=%d\n", info->first_mb_in_slice);

	// slice_type
	info->slice_type = get_ue_golomb_31(s);
	if(printmask & 0x01)
		printf("slice::slice_type=%d\n", info->slice_type);
	if(info->slice_type > 9)
		return -2;
    if(info->slice_type > 4)
       info->slice_type -= 5;
    info->slice_type = __golomb_to_pict_type[info->slice_type];
    
	// pps_id
	info->pps_id = get_ue_golomb(s);
	if(printmask & 0x01)
		printf("slice::pps_id=%d\n", info->pps_id);
	
	if(!sps)
		return 0;
	
	if(sps->log2_max_frame_num < 1)
		return -1;

	info->frame_num = get_bits(s, sps->log2_max_frame_num);
	if(printmask & 0x01)
		printf("slice::frame_num=%d\n", info->frame_num);

	if(sps->frame_mbs_only_flag > 0)
	{
	}
	else
	{
		info->field_pic = get_bits1(s);
		if(printmask & 0x01)
			printf("slice::field_pic=%d\n", info->field_pic);
		if(info->field_pic > 0)
		{
			info->bottom_field = get_bits1(s);
			if(printmask & 0x01)
				printf("slice::bottom_field=%d\n", info->bottom_field);
		}
		else
		{
			info->bottom_field = 0;
		}
	}
		
    if(nal_type == 0x05 /* NAL_IDR_SLICE */)
        get_ue_golomb(s); /* idr_pic_id */
    	
	if(sps->poc_type == 0)
	{
		info->poc_lsb = get_bits(s, sps->log2_max_poc_lsb);
		if(printmask & 0x01)
			printf("slice::poc_lsb=%d\n", info->poc_lsb);
	}
	
	return 0;
}

#if 0
int h264_set_frame_num(const unsigned char * bits, 
					   unsigned int bitslen,
					   h264_slice_info * info,
					   unsigned int printmask)
{
	int pos;
	unsigned int frame_numb;
	unsigned char nal_type = bits[0] & 0x1f;
	unsigned char * ptr = NULL;
	__GetBitContext * s = NULL;
	__GetBitContext bs;
	
	if(info->log2_max_frame_num < 1)
		return -1;

	bs_init_(&bs, (unsigned char *)bits + 1, bitslen - 1);
	s = &bs;

	if(printmask & 0x01)
		printf("\nslice::nal_type=%x\n", nal_type);
	
	if(printmask & 0x02)
	{
		printf("index=%d", s->index);
		pbuf(s->buffer + (s->index >> 3), s->buffer_end - s->buffer - (s->index >> 3));
	}
		
	// first_mb_in_slice				
	info->first_mb_in_slice = get_ue_golomb(s);
	if(printmask & 0x01)
		printf("slice::first_mb_in_slice=%d\n", info->first_mb_in_slice);

	// slice_type
	info->slice_type = get_ue_golomb_31(s);
	if(printmask & 0x01)
		printf("slice::slice_type=%d\n", info->slice_type);
	if(info->slice_type > 9)
		return -2;
    
	// pps_id
	info->pps_id = get_ue_golomb(s);
	if(printmask & 0x01)
		printf("slice::pps_id=%d\n", info->pps_id);

	pos = (s->index % 8);
	ptr = (unsigned char *)s->buffer + (s->index >> 3);
	if(printmask & 0x02)
	{
		printf("index=%d, pos=%d\n", s->index, pos);
		pbuf(s->buffer + (s->index >> 3), s->buffer_end - s->buffer - (s->index >> 3));
	}
		
	/*
	    log2_max_frame_num = 9
	    
	IN:	b9  	  d2        5b
		1011,1001 1101,0010
		      +-- ---- --
		   frame_num=0x074=116
		   
	  u:02        40
		0000,0010 0100,0000 
		
		|
			   
	OT:	ba        42        5b
		1011,1010 0100,0010
		      +-- ---- --
		   frame_num=0x90=144
	 */
	frame_numb = get_bits(s, info->log2_max_frame_num);
	if(printmask & 0x01)
		printf("slice::frame_num=%d\n", frame_numb);

	if(info->frame_num > -1)
	{
		///int bexit = (frame_numb == 116);
		
		unsigned char u[8];
		unsigned int m, frame_mask = 0;
		int i, j, n;
		
		// how much bytes will be modfied
		n = (pos + info->log2_max_frame_num) >> 3;
		if((n << 3) < (pos + info->log2_max_frame_num))
			n++;
		if(n > 8)
			return -3;
				
		// mask the frame_num
		for(i = 0; i < info->log2_max_frame_num; i++)
			 frame_mask += (1 << i);
		frame_numb = info->frame_num % frame_mask; 
		///printf("frame_numb=%d, frame_mask=%x, n=%d\n", frame_numb, frame_mask, n);
		
		// construct new frame_num buffer
		memset(u, 0, 8);
		m = 1 << (8 - pos - 1);
		for(i = info->log2_max_frame_num; i > 0; i--)
		{
			j = pos >> 3;
			ptr[j] &= ~(1 << (7 - (pos % 8)));
			///printf("[%d/%d/%d/%x]\n", i, j, pos, m);
			if((frame_numb & (1 << (i - 1))))
			{
				u[j] |= m;
				///pbuf(u, n);
			}
			pos++;
			if((pos % 8) == 0)
				m = 0x80;
			else
				m >>= 1;
		}
		///pbuf(ptr, n);
		///pbuf(u, n);
			
		// OR with the input bits
		for(i = 0; i < n; i++)
			ptr[i] |= u[i];
		///pbuf(ptr, n);
		
		///if(bexit)
		///	exit(0);
	}
	
	info->field_pic = get_bits1(s);
	if(printmask & 0x01)
		printf("slice::field_pic=%d\n", info->field_pic);
	if(info->field_pic > 0)
	{
		info->bottom_field = get_bits1(s);
		if(printmask & 0x01)
			printf("slice::bottom_field=%d\n", info->bottom_field);
	}
	else
	{
		info->bottom_field = 0;
	}
	
    if(nal_type == 0x05 /* NAL_IDR_SLICE */)
        get_ue_golomb(s); /* idr_pic_id */
    	
	if(info->poc_type == 0)
	{
		if(info->poc_lsb > -1)
		{
			unsigned char u[8];
			unsigned int m, frame_mask = 0;
			int i, j, n;
			
			pos = (s->index % 8);
			ptr = (unsigned char *)s->buffer + (s->index >> 3);
			if(printmask & 0x02)
			{
				printf("index=%d, pos=%d\n", s->index, pos);
				pbuf(s->buffer + (s->index >> 3), s->buffer_end - s->buffer - (s->index >> 3));
			}
					
			// how much bytes will be modfied
			n = (pos + info->log2_max_poc_lsb) >> 3;
			if((n << 3) < (pos + info->log2_max_poc_lsb))
				n++;
			if(n > 8)
				return -3;
					
			// mask the frame_num
			for(i = 0; i < info->log2_max_poc_lsb; i++)
				 frame_mask += (1 << i);
			info->poc_lsb = info->poc_lsb % frame_mask; 
			
			// construct new frame_num buffer
			memset(u, 0, 8);
			m = 1 << (8 - pos - 1);
			for(i = info->log2_max_poc_lsb; i > 0; i--)
			{
				j = pos >> 3;
				ptr[j] &= ~(1 << (7 - (pos % 8)));
				///printf("[%d/%d/%d/%x]\n", i, j, pos, m);
				if((info->poc_lsb & (1 << (i - 1))))
				{
					u[j] |= m;
					///pbuf(u, n);
				}
				pos++;
				if((pos % 8) == 0)
					m = 0x80;
				else
					m >>= 1;
			}
			///pbuf(ptr, n);
			///pbuf(u, n);
				
			// OR with the input bits
			for(i = 0; i < n; i++)
				ptr[i] |= u[i];
			///pbuf(ptr, n);
		}
	}	
	return 0;	
}
#endif
