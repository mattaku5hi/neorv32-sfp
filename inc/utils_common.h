#pragma once


#include <stdint.h>

#include <neorv32.h>


void neo_uart_print_hex(neorv32_uart_t* pBase, uint32_t value);
void neo_uart_print_device_info(void);

inline uint32_t __attribute__ ((always_inline)) bswap32(uint32_t value)
{
    uint32_t result = ((value & 0xFF000000) >> 24) |
                        ((value & 0x00FF0000) >> 8)  |
                        ((value & 0x0000FF00) << 8)  |
                        ((value & 0x000000FF) << 24);
    return result;
}

