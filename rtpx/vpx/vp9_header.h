/**
 * @file vp9_header.h
 * @brief
 * @author Shijie Zhou
 * @copyright 2024 Shijie Zhou
 */



#pragma once

#include <stdint.h>
#include <vector>


/// <summary>
///       +-+-+-+-+-+-+-+-+
///    V: | N_S |Y|G|-|-|-|
///       +-+-+-+-+-+-+-+-+                 -\
///    Y: |     WIDTH     | (OPTIONAL)       .
///       +               +                  .
///       |               | (OPTIONAL)       .
///       +-+-+-+-+-+-+-+-+                  . - N_S + 1 times
///       |     HEIGHT    | (OPTIONAL)       .
///       +               +                  .
///       |               | (OPTIONAL)       .
///       +-+-+-+-+-+-+-+-+                 -/
///    G: |      N_G      | (OPTIONAL)
///       +-+-+-+-+-+-+-+-+                           -\
///  N_G: | TID |U| R |-|-| (OPTIONAL)                 .
///       +-+-+-+-+-+-+-+-+              -\            . - N_G times
///       |    P_DIFF     | (OPTIONAL)    . - R times  .
///       +-+-+-+-+-+-+-+-+              -/           -/
/// </summary>

typedef struct _vp9_resolution
{
	uint16_t width;
	uint16_t height;
}vp9_resolution;


class vp9_scalability_pg
{
public:
	vp9_scalability_pg();
	~vp9_scalability_pg();

	int serialize(uint8_t* buffer, int offset, int size)const;
	int deserialize(const uint8_t* buffer, int offset, int size);
public:
	uint8_t tid : 3;
	uint8_t u : 1;
	std::vector<uint8_t> p_diff;
};



class vp9_scalability_header
{
public:
	vp9_scalability_header();
	~vp9_scalability_header();

	int serialize(uint8_t* buffer,int offset, int size)const;
	int deserialize(const uint8_t* buffer, int offset, int size);
public:
	uint8_t ns : 3; //res.size()-1
	uint8_t y : 1;
	uint8_t g : 1;
	std::vector<vp9_resolution> res;
	std::vector<vp9_scalability_pg> pg;
};









/// <summary>
/// rfc9628
/// 
///      0 1 2 3 4 5 6 7
///     +-+-+-+-+-+-+-+-+
///     |I|P|L|F|B|E|V|Z| (REQUIRED)
///     +-+-+-+-+-+-+-+-+
///   I:| M |    PID    | (REQUIRED)
///     +-+-+-+-+-+-+-+-+
///   M:| EXTENDED PID  | (RECOMMENDED)
///     +-+-+-+-+-+-+-+-+
///   L:| TID |U| SID |D| (Conditionally RECOMMENDED)
///     +-+-+-+-+-+-+-+-+                             -\
/// P,F:|    P_DIFF   |N| (Conditionally REQUIRED)    - up to 3 times
///     +-+-+-+-+-+-+-+-+                             -/
///   V:|      SS       |
///     |      ..       |
///     +-+-+-+-+-+-+-+-+
/// 
/// 
///      0 1 2 3 4 5 6 7
///     +-+-+-+-+-+-+-+-+
///     |I|P|L|F|B|E|V|Z| (REQUIRED)
///     +-+-+-+-+-+-+-+-+
///   I:| M |    PID    | (RECOMMENDED)
///     +-+-+-+-+-+-+-+-+
///   M:| EXTENDED PID  | (RECOMMENDED)
///     +-+-+-+-+-+-+-+-+
///   L:| TID |U| SID |D| (Conditionally RECOMMENDED)
///     +-+-+-+-+-+-+-+-+
/// P,F:|   TL0PICIDX   | (Conditionally REQUIRED)
///     +-+-+-+-+-+-+-+-+
///   V:|      SS       |
///     |      ..       |
///     +-+-+-+-+-+-+-+-+
/// 
/// </summary>

class vp9_header
{
public:
	vp9_header();
	~vp9_header();

	int serialize(uint8_t* buffer, int offset, int size)const;
	int deserialize(const uint8_t* buffer, int offset, int size);

	static bool is_keyframe(const uint8_t* buffer, size_t size);
public:
	uint8_t i : 1; // Picture ID present
	uint8_t p : 1; // inter-picture predicted
	uint8_t l : 1; // layer indices present
	uint8_t f : 1; // flexible mode
	uint8_t b : 1; // begin of frame
	uint8_t e : 1; // end of frame
	uint8_t v : 1; // scalability structure present
	uint8_t z : 1; // reserved / Z flag


	// Picture ID
	uint8_t pid_m : 1; // M bit (0 -> 7-bit PID; 1 -> 15-bit PID)
	uint16_t pid; // 7 or 15 bits (stored in lower bits)


	// Layer indices (if L)
	// In flexible mode (F=1): 1 octet
	// In non-flexible mode (F=0): 2 octets (first: indices, second: TL0PICIDX)
	uint8_t tid : 3;			// temporal layer id
	uint8_t up_switch : 1;		// U bit
	uint8_t sid : 3;			// spatial id
	uint8_t spatial_d : 1;		// D bit

	std::vector<uint8_t> p_diff;

	// TL0PICIDX (present in non-flexible mode when applicable)
	uint8_t tl0_pic_idx;

	vp9_scalability_header ss;
};


/*
typedef struct _vp9_header
{
	// Mandatory first octet
	uint8_t i : 1; // Picture ID present
	uint8_t p : 1; // inter-picture predicted
	uint8_t l : 1; // layer indices present
	uint8_t f : 1; // flexible mode
	uint8_t b : 1; // begin of frame
	uint8_t e : 1; // end of frame
	uint8_t v : 1; // scalability structure present
	uint8_t z : 1; // reserved / Z flag


	// Picture ID
	uint8_t pid_m : 1; // M bit (0 -> 7-bit PID; 1 -> 15-bit PID)
	uint16_t pid; // 7 or 15 bits (stored in lower bits)


	// Layer indices (if L)
	// In flexible mode (F=1): 1 octet
	// In non-flexible mode (F=0): 2 octets (first: indices, second: TL0PICIDX)
	uint8_t tid : 3;			// temporal layer id
	uint8_t up_switch : 1;		// U bit
	uint8_t sid : 3;			// spatial id
	uint8_t spatial_d : 1;		// D bit

	uint8_t p_diff[3];
	uint8_t p_diff_count;

	// TL0PICIDX (present in non-flexible mode when applicable)
	uint8_t tl0_pic_idx;


	// Flags
	uint8_t ss_present; // V bit set (scalability structure present)

} vp9_header;






int vp9_header_deserialize(vp9_header* hdr, const uint8_t* buffer, int size);
int vp9_header_serialize(const vp9_header* hdr, uint8_t* buffer, int buf_size);

*/