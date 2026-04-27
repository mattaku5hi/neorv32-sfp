// Application entry point.  Brings up NEORV32 peripherals, prints a banner
// with build/runtime info and hands control to the TWI/GPIO shell.

#include <neorv32.h>
#include <stdint.h>

#include "neo_app.h"
#include "neo_io.h"
#include "neo_uart.h"
#include "utils.h"
#include "utils_common.h"


#define NEO_SYS_CLK             27000000UL
#define NEO_UART_BAUDRATE       115200


static void neo_init_banner(void);


int main(void)
{
    neo_uart_init(NEO_UART_BAUDRATE);
    neo_uart_install_io();
    neo_init_banner();
    
    neo_app_run(NEO_UART_BAUDRATE);
    __builtin_unreachable();
}


static void neo_init_banner(void)
{
    neorv32_gpio_port_set(0);

    if(neorv32_clint_available() == 0) 
    {
        neo_puts("WARNING! MTIME machine timer not available!\n");
    } 
    else 
    {
        neorv32_cpu_csr_set(CSR_MIE, 1 << CSR_MIE_MTIE);
    }

    neo_printf("NEORV32 ROM size: %u bytes\n", neo_rom_size_get());
    neo_printf("NEORV32 RAM size: %u bytes\n", neo_ram_size_get());
    neo_printf("NEORV32 heap @ %p, %u bytes\n",
               neorv32_heap_begin_c, neorv32_heap_size_c);

    uint32_t coreClock = neorv32_sysinfo_get_clk();
    if(coreClock != NEO_SYS_CLK) 
    {
        neo_printf("WARNING: clock mismatch: hw=%u Hz sw=%u Hz\n",
                   coreClock, NEO_SYS_CLK);
    } 
    else 
    {
        neo_printf("NEORV32 clock rate: %u Hz\n", coreClock);
    }
}
