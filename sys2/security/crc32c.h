#ifndef __CRC32_H__
#define __CRC32_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
	extern const uint32_t crc_c[256];

#define CRC32C_POLY 0x1EDC6F41
#define CRC32C(c,d) (c=(c>>8)^crc_c[(c^(d))&0xFF])

	//uint32_t crc32 = ~(uint32_t)0;

	uint32_t crc32c(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif//__CRC32_H__

