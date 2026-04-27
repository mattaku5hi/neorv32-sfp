#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

// Minimal printf/scanf-like routines without <stdlib.h> / FP.
//
// Supported conversions in neo_printf():
//   %c           single char
//   %s           NUL-terminated string (precision = max chars)
//   %d  / %i     signed   decimal (int)
//   %u           unsigned decimal (unsigned)
//   %x  / %X     unsigned hex (lower / upper)
//   %o           unsigned octal
//   %b           unsigned binary
//   %p           pointer (printed as 0x%08x)
//   %%           literal '%'
//
// Supported flags : '-', '0'
// Supported width : decimal (incl. '*')
// Supported prec  : decimal (incl. '*') for %s/%d/%u/%x/%o/%b
// Length modifier : 'l' / 'll' accepted (treated as int / int64 respectively;
//                   the integer formatter uses a 32-bit core, ll uses 64-bit)

typedef int (*neo_putc_fn)(int c, void *ctx);
typedef int (*neo_getc_fn)(void *ctx);   // returns byte 0..255 or -1

typedef struct 
{
    neo_putc_fn putc;
    neo_getc_fn getc;
    void       *ctx;
} neo_io_backend_t;

void neo_io_set_backend(const neo_io_backend_t *b);

int  neo_putc (int c);
int  neo_puts (const char *s);              // like fputs (no newline added)
int  neo_getc (void);                        // blocking get; -1 on EOF

int  neo_printf (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int  neo_vprintf(const char *fmt, va_list ap);

// Format into caller-provided buffer.  Always NUL-terminates if size > 0.
// Returns number of chars that would have been written (excl. NUL),
// exactly like C99 snprintf.
int  neo_snprintf (char *buf, size_t size, const char *fmt, ...)
        __attribute__((format(printf, 3, 4)));
int  neo_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

// Format into caller backend (e.g. UART) without touching the global one.
int  neo_vfprintf(neo_putc_fn out, void* ctx, const char* fmt, va_list ap);

// Read a line into buf (up to max-1 chars).  Reads from the global backend.
// Terminates on '\n' or '\r'.  Backspace (0x08 / 0x7f) erases.  Echoes when
// 'echo' is non-zero.  Always NUL-terminates.  Returns length (excl. NUL).
int  neo_read_line(char* buf, size_t max, int echo);

