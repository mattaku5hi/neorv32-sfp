// neo_app.c -- top-level command-line application.  See spec in repo README.

#include <neorv32.h>

#include "neo_app.h"
#include "neo_exec.h"
#include "neo_io.h"
#include "neo_parser.h"
#include "neo_uart.h"


// Static line buffer (1024 useful bytes per spec + 1 for the NUL) and decoded
// transaction state.  Kept as file-scope statics to avoid the large struct
// landing on the (small) stack.
static char       s_line[NEO_PARSER_LINE_MAX + 1];
static neo_txn_t  s_txn;


void neo_app_run(uint32_t uart_baud)
{
    neo_uart_init(uart_baud);
    neo_uart_install_io();

    // TWI: prescaler=2048, divider=15, no clock stretching (matches the
    // NEORV32 demo and yields a safe slow bus regardless of core clock).
    if(neorv32_twi_available()) 
    {
        neorv32_twi_setup(CLK_PRSC_2048, 15, 0);
    } 
    else 
    {
        neo_puts("WARNING: TWI controller not available\n");
    }

    if(!neorv32_gpio_available()) 
    {
        neo_puts("WARNING: GPIO controller not available\n");
    }

    neo_puts("\nNEO shell ready.  Type 'help' or '?' for commands.\n");

    for(;;) 
    {
        neo_putc('>');
        neo_putc(' ');

        int n = neo_uart_read_line(s_line, sizeof(s_line));
        if(n == 0) 
        {
            continue;
        }

        if(neo_parse_transaction(s_line, &s_txn) != 0) 
        {
            neo_printf("ERROR at offset %d: %s\n",
                       s_txn.error_pos,
                       s_txn.error ? s_txn.error : "(unknown)");
            continue;
        }

        neo_exec_run(&s_txn);
    }
}

