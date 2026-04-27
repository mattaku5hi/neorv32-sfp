// Tests the transaction executor against a mocked TWI / GPIO HAL.

#include <stdarg.h>
#include <string.h>

#include "host_io.h"
#include "neo_exec.h"
#include "neo_io.h"
#include "neo_parser.h"
#include "test_common.h"

// ------------------------------------------------------------------------
// Mock HAL: records every TWI byte and lets each test script NACK patterns
// and set slave response bytes.
// ------------------------------------------------------------------------

typedef struct 
{
    int     log_n;
    char    log[1024];      // human-readable trace
    // response bytes to feed as RX on transactions; if exhausted, returns 0x00
    uint8_t rx_data[64];
    int     rx_data_n;
    int     rx_data_idx;
    // if ack_pattern[i] is 1, the i-th transaction reports NACK
    uint8_t ack_pattern[64];
    int     ack_n;
    int     ack_idx;
    int     starts, stops;
    uint8_t last_gpio_write;
    uint8_t next_gpio_read;
} mock_hal_t;

static mock_hal_t mk;

static void mk_log(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    char tmp[64];
    int n = neo_vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if(n > (int)sizeof(tmp) - 1) 
    {
        n = sizeof(tmp) - 1;
    }
    if(mk.log_n + n < (int)sizeof(mk.log)) 
    {
        memcpy(mk.log + mk.log_n, tmp, n);
        mk.log_n += n;
        mk.log[mk.log_n] = '\0';
    }
}

static void mock_start(void) 
{ 
    mk.starts++; 
    mk_log("[S]"); 
}
static void mock_stop (void) 
{ 
    mk.stops++;  
    mk_log("[P]"); 
}

static int  mock_trans(uint8_t *b, int host_ack)
{
    // TX byte goes out; RX byte replaces *b for reads (master writes 0xFF).
    uint8_t tx = *b;
    int ack = 0;
    if(mk.ack_idx < mk.ack_n) 
    {
        ack = mk.ack_pattern[mk.ack_idx++];
    }
    // Model a slave that drives the bus on every read (tx==0xFF means the
    // master released SDA for the slave).  The 'host_ack' flag is irrelevant
    // to whether the slave transmits.
    if(mk.rx_data_idx < mk.rx_data_n && tx == 0xFF) 
    {
        *b = mk.rx_data[mk.rx_data_idx++];
    }
    mk_log("<%02X/%c>", tx, ack ? 'N' : 'A');
    return ack;
}

static void mock_gpio_w(uint8_t v) 
{ 
    mk.last_gpio_write = v; 
}
static uint8_t mock_gpio_r(void)    
{ 
    return mk.next_gpio_read; 
}

static const neo_exec_hal_t mock_hal = 
{
    .twi_start       = mock_start,
    .twi_stop        = mock_stop,
    .twi_trans       = mock_trans,
    .gpio_write_byte = mock_gpio_w,
    .gpio_read_byte  = mock_gpio_r,
};

static void mock_reset(void)
{
    memset(&mk, 0, sizeof(mk));
    neo_exec_set_hal(&mock_hal);
}

// ------------------------------------------------------------------------

TEST(exec_simple_write)
{
    host_io_begin();
    mock_reset();
    neo_txn_t t; memset(&t, 0, sizeof(t));
    ASSERT_EQ_INT(neo_parse_transaction("hA0_h10_hAA_hBB", &t), 0);
    neo_exec_run(&t);

    ASSERT_EQ_INT(mk.starts, 1);
    ASSERT_EQ_INT(mk.stops, 1);
    // Four bytes transmitted: ctrl, iaddr, data1, data2
    ASSERT_EQ_STR(mk.log, "[S]<A0/A><10/A><AA/A><BB/A>[P]");
    // Output: each byte + '+' on ACK, newline
    ASSERT_EQ_STR(host_io_output(), "A0+10+AA+BB+\n");
    return 0;
}

TEST(exec_write_nack)
{
    host_io_begin();
    mock_reset();
    // Slave NACKs the 3rd transaction (the data byte).
    mk.ack_pattern[2] = 1; mk.ack_n = 3;
    neo_txn_t t; memset(&t, 0, sizeof(t));
    neo_parse_transaction("hA0_h00_hFF", &t);
    neo_exec_run(&t);
    ASSERT_EQ_STR(host_io_output(), "A0+00+FF-\n");
    return 0;
}

TEST(exec_read_hex_format)
{
    host_io_begin();
    mock_reset();
    mk.rx_data[0] = 0x11; mk.rx_data[1] = 0x22; mk.rx_data_n = 2;
    neo_txn_t t; 
    memset(&t, 0, sizeof(t));
    neo_parse_transaction("hA1_h00_h02h_", &t);
    neo_exec_run(&t);
    // ctrl/iaddr echoed as hex with '+', then two RX bytes (no +)
    ASSERT_EQ_STR(host_io_output(), "A1+00+1122\n");
    return 0;
}

TEST(exec_gpio_out_in)
{
    host_io_begin();
    mock_reset();
    mk.next_gpio_read = 0x5A;
    neo_txn_t t; 
    memset(&t, 0, sizeof(t));
    neo_parse_transaction("out_10110011", &t);
    neo_exec_run(&t);
    ASSERT_EQ_INT(mk.last_gpio_write, 0xB3);
    ASSERT_TRUE(strstr(host_io_output(), "gpio <= 10110011") != NULL);

    host_io_begin();
    mock_reset();
    mk.next_gpio_read = 0x5A;
    memset(&t, 0, sizeof(t));
    neo_parse_transaction("inp", &t);
    neo_exec_run(&t);
    ASSERT_EQ_STR(host_io_output(), "gpio => 01011010\n");
    return 0;
}
