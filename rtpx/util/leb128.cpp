
#include "leb128.h"



size_t leb128::encode(uint32_t value, uint8_t* out, size_t len)
{
    size_t i = 0;

    for (;;)
    {
        if (i >= len) 
            return 0;

        uint8_t byte = value & 0x7F;
        value >>= 7;

        if (value != 0) {
            byte |= 0x80;
        }

        out[i++] = byte;

        if (value == 0) 
            break;
    }

    return i;
}

size_t leb128::decode(const uint8_t* data, size_t len, uint32_t* value)
{
    uint32_t result = 0;
    int shift = 0;
    size_t i = 0;

    while (i < len) {
        uint8_t byte = data[i];
        result |= (uint32_t)(byte & 0x7F) << shift;
        i++;

        if ((byte & 0x80) == 0) 
        {
            *value = result;
            return i;
        }

        shift += 7;
        if (shift >= 32) 
            break; //
    }

    return 0; // error
}