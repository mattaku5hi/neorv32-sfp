
#include <utils.h>


void neo_uart_print_hex(neorv32_uart_t* pBase, uint32_t value)
{
    static const char hex_symbols[16] = "0123456789abcdef";

    neorv32_uart_putc(pBase, '0');
    neorv32_uart_putc(pBase, 'x');

    int i;
    for(i = 28; i >= 0; i -= 4)
    {
        char c = hex_symbols[(value >> i) & 0xf];
        neorv32_uart_putc(pBase, c);
    }

    return;
}

void neo_uart_print_device_info(void)
{
    unsigned char mvendorid = neorv32_cpu_csr_read(CSR_MVENDORID);
    unsigned char marchid = neorv32_cpu_csr_read(CSR_MARCHID);
    unsigned char mimpid = neorv32_cpu_csr_read(CSR_MIMPID);
    unsigned char mhartid = neorv32_cpu_csr_read(CSR_MHARTID);

    neorv32_uart0_printf("Machine information: vendor id - 0x%x; arch id - 0x%x; "
                            "thread id - 0x%x; implementation id - 0x%x\n",
                            mvendorid, marchid, mimpid, mhartid); 
    
    return;
}
