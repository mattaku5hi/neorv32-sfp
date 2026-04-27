#pragma once


#include <stdint.h>
#include <neorv32.h>
#include <utils_common.h>


#ifdef PRINT_LOG_UART
#define NEO_CONSOLE_INIT(rate)  neorv32_uart0_setup(rate, 0);
#define NEO_CONSOLE_PRINTCHR(value) neorv32_uart0_putc(value)
#define NEO_CONSOLE_PRINTHEX(value)  neo_uart_print_hex(NEORV32_UART0, value)
#define NEO_CONSOLE_PRINTSTR(value) neorv32_uart0_puts(value)
#define NEO_CONSOLE_PRINTF(...) neorv32_uart0_printf(__VA_ARGS__)
#define NEO_CONSOLE_MACHINE_INFO() neo_uart_print_device_info()
#else
#define NEO_CONSOLE_INIT(rate)  {} 
#define NEO_CONSOLE_PRINTCHR(value) {}
#define NEO_CONSOLE_PRINTHEX(value)  {}
#define NEO_CONSOLE_PRINTSTR(value) {}
#define NEO_CONSOLE_PRINTF(...) {}
#define NEO_CONSOLE_MACHINE_INFO() {}
#endif


extern char __neorv32_ram_size[];
extern char __neorv32_rom_size[];


inline __attribute__ ((always_inline)) uint32_t neo_rom_size_get(void)
{
    return (uint32_t)&__neorv32_rom_size[0];
}

inline __attribute__ ((always_inline)) uint32_t neo_ram_size_get(void)
{
    return (uint32_t)&__neorv32_ram_size[0];
}

