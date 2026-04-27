# Host-side unit tests

Tiny, dependency-free test harness for the freestanding modules used on the
NEORV32 target:

* `neo_io`       — our minimal `printf` / `snprintf` / line-input (no `<stdlib.h>`,
                    no floating point).
* `neo_parser`   — transaction parser / validator for the shell language.
* `neo_exec`     — TWI / GPIO executor (linked with a mock HAL on the host).
* `neo_uart`     — UART line-buffer shim (tested indirectly through `neo_io`
                    using a string-fed backend in `host_io.c`).

## Build & run

```bash
make -C test
make -C test run
```

`make run` builds `test/run_tests` and executes every registered `TEST()` case.
Exit status is non-zero on any failure.

## Layout

```
test/
├── Makefile          – host build rules
├── main.c            – test runner (walks the registration table)
├── test_common.h     – TEST(...) + ASSERT_* macros
├── host_io.c/.h      – string-fed neo_io backend used by the suites
├── test_io.c         – snprintf / printf / line-input tests
├── test_parser.c     – positive and negative parser cases
├── test_linebuf.c    – 1024-byte round-trip through the buffer
└── test_exec.c       – executor with a mock TWI + GPIO HAL
```

## How the host backend works

`host_io.c` registers a `neo_io_backend_t` that stores everything printed in a
16 KiB in-memory buffer (`host_io_output()`) and drains input from a string
(`host_io_set_input()`).  Every test calls `host_io_begin()` first to reset the
buffers.  This is the same backend plug used on the target, so the logic
covered by the tests is byte-for-byte identical to what runs on the FPGA.

## Test-inventory quick reference

```
snprintf_basic               basic %s %d formatting
snprintf_hex_and_width        %02x / width / -flag
snprintf_binary               %08b
snprintf_truncation           returns would-have-been length, NUL-terminates
snprintf_signed_zero_pad      sign-aware zero padding for negatives
snprintf_percent_and_char     %% and %c
snprintf_string_prec          %.Ns
printf_backend_capture        printf writes through the active backend
read_line_simple              plain line
read_line_backspace           0x08 erases previous char
read_line_max                 caller-bound truncation respected
read_line_echo                echo mode prints characters and '\n'
parse_simple_write            hA0_h00_hDE_hAD
parse_mixed_formats_from_spec hA0_b10010101h09E7b..._aCentin_h4E
parse_chained_commands        two chained write commands
parse_read_command            hA1_h00_h10h_
parse_read_zero_bytes         hA1_h00_
parse_out_command             out_10101010 → 0xAA
parse_inp_help                inp / help / ?
parse_rejects_*               negative cases (odd hex, 7-bit binary, ...)
linebuf_round_trip            read one line, echo it back through our printf
linebuf_max_1024              full 1024-byte line buffer
exec_simple_write             full write TX trace + "HH+" output
exec_write_nack               NACK reported as "HH-"
exec_read_hex_format          read header + 2 rx bytes in hex
exec_gpio_out_in              GPIO write/read + formatting
```
