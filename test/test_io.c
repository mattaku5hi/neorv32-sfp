// Unit tests for neo_io: printf / snprintf / read_line.

#include <string.h>

#include "host_io.h"
#include "neo_io.h"
#include "test_common.h"


// ---------- snprintf tests (don't need the backend) ----------------------

TEST(snprintf_basic)
{
    char b[64];
    int n;

    n = neo_snprintf(b, sizeof(b), "hello %s!", "world");
    ASSERT_EQ_INT(n, 12);
    ASSERT_EQ_STR(b, "hello world!");

    n = neo_snprintf(b, sizeof(b), "%d %d %d", -1, 0, 42);
    ASSERT_EQ_INT(n, 7);
    ASSERT_EQ_STR(b, "-1 0 42");

    return 0;
}

TEST(snprintf_hex_and_width)
{
    char b[64];
    neo_snprintf(b, sizeof(b), "%02x-%04x-%08X", 0x5, 0xBEEF, 0xDEADBEEF);
    ASSERT_EQ_STR(b, "05-beef-DEADBEEF");

    neo_snprintf(b, sizeof(b), "[%5d][%-5d]", 7, 7);
    ASSERT_EQ_STR(b, "[    7][7    ]");

    return 0;
}

TEST(snprintf_binary)
{
    char b[64];
    neo_snprintf(b, sizeof(b), "%08b", 0xA5);
    ASSERT_EQ_STR(b, "10100101");
    return 0;
}

TEST(snprintf_truncation)
{
    char b[6];
    int n = neo_snprintf(b, sizeof(b), "0123456789");
    ASSERT_EQ_INT(n, 10);
    ASSERT_EQ_STR(b, "01234");   // 5 chars + NUL
    return 0;
}

TEST(snprintf_signed_zero_pad)
{
    char b[32];
    neo_snprintf(b, sizeof(b), "%08d", -42);
    ASSERT_EQ_STR(b, "-0000042");
    return 0;
}

TEST(snprintf_percent_and_char)
{
    char b[32];
    neo_snprintf(b, sizeof(b), "%%-%c-%%", 'Z');
    ASSERT_EQ_STR(b, "%-Z-%");
    return 0;
}

TEST(snprintf_string_prec)
{
    char b[32];
    neo_snprintf(b, sizeof(b), "[%.3s]", "abcdef");
    ASSERT_EQ_STR(b, "[abc]");
    return 0;
}

// ---------- printf via backend ------------------------------------------

TEST(printf_backend_capture)
{
    host_io_begin();
    neo_printf("n=%d, s=%s\n", 7, "x");
    ASSERT_EQ_STR(host_io_output(), "n=7, s=x\n");
    return 0;
}

// ---------- read_line tests ---------------------------------------------

TEST(read_line_simple)
{
    host_io_begin();
    host_io_set_input("hello\n");
    char buf[16];
    int n = neo_read_line(buf, sizeof(buf), 0);
    ASSERT_EQ_INT(n, 5);
    ASSERT_EQ_STR(buf, "hello");
    return 0;
}

TEST(read_line_backspace)
{
    host_io_begin();
    // type 'a','b','c', BS, 'd' → line is 'abd'
    host_io_set_input("abc\x08" "d\n");
    char buf[16];
    int n = neo_read_line(buf, sizeof(buf), 0);
    ASSERT_EQ_INT(n, 3);
    ASSERT_EQ_STR(buf, "abd");
    return 0;
}

TEST(read_line_max)
{
    host_io_begin();
    host_io_set_input("0123456789ABCDEF\n"); // 16 chars
    char buf[8];     // holds up to 7 + NUL
    int n = neo_read_line(buf, sizeof(buf), 0);
    ASSERT_EQ_INT(n, 7);
    ASSERT_EQ_STR(buf, "0123456");
    return 0;
}

TEST(read_line_echo)
{
    host_io_begin();
    host_io_set_input("ok\n");
    char buf[8];
    neo_read_line(buf, sizeof(buf), 1);
    // echo 'o','k' then '\n' upon CR/LF
    ASSERT_EQ_STR(host_io_output(), "ok\n");
    return 0;
}
