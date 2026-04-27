// neo_exec.c -- transaction executor.
//
// Output rules (per spec):
//   write byte:    "<HH>+" on ACK, "<HH>-" on NACK
//   read header:   ctrl byte and internal-addr byte echoed in hex with the
//                  same +/- ACK marker
//   read payload:  printed in the requested format; if the device NACKed a
//                  particular byte, '-' is printed in place of the byte.
//   each command on its own line; transaction prompt printed elsewhere.

#include "neo_exec.h"
#include "neo_io.h"

// -------------------------------------------------------------------------
// HAL wiring
// -------------------------------------------------------------------------

static const neo_exec_hal_t *s_hal = 0;

void neo_exec_set_hal(const neo_exec_hal_t *hal) { s_hal = hal; }

#ifndef NEO_EXEC_NO_NEORV32
#  include <neorv32.h>

static void hal_neo_start(void)           
{ 
    neorv32_twi_generate_start(); 
}
static void hal_neo_stop (void)           
{ 
    neorv32_twi_generate_stop();  
}
static int  hal_neo_trans(uint8_t *b, int a)
{ 
    return neorv32_twi_trans(b, a); 
}
static void hal_neo_gpio_w(uint8_t v)     
{ 
    neorv32_gpio_port_set(v); 
}
static uint8_t hal_neo_gpio_r(void)       
{ 
    return (uint8_t)neorv32_gpio_port_get(); 
}

const neo_exec_hal_t neo_exec_hal_neorv32 = 
{
    .twi_start       = hal_neo_start,
    .twi_stop        = hal_neo_stop,
    .twi_trans       = hal_neo_trans,
    .gpio_write_byte = hal_neo_gpio_w,
    .gpio_read_byte  = hal_neo_gpio_r,
};
#endif

// -------------------------------------------------------------------------
// Pretty-printers
// -------------------------------------------------------------------------
static void print_hex_byte(uint8_t b)
{
    static const char hd[] = "0123456789ABCDEF";
    neo_putc(hd[(b >> 4) & 0xF]);
    neo_putc(hd[b & 0xF]);
}

static void print_bin_byte(uint8_t b)
{
    int i;
    for(i = 7; i >= 0; --i) 
    {
        neo_putc((b >> i) & 1 ? '1' : '0');
    }
}

static void print_byte_in_fmt(uint8_t b, neo_fmt_t f)
{
    switch(f)
    {
    case NEO_FMT_HEX:   
        print_hex_byte(b); 
        break;
    case NEO_FMT_BIN:   
        print_bin_byte(b); 
        break;
    case NEO_FMT_ASCII:
    {
        if(b >= 0x20 && b < 0x7F)
        {
            neo_putc((char)b);
        }
        else
        {
            neo_putc('.');
        }
        break;
    }
    default:            
        print_hex_byte(b); 
        break;
    }
}

// -------------------------------------------------------------------------
// TWI write: send ctrl, internal addr, then payload bytes.  No STOP at the
// end, per spec ("после окончания передачи команды комбинации STOP может не
// быть, сразу может быть комбинация START для следующей команды").  The
// caller (neo_exec_run) issues the trailing STOP at the end of the chain.
// -------------------------------------------------------------------------

static void exec_twi_byte(uint8_t v, int host_ack)
{
    uint8_t b = v;
    int ack = s_hal->twi_trans(&b, host_ack);
    print_hex_byte(v);
    neo_putc(ack == 0 ? '+' : '-');
}

static void exec_write(const neo_txn_t *txn, const neo_cmd_t *cmd)
{
    s_hal->twi_start();
    exec_twi_byte(cmd->ctrl_byte, 0);
    exec_twi_byte(cmd->internal_addr, 0);

    int i;
    for(i = 0; i < cmd->payload_len; ++i) 
    {
        uint8_t v = txn->payload_pool[cmd->payload_off + i];
        exec_twi_byte(v, 0);
    }
}

static void exec_read(const neo_cmd_t *cmd)
{
    s_hal->twi_start();
    exec_twi_byte(cmd->ctrl_byte, 0);
    exec_twi_byte(cmd->internal_addr, 0);

    // For each requested byte, transmit 0xFF with host_ack=1 except the last
    // byte which gets host_ack=0 (NACK from master to signal end of read).
    int i;
    for(i = 0; i < cmd->rd_len; ++i) 
    {
        uint8_t b = 0xFF;
        int last = (i + 1 == cmd->rd_len);
        int ack = s_hal->twi_trans(&b, last ? 0 : 1);
        if(ack != 0) 
        {
            // Slave failed to drive ACK on this byte: emit '-'.
            neo_putc('-');
        }
        else 
        {
            print_byte_in_fmt(b, cmd->rd_fmt);
        }
    }
}

// -------------------------------------------------------------------------
// Help text.
// -------------------------------------------------------------------------

void neo_exec_print_help(void)
{
    neo_puts(
      "Commands:\n"
      "  hA0|hA2 <iaddr> <data>           write data to slave (no STOP if chained)\n"
      "  hA1|hA3 <iaddr> h<n><h|b|a>_     read up to h3FE bytes, display in fmt\n"
      "  out_xxxxxxxx                     write 8-bit GPIO from 8 binary digits\n"
      "  inp                              read GPIO byte\n"
      "  help | ?                         this message\n"
      "Data byte formats: h<HH>... (hex pairs), b<bbbbbbbb>... (8-bit groups,\n"
      "  '_' for grouping), a<chars>_ (latin letters/digits, '_' terminated).\n"
      "Bytes equal to A0/A1/A2/A3 inside a command MUST use binary form.\n"
      "Transaction terminator: Enter (max 1024 bytes per line).\n");
}

// -------------------------------------------------------------------------
// Top-level executor.
// -------------------------------------------------------------------------

void neo_exec_run(const neo_txn_t *txn)
{
    if(!s_hal) 
    {
#ifndef NEO_EXEC_NO_NEORV32
        s_hal = &neo_exec_hal_neorv32;
#else
        neo_puts("ERROR: no HAL configured\n");
        return;
#endif
    }

    int issued_twi = 0;
    int i;
    for(i = 0; i < txn->count; ++i) 
    {
        const neo_cmd_t *cmd = &txn->cmds[i];
        switch(cmd->kind) 
        {
        case NEO_CMD_TWI_WRITE:
            exec_write(txn, cmd);
            issued_twi = 1;
            neo_putc('\n');
            break;
        case NEO_CMD_TWI_READ:
            exec_read(cmd);
            issued_twi = 1;
            neo_putc('\n');
            break;
        case NEO_CMD_GPIO_OUT:
            s_hal->gpio_write_byte(cmd->gpio_byte);
            neo_puts("gpio <= ");
            print_bin_byte(cmd->gpio_byte);
            neo_putc('\n');
            break;
        case NEO_CMD_GPIO_IN: 
        {
            uint8_t v = s_hal->gpio_read_byte();
            neo_puts("gpio => ");
            print_bin_byte(v);
            neo_putc('\n');
            break;
        }
        case NEO_CMD_HELP:
            neo_exec_print_help();
            break;
        default:
            break;
        }
    }
    if(issued_twi) 
    {
        s_hal->twi_stop();
    }
}
