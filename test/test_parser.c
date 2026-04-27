// Unit tests for neo_parser.

#include <string.h>

#include "neo_parser.h"
#include "test_common.h"


static neo_txn_t txn;


static int do_parse(const char *s)
{
    memset(&txn, 0, sizeof(txn));
    return neo_parse_transaction(s, &txn);
}


// ---------- Positive cases ----------------------------------------------
TEST(parse_simple_write)
{
    ASSERT_EQ_INT(do_parse("hA0_h00_hDE_hAD"), 0);
    ASSERT_EQ_INT(txn.count, 1);
    ASSERT_EQ_INT(txn.cmds[0].kind, NEO_CMD_TWI_WRITE);
    ASSERT_EQ_INT(txn.cmds[0].ctrl_byte, 0xA0);
    ASSERT_EQ_INT(txn.cmds[0].internal_addr, 0x00);
    ASSERT_EQ_INT(txn.cmds[0].payload_len, 2);
    ASSERT_EQ_INT(txn.payload_pool[txn.cmds[0].payload_off + 0], 0xDE);
    ASSERT_EQ_INT(txn.payload_pool[txn.cmds[0].payload_off + 1], 0xAD);
    return 0;
}

TEST(parse_mixed_formats_from_spec)
{
    // The canonical example from the project spec.
    const char *line = "hA0_b10010101h09E7b0011_010_11111_1_011_aCentin_h4E";
    ASSERT_EQ_INT(do_parse(line), 0);
    ASSERT_EQ_INT(txn.count, 1);
    const neo_cmd_t *c = &txn.cmds[0];
    ASSERT_EQ_INT(c->kind, NEO_CMD_TWI_WRITE);
    ASSERT_EQ_INT(c->ctrl_byte, 0xA0);
    ASSERT_EQ_INT(c->internal_addr, 0x95);           // b10010101
    // payload: 0x09 0xE7 0x35 0xFB 'C' 'e' 'n' 't' 'i' 'n' 0x4E
    const uint8_t expect[] = { 0x09, 0xE7, 0x35, 0xFB,
                               'C','e','n','t','i','n', 0x4E 
    };
    ASSERT_EQ_INT(c->payload_len, (int)sizeof(expect));
    ASSERT_EQ_MEM(&txn.payload_pool[c->payload_off], expect, sizeof(expect));
    return 0;
}

TEST(parse_chained_commands)
{
    ASSERT_EQ_INT(do_parse("hA0_h10_hAA""hA2_h11_hBB"), 0);
    ASSERT_EQ_INT(txn.count, 2);
    ASSERT_EQ_INT(txn.cmds[0].ctrl_byte, 0xA0);
    ASSERT_EQ_INT(txn.cmds[0].internal_addr, 0x10);
    ASSERT_EQ_INT(txn.cmds[0].payload_len, 1);
    ASSERT_EQ_INT(txn.payload_pool[txn.cmds[0].payload_off], 0xAA);
    ASSERT_EQ_INT(txn.cmds[1].ctrl_byte, 0xA2);
    ASSERT_EQ_INT(txn.cmds[1].internal_addr, 0x11);
    ASSERT_EQ_INT(txn.cmds[1].payload_len, 1);
    ASSERT_EQ_INT(txn.payload_pool[txn.cmds[1].payload_off], 0xBB);
    return 0;
}

TEST(parse_read_command)
{
    ASSERT_EQ_INT(do_parse("hA1_h00_h10h_"), 0);
    ASSERT_EQ_INT(txn.count, 1);
    ASSERT_EQ_INT(txn.cmds[0].kind, NEO_CMD_TWI_READ);
    ASSERT_EQ_INT(txn.cmds[0].ctrl_byte, 0xA1);
    ASSERT_EQ_INT(txn.cmds[0].internal_addr, 0);
    ASSERT_EQ_INT(txn.cmds[0].rd_len, 0x10);
    ASSERT_EQ_INT(txn.cmds[0].rd_fmt, NEO_FMT_HEX);
    return 0;
}

TEST(parse_read_zero_bytes)
{
    ASSERT_EQ_INT(do_parse("hA1_h00_"), 0);
    ASSERT_EQ_INT(txn.count, 1);
    ASSERT_EQ_INT(txn.cmds[0].rd_len, 0);
    return 0;
}

TEST(parse_out_command)
{
    ASSERT_EQ_INT(do_parse("out_10101010"), 0);
    ASSERT_EQ_INT(txn.count, 1);
    ASSERT_EQ_INT(txn.cmds[0].kind, NEO_CMD_GPIO_OUT);
    ASSERT_EQ_INT(txn.cmds[0].gpio_byte, 0xAA);
    return 0;
}

TEST(parse_inp_help)
{
    ASSERT_EQ_INT(do_parse("inp"), 0);
    ASSERT_EQ_INT(txn.cmds[0].kind, NEO_CMD_GPIO_IN);

    ASSERT_EQ_INT(do_parse("help"), 0);
    ASSERT_EQ_INT(txn.cmds[0].kind, NEO_CMD_HELP);

    ASSERT_EQ_INT(do_parse("?"), 0);
    ASSERT_EQ_INT(txn.cmds[0].kind, NEO_CMD_HELP);
    return 0;
}

// ---------- Negative cases ----------------------------------------------

TEST(parse_rejects_odd_hex)
{
    ASSERT_TRUE(do_parse("hA0_h00_h123") != 0);   // 3 hex digits
    return 0;
}

TEST(parse_rejects_bad_binary_len)
{
    ASSERT_TRUE(do_parse("hA0_h00_b1010101") != 0); // 7 bits
    return 0;
}

TEST(parse_rejects_bad_start)
{
    ASSERT_TRUE(do_parse("foo") != 0);
    ASSERT_TRUE(do_parse("hB0_h00") != 0);       // bad ctrl byte
    return 0;
}

TEST(parse_rejects_read_overlimit)
{
    ASSERT_TRUE(do_parse("hA1_h00_h3FFh_") != 0); // 0x3FF > 0x3FE
    return 0;
}

TEST(parse_rejects_read_missing_fmt)
{
    ASSERT_TRUE(do_parse("hA1_h00_h01_") != 0);
    return 0;
}

TEST(parse_rejects_ascii_non_alnum)
{
    ASSERT_TRUE(do_parse("hA0_h00_aFoo!_") != 0);
    return 0;
}

TEST(parse_rejects_empty_line)
{
    ASSERT_TRUE(do_parse("") != 0);
    ASSERT_TRUE(do_parse("   ") != 0);
    return 0;
}
