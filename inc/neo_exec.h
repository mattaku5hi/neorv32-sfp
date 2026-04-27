#pragma once


#include "neo_parser.h"

// Executes a parsed transaction by talking to the TWI bus and the GPIO port,
// and prints the result via neo_io.
//
// The HAL is abstracted via a tiny vtable so the executor can be unit-tested
// on the host machine with mocked hardware.

typedef struct
{
    // TWI: generate START/STOP and exchange one byte.
    // twi_trans must return 0 on ACK and 1 on NACK from the slave.
    void (*twi_start)(void);
    void (*twi_stop) (void);
    int  (*twi_trans)(uint8_t *byte, int host_ack);
    // GPIO: write/read a single byte.
    void    (*gpio_write_byte)(uint8_t v);
    uint8_t (*gpio_read_byte) (void);
} neo_exec_hal_t;

// Default HAL using the on-target NEORV32 drivers (provided by neo_exec.c).
extern const neo_exec_hal_t neo_exec_hal_neorv32;

void neo_exec_set_hal(const neo_exec_hal_t *hal);

// Run all commands of the given transaction.  All output goes via neo_io.
void neo_exec_run(const neo_txn_t *txn);

// Print the help message.
void neo_exec_print_help(void);

