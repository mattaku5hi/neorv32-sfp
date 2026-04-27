#pragma once

#include <stddef.h>

#include "neo_io.h"

// Host-side neo_io backend.  Output is captured in an in-memory buffer so
// tests can assert on it; input is drained from a caller-supplied string.
// Call host_io_begin() before each test case.

void  host_io_begin(void);
void  host_io_set_input(const char *s);
const char *host_io_output(void);        // NUL-terminated captured output
size_t host_io_output_len(void);

void host_io_install(void);              // binds to neo_io_set_backend()

// Stdio fallback (useful for interactive debug main).
void host_io_install_stdio(void);

