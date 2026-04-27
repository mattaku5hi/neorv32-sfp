// End-to-end line-buffer round-trip tests.  Feeds a string through neo_read_line
// and then echoes the captured buffer back via neo_printf so we can assert that
// bytes get in, are stored correctly, and can be written back out using our own
// stdlib replacement.

#include <string.h>

#include "host_io.h"
#include "neo_io.h"
#include "test_common.h"


TEST(linebuf_round_trip)
{
    host_io_begin();
    const char *input = "hA0_h00_hDE_hAD\n";
    host_io_set_input(input);

    char buf[32];
    int n = neo_read_line(buf, sizeof(buf), 0);
    ASSERT_EQ_INT(n, (int)strlen(input) - 1);   // minus the '\n'
    ASSERT_EQ_STR(buf, "hA0_h00_hDE_hAD");

    // Now echo it back via neo_printf and verify identical bytes landed in
// the output capture.
    neo_printf("echo=%s|len=%d\n", buf, n);
    ASSERT_EQ_STR(host_io_output(), "echo=hA0_h00_hDE_hAD|len=15\n");
    return 0;
}

TEST(linebuf_max_1024)
{
    host_io_begin();
    // Build a 1024-byte alnum line ending with '\n'.
    static char in[1026];
    int i;
    for(i = 0; i < 1024; ++i)
    {
        in[i] = (char)('a' + (i % 26));
    }
    in[1024] = '\n';
    in[1025] = '\0';
    host_io_set_input(in);

    static char buf[1025];
    int n = neo_read_line(buf, sizeof(buf), 0);
    ASSERT_EQ_INT(n, 1024);
    ASSERT_EQ_INT((int)strlen(buf), 1024);
    ASSERT_EQ_MEM(buf, in, 1024);
    return 0;
}
