#pragma once


#include <stddef.h>
#include <stdint.h>

// Transaction parser / validator.
//
// One "transaction" is the line entered by the user (terminated by Enter).
// Maximum input length: NEO_PARSER_LINE_MAX bytes (1024 per spec).
//
// A transaction may contain:
//
//   - Several TWI commands, each starting with 'h' followed by a control byte
//     (A0/A1/A2/A3).  The remainder of a command is a sequence of bytes
//     encoded in one of three formats:
//
//       h<HH>...           hex bytes (count of hex digits must be a multiple
//                          of 2).  May be split with '_' between *different*
//                          format groups, but *not* inside a hex run.
//       b<bbbbbbbb>...     binary bytes (count of bits must be a multiple
//                          of 8).  '_' allowed inside for visual grouping.
//       a<chars>           ASCII letters/digits.  Terminated by '_'.
//
//     Any of A0/A1/A2/A3 byte values that occur *inside* a command's payload
//     must be rendered in binary form (so the parser cannot mistake them for
//     a fresh command).
//
//     Read commands (A1/A3) carry: internal_addr byte, then read length in
//     hex (max h3FE), then a single format char (h/b/a), then a closing '_'.
//     Missing length means zero bytes; missing format is OK only if length
//     is zero.
//
//   - Or one of the special textual commands:
//       out_xxxxxxxx       set GPIO byte (8 binary digits)
//       inp                read GPIO byte and print
//       help               print help
//       ?                  same as help
//
//  Special commands appear standalone (not chained).
//
// Parsing produces a list of decoded commands sharing a single byte buffer.

#define NEO_PARSER_LINE_MAX     1024
#define NEO_PARSER_PAYLOAD_MAX  1022   // per spec: write up to 1022 data bytes
// Upper bound on commands per transaction.  A minimal command is ~7 chars
// (e.g. "hA1h0h_"), so a 1024-byte line could in theory pack ~140 commands;
// 160 gives comfortable headroom while keeping neo_txn_t well below 8 KiB.
#define NEO_PARSER_MAX_CMDS     160

typedef enum 
{
    NEO_CMD_NONE = 0,
    NEO_CMD_TWI_WRITE,    // A0 / A2
    NEO_CMD_TWI_READ,     // A1 / A3
    NEO_CMD_GPIO_OUT,
    NEO_CMD_GPIO_IN,
    NEO_CMD_HELP,
} neo_cmd_kind_t;

typedef enum 
{
    NEO_FMT_NONE  = 0,
    NEO_FMT_HEX   = 'h',
    NEO_FMT_BIN   = 'b',
    NEO_FMT_ASCII = 'a',
} neo_fmt_t;

typedef struct 
{
    neo_cmd_kind_t kind;
    // TWI fields
    uint8_t   ctrl_byte;        // A0/A1/A2/A3 (incl. R/W bit)
    uint8_t   internal_addr;    // device-internal address byte
    uint16_t  rd_len;            // bytes to read (TWI_READ)
    neo_fmt_t rd_fmt;            // display format for read result
    // For TWI_WRITE: location of payload bytes inside the shared payload pool.
    uint16_t  payload_off;
    uint16_t  payload_len;
    // GPIO_OUT byte (already decoded)
    uint8_t   gpio_byte;
} neo_cmd_t;

typedef struct 
{
    int         count;
    neo_cmd_t   cmds[NEO_PARSER_MAX_CMDS];
    uint8_t     payload_pool[NEO_PARSER_PAYLOAD_MAX * 2];
    int         payload_used;
    const char *error;          // NULL on success; static string on failure
    int         error_pos;      // byte offset within input where error was
} neo_txn_t;

// Parse + validate a NUL-terminated input line.  On success, returns 0 and
// fills 'out'.  On failure, returns non-zero and out->error is set.
int neo_parse_transaction(const char *line, neo_txn_t *out);

