#pragma once


#include <stddef.h>
#include <stdint.h>

#include "neo_io.h"


// Thin UART driver wrapper used by the application.  Compile-time selectable
// between the on-target NEORV32 UART and a host stdio stub used by the unit
// tests in test/.
//
// The maximum length of one transaction line (excluding terminator) is
// NEO_UART_LINE_MAX bytes.  This must be >= 1024 per project specification.

#define NEO_UART_LINE_MAX  1024

void neo_uart_init(uint32_t baudrate);

// Low-level char I/O (suitable as neo_io backend).
int  neo_uart_putc_be(int c, void* ctx);
int  neo_uart_getc_be(void* ctx);

// Convenience: install neo_uart as the global neo_io backend.
void neo_uart_install_io(void);

// Read one transaction line into 'buf' (capacity 'max', incl. NUL).
// Echoes characters as the user types.  Honors backspace.  Stops on '\r' or
// '\n'.  Returns the number of bytes stored (excluding NUL).
//
// If the line would exceed 'max - 1', extra bytes are dropped silently
// (the caller may detect this by reading exactly max-1 bytes).
int neo_uart_read_line(char* buf, size_t max);


