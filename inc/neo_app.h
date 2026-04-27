#pragma once

#include <stdint.h>


// Top-level REPL.  Initializes UART/TWI/GPIO and never returns.
void neo_app_run(uint32_t uart_baud);

