// neo_parser.c -- transaction parser / validator.
//
// This module is intentionally free of NEORV32 dependencies so that it can be
// compiled and unit-tested on the host machine (see test/).

#include "neo_parser.h"

// -------------------------------------------------------------------------
// Tiny ctype helpers (locale-free).
// -------------------------------------------------------------------------

// Hex digits are uppercase only so they don't collide with the lowercase
// 'h' / 'b' / 'a' format markers (in particular 'b' and 'a' would otherwise
// be valid hex digits, making format boundaries ambiguous).  This matches
// the canonical example in the project spec.
static inline int is_hex(int c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
}

static inline int hex_val(int c)
{
    if(c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') 
    {
        return c - 'A' + 10;
    }
    return -1;
}

static inline int is_bin(int c) 
{ 
    return c == '0' || c == '1'; 
}

static inline int is_alnum_lat(int c)
{
    return (c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z');
}

// -------------------------------------------------------------------------
// Cursor / error helpers.
// -------------------------------------------------------------------------
typedef struct 
{
    const char* base;       // original string
    const char* p;          // current cursor
    neo_txn_t* out;
} cur_t;

static int err_at(cur_t* c, const char* msg)
{
    c->out->error = msg;
    c->out->error_pos = (int)(c->p - c->base);
    return -1;
}

static inline int eos(cur_t* c) 
{ 
    return *c->p == '\0'; 
}

// -------------------------------------------------------------------------
// Single-byte decoders for each format.  On success advance c->p and write
// the decoded byte into *out.  On failure return -1 with err set.
// -------------------------------------------------------------------------

static int decode_hex_byte(cur_t* c, uint8_t* out)
{
    // Exactly two hex digits.
    if(!is_hex(c->p[0]) || !is_hex(c->p[1])) 
    {
        return err_at(c, "hex byte requires two hex digits");
    }

    *out = (uint8_t)((hex_val(c->p[0]) << 4) | hex_val(c->p[1]));
    c->p += 2;
    return 0;
}

static int decode_bin_byte(cur_t* c, uint8_t* out)
{
    // Exactly 8 binary bits, '_' allowed between bits but not as the very
// first char of the byte and counted as a no-op.
    uint8_t v = 0;
    int bits = 0;
    while(bits < 8)
    {
        char ch = *c->p;
        if(ch == '_') 
        { 
            ++c->p; 
            continue; 
        }

        if(!is_bin(ch))
        {
            return err_at(c, "binary byte requires eight bits");
        }

        v = (uint8_t)((v << 1) | (ch - '0'));
        ++c->p;
        ++bits;
    }

    *out = v;
    return 0;
}

static int decode_ascii_byte(cur_t* c, uint8_t* out)
{
    if(!is_alnum_lat(*c->p))
    {
        return err_at(c, "ascii field requires latin letter or digit");
    }
    *out = (uint8_t)*c->p++;

    return 0;
}

// -------------------------------------------------------------------------
// Lookahead: are we sitting at the start of a new TWI command?  This is the
// only thing that ends a write command's payload.
// -------------------------------------------------------------------------

static int at_cmd_start(const char* p)
{
    if(p[0] != 'h')
    {
        return 0;
    }
    if(p[1] != 'A')
    {
        return 0;
    }
    char d = p[2];

    return (d == '0' || d == '1' || d == '2' || d == '3');
}

// -------------------------------------------------------------------------
// Append a decoded byte to the shared payload pool.
// -------------------------------------------------------------------------

static int push_payload(cur_t* c, neo_cmd_t* cmd, uint8_t b)
{
    neo_txn_t* t = c->out;
    
    if(cmd->payload_len >= NEO_PARSER_PAYLOAD_MAX)
    {
        return err_at(c, "write payload exceeds 1022 bytes");
    }
    if(t->payload_used >= (int)sizeof(t->payload_pool))
    {
        return err_at(c, "transaction payload pool exhausted");
    }

    t->payload_pool[t->payload_used++] = b;
    cmd->payload_len++;

    return 0;
}

// -------------------------------------------------------------------------
// Parse a write command's payload (zero or more data bytes).
// Stops at end-of-input or at the start of the next TWI command.
//
// Format groups inside the payload:
//   h... : sequence of hex bytes (no '_' inside).
//   b... : sequence of binary bytes (8-bit groups; '_' allowed inside).
//   a... : sequence of ASCII letters/digits, terminated by '_'.
//
// '_' between groups is allowed and ignored.  A standalone leading '_'
// (immediately after the ctrl byte / internal addr) is also tolerated.
// -------------------------------------------------------------------------

static int parse_write_payload(cur_t* c, neo_cmd_t* cmd)
{
    while(!eos(c))
    {
        // Could a new command start here?
        if(at_cmd_start(c->p)) return 0;
        char fc = *c->p;

        if(fc == '_')
        {
            ++c->p;
            continue;
        }

        if(fc != 'h' && fc != 'b' && fc != 'a')
        {
            return err_at(c, "expected format selector h/b/a or _");
        }
        ++c->p;     // consume format char

        if(fc == 'h')
        {
            // Read hex bytes until non-hex (or potential next-cmd boundary).
            // Note: at_cmd_start() peeks 'h' so we won't accidentally swallow it.
            int got = 0;
            while(is_hex(*c->p))
            {
                // If we previously saw 'h' which kicks off this run, we must
                // next char is not hex, decode_hex_byte will report it.
                uint8_t b;
                if(decode_hex_byte(c, &b) < 0)
                {
                    return -1;
                }
                if(push_payload(c, cmd, b) < 0) 
                {
                    return -1;
                }
                ++got;
                // After two digits, lookahead: if next is not hex, we yield.
            }
            if(!got) 
            {
                return err_at(c, "empty hex group");
            }
        }
        else if(fc == 'b')
        {
            int got = 0;
            while(is_bin(*c->p) || *c->p == '_')
            {
                // Inside a binary run, '_' is grouping but cannot end a run
                // unless followed by a new format selector or new command.
                // Strategy: try to decode a byte; if the byte starts with '_'
                // and the next non-'_' is not a bit, stop.
                // peek for run end: skip leading '_' and check
                const char *peek = c->p;
                while(*peek == '_') ++peek;
                if(!is_bin(*peek))
                {
                    break;
                }
                uint8_t b;
                if(decode_bin_byte(c, &b) < 0) 
                {
                    return -1;
                }
                if(push_payload(c, cmd, b) < 0) 
                {
                    return -1;
                }
                ++got;
            }

            if(!got) 
            {
                return err_at(c, "empty binary group");
            }
        }
        // 'a'
        else
        {
            int got = 0;
            while(is_alnum_lat(*c->p))
            {
                uint8_t b;
                if(decode_ascii_byte(c, &b) < 0) 
                {
                    return -1;
                }
                if(push_payload(c, cmd, b) < 0) 
                {
                    return -1;
                }
                ++got;
            }
            if(!got)
            {
                return err_at(c, "empty ascii group");
            }
            // ASCII run must be terminated by '_' (or end-of-input / next cmd).
            if(*c->p == '_')
            {
                ++c->p;
            }
            else if(eos(c) || at_cmd_start(c->p))
            {
                // tolerate missing '_' at very end
            }
            else
            {
                return err_at(c, "ascii run must end with '_'");
            }
        }
    }
    return 0;
}

// -------------------------------------------------------------------------
// Parse one byte (any format) for read-command's internal-address slot.
// Tolerates a leading '_' before the format selector.
// -------------------------------------------------------------------------

static int parse_one_byte_any(cur_t* c, uint8_t* out)
{
    while(*c->p == '_')
    {
        ++c->p;
    }
    
    char fc = *c->p;
    if(fc != 'h' && fc != 'b' && fc != 'a') 
    {
        return err_at(c, "expected format selector before internal address");
    }

    ++c->p;
    int rc;

    if(fc == 'h')
    {
        rc = decode_hex_byte (c, out);
    }
    else if(fc == 'b') 
    {
        rc = decode_bin_byte (c, out);
    }
    else 
    {
        rc = decode_ascii_byte(c, out);
    }
    
    if(rc < 0)
    {
        return -1;
    }
    
    // For ASCII, the spec requires '_' termination, but a single byte slot is
    // implicitly delimited by the next syntactic element; we tolerate either.
    if(fc == 'a' && *c->p == '_') 
    {
        ++c->p;
    }
    return 0;
}

// -------------------------------------------------------------------------
// Parse the read-command tail:  [_h<HEX>(h|b|a)] _
// Already consumed: ctrl byte, internal address byte.
// -------------------------------------------------------------------------

static int parse_read_tail(cur_t* c, neo_cmd_t* cmd)
{
    while(*c->p == '_')
    {
        ++c->p;
    }

    // Three valid follow-ups:
    //   1) end-of-input or start-of-next-cmd : zero-byte read
    //   2) explicit h<HEX> then format char then '_' (or end) : non-zero read
    if(eos(c) || at_cmd_start(c->p)) 
    {
        cmd->rd_len = 0;
        cmd->rd_fmt = NEO_FMT_NONE;
        return 0;
    }

    if(*c->p != 'h') 
    {
        return err_at(c, "expected 'h<count>' or end of read command");
    }
    ++c->p;     // consume 'h'

    // 1..3 hex digits for the count.  This is a number, not a byte sequence,
    // so the multiple-of-2 rule does not apply.
    int n = 0, digits = 0;
    while(is_hex(*c->p) && digits < 3) 
    {
        n = (n << 4) | hex_val(*c->p);
        ++c->p;
        ++digits;
    }
    if(digits == 0)
    {
        return err_at(c, "missing read count digits");
    }
    if(n > 0x3FE)
    {
        return err_at(c, "read count exceeds h3FE");
    }
    cmd->rd_len = (uint16_t)n;

    // Format selector char.
    char ff = *c->p;
    if(ff != 'h' && ff != 'b' && ff != 'a') 
    {
        if(n == 0) 
        {
            cmd->rd_fmt = NEO_FMT_NONE;
        } 
        else 
        {
            return err_at(c, "missing display format for read");
        }
    } 
    else 
    {
        cmd->rd_fmt = (neo_fmt_t)ff;
        ++c->p;
    }

    // Closing '_' is required by spec.
    if(*c->p != '_') 
    {
        return err_at(c, "read command must end with '_'");
    }
    ++c->p;
    return 0;
}

// -------------------------------------------------------------------------
// Parse one TWI command.  Cursor must be at 'h'.
// -------------------------------------------------------------------------

static int parse_one_twi_cmd(cur_t* c)
{
    if(c->out->count >= NEO_PARSER_MAX_CMDS) 
    {
        return err_at(c, "too many commands in transaction");
    }
    if(!at_cmd_start(c->p)) 
    {
        return err_at(c, "command must start with hA0/hA1/hA2/hA3");
    }
    // Consume 'h' then read ctrl byte (always exactly 2 hex chars).
    ++c->p; // 'h'
    uint8_t ctrl;
    if(decode_hex_byte(c, &ctrl) < 0)
    {
        return -1;
    }

    neo_cmd_t *cmd = &c->out->cmds[c->out->count];
    cmd->ctrl_byte    = ctrl;
    cmd->payload_off  = (uint16_t)c->out->payload_used;
    cmd->payload_len  = 0;
    cmd->rd_len       = 0;
    cmd->rd_fmt       = NEO_FMT_NONE;

    int is_read = (ctrl == 0xA1 || ctrl == 0xA3);
    cmd->kind = is_read ? NEO_CMD_TWI_READ : NEO_CMD_TWI_WRITE;

    // Both flavors take an internal-address byte next.
    if(parse_one_byte_any(c, &cmd->internal_addr) < 0)
    {
        return -1;
    }

    if(is_read)
    {
        if(parse_read_tail(c, cmd) < 0)
        {
            return -1;
        }
    } 
    else 
    {
        if(parse_write_payload(c, cmd) < 0)
        {
            return -1;
        }
    }

    c->out->count++;
    return 0;
}

// -------------------------------------------------------------------------
// Special command parsers.
// -------------------------------------------------------------------------

static int strprefix(const char* s, const char* pfx, const char** end)
{
    while(*pfx) 
    {
        if(*s++ != *pfx++) return 0;
    }

    if(end)
    {
        *end = s;
    }
    return 1;
}

static int strtail_empty(const char* s)
{
    while(*s == ' ' || *s == '\t')
    {
        ++s;
    }

    return *s == '\0';
}

static int parse_special(cur_t* c)
{
    const char *p = c->p;
    const char *q;

    if(strprefix(p, "out_", &q))
    {
        // expect exactly 8 binary digits then end
        uint8_t v = 0;
        int i;
        for(i = 0; i < 8; ++i)
        {
            if(q[i] != '0' && q[i] != '1')
            {
                c->p = q + i;
                return err_at(c, "out_ requires 8 binary digits");
            }
            v = (uint8_t)((v << 1) | (q[i] - '0'));
        }

        c->p = q + 8;
        if(!strtail_empty(c->p))
        {
            return err_at(c, "extra characters after out_xxxxxxxx");
        }
        neo_cmd_t *cmd = &c->out->cmds[c->out->count++];
        cmd->kind = NEO_CMD_GPIO_OUT;
        cmd->gpio_byte = v;
        
        return 0;
    }
    if(strprefix(p, "inp", &q) && strtail_empty(q))
    {
        c->out->cmds[c->out->count++] = (neo_cmd_t){ .kind = NEO_CMD_GPIO_IN };
        c->p = q;
        return 0;
    }
    if((strprefix(p, "help", &q) && strtail_empty(q)) || (p[0] == '?' && strtail_empty(p + 1)))
    {
        c->out->cmds[c->out->count++] = (neo_cmd_t) { 
            .kind = NEO_CMD_HELP 
        };
        c->p = (p[0] == '?') ? (p + 1) : q;
        return 0;
    }
    return err_at(c, "unknown command");
}

// -------------------------------------------------------------------------
// Public entry point.
// -------------------------------------------------------------------------

int neo_parse_transaction(const char* line, neo_txn_t* out)
{
    // zero-init output.
    int i;
    out->count       = 0;
    out->payload_used = 0;
    out->error       = 0;
    out->error_pos   = 0;
    for(i = 0; i < NEO_PARSER_MAX_CMDS; ++i)
    {
        neo_cmd_t z = {0};
        out->cmds[i] = z;
    }

    if(!line)
    {
        out->error = "null input";
        return -1;
    }

    // skip leading whitespace
    while(*line == ' ' || *line == '\t')
    {
        ++line;
    }
    if(*line == '\0') 
    {
        out->error = "empty transaction"; 
        return -1; 
    }

    cur_t c = { 
        .base = line, 
        .p = line, 
        .out = out 
    };

    // TWI chain vs special command?
    if(at_cmd_start(c.p))
    {
        while(!eos(&c))
        {
            // tolerate stray '_' between commands
            while(*c.p == '_')
            {
                ++c.p;
            }
            if(eos(&c))
            {
                break;
            }
            if(!at_cmd_start(c.p))
            {
                return err_at(&c, "garbage after command");
            }
            if(parse_one_twi_cmd(&c) < 0)
            {
                return -1;
            }
        }
        return 0;
    }

    return parse_special(&c);
}
