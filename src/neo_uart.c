// neo_uart.c -- target NEORV32 UART backend for neo_io and the application.

#include <neorv32.h>

#include "neo_uart.h"


static const neo_io_backend_t s_be = 
{
    .putc = neo_uart_putc_be,
    .getc = neo_uart_getc_be,
    .ctx  = 0,
};


void neo_uart_init(uint32_t baudrate)
{
    neorv32_uart0_setup(baudrate, 0);
}

int neo_uart_putc_be(int c, void *ctx)
{
    (void)ctx;
    neorv32_uart0_putc((char)c);
    return c;
}

int neo_uart_getc_be(void *ctx)
{
    (void)ctx;
    // Block until a char arrives.
    return (unsigned char)neorv32_uart0_getc();
}

void neo_uart_install_io(void)
{
    neo_io_set_backend(&s_be);
}

int neo_uart_read_line(char *buf, size_t max)
{
    // Implemented in terms of neo_read_line so logic is shared with tests.
    neo_uart_install_io();
    return neo_read_line(buf, max, 1);
}

