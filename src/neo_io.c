// neo_io.c -- minimal printf/snprintf/line-input without <stdlib.h> / FP.
//
// Designed for tiny RISC-V targets: no malloc, no statics other than the
// pluggable backend pointer, no recursion.  All conversions go through a
// single 8-byte stack buffer.  Integer divisions are kept to a minimum.

#include "neo_io.h"


#define NEO_FLAG_LEFT   (1u << 0)
#define NEO_FLAG_ZERO   (1u << 1)
#define NEO_FLAG_NEG    (1u << 2)   // the rendered number is negative

// -------------------------------------------------------------------------
// Pluggable backend
// -------------------------------------------------------------------------

static const neo_io_backend_t *s_io = 0;

void neo_io_set_backend(const neo_io_backend_t *b)
{ 
    s_io = b; 
}

int neo_putc(int c)
{
    if(s_io && s_io->putc)
    {
        return s_io->putc(c, s_io->ctx);
    }
    
    return -1;
}

int neo_puts(const char *s)
{
    if(!s) 
    {
        return 0;
    }

    int n = 0;

    while(*s)
    {
        if(neo_putc((unsigned char)*s++) < 0) 
        {
            return -1;
        }
        ++n;
    }
    return n;
}

int neo_getc(void)
{
    if(s_io && s_io->getc) 
    {
        return s_io->getc(s_io->ctx);
    }

    return -1;
}

// -------------------------------------------------------------------------
// Internal "print sink" abstraction so we can share the formatter between
// neo_vprintf() (writes to backend) and neo_vsnprintf() (writes to buffer).
// -------------------------------------------------------------------------
typedef struct neo_sink neo_sink_t;
struct neo_sink
{
    void(*emit)(neo_sink_t *self, char c);
    int produced;          // total chars the format would produce
    // buffer-mode fields
    char* buf;
    size_t cap;               // total buffer capacity (incl. NUL slot)
    size_t pos;               // next write index
    // backend-mode fields
    neo_putc_fn out;
    void* ctx;
};

static void emit_buf(neo_sink_t* s, char c)
{
    if(s->buf && s->pos + 1 < s->cap) 
    {
        s->buf[s->pos++] = c;
    }
    s->produced++;
}

static void emit_out(neo_sink_t* s, char c)
{
    if(s->out) 
    {
        s->out((unsigned char)c, s->ctx);
    }
    s->produced++;
}

static inline void sink_putc(neo_sink_t *s, char c) { s->emit(s, c); }

// -------------------------------------------------------------------------
// Integer to ASCII (no division-by-variable for bases 2/8/16).
// Buffer must hold at least 65 chars (binary 64-bit + sign).
// Returns number of chars written into 'out' (no NUL).
// -------------------------------------------------------------------------

static int u64_to_str(uint64_t v, unsigned base, int upper, char *out)
{
    static const char dl[] = "0123456789abcdef";
    static const char du[] = "0123456789ABCDEF";
    const char *digits = upper ? du : dl;
    char tmp[65];
    int  n = 0;

    if(v == 0) 
    { 
        tmp[n++] = '0'; 
    }
    else if(base == 10) 
    {
        while(v) 
        { 
            tmp[n++] = digits[v % 10]; 
            v /= 10; 
        }
    } 
    else if(base == 16) 
    {
        while(v) 
        { 
            tmp[n++] = digits[v & 0xF]; 
            v >>= 4; 
        }
    } 
    else if(base == 8) 
    {
        while(v) 
        { 
            tmp[n++] = digits[v & 0x7]; 
            v >>= 3; 
        }
    } 
    else if(base == 2) 
    {
        while(v) 
        { 
            tmp[n++] = digits[v & 0x1]; 
            v >>= 1; 
        }
    } 
    else 
    {
        while(v) 
        { 
            tmp[n++] = digits[v % base]; 
            v /= base; 
        }
    }

    // reverse
    int i;
    for(i = 0; i < n; ++i) 
    {
        out[i] = tmp[n - 1 - i];
    }
    return n;
}

// -------------------------------------------------------------------------
// Formatter core.  Reads format string and pumps chars into 'sink'.
// -------------------------------------------------------------------------
static int parse_uint(const char** pp)
{
    int v = 0;
    const char *p = *pp;
    while(*p >= '0' && *p <= '9') 
    { 
        v = v*10 + (*p - '0'); 
        ++p; 
    }
    *pp = p;
    return v;
}

static void emit_padded(neo_sink_t* s, const char* body, int blen,
                        int width, int flags, int prec_digits)
{
    // prec_digits = minimum number of digits to render (only meaningful for
    // numeric output, where caller passed already-stripped digit body).
    int pad_zero = 0;
    if(prec_digits > blen) 
    {
        pad_zero = prec_digits - blen;
    }

    int total = blen + pad_zero;
    int pad_space = (width > total) ? (width - total) : 0;

    // When precision is given, '0' flag is ignored (C99).
    int zero_pad_via_width = 0;
    if((flags & NEO_FLAG_ZERO) && !(flags & NEO_FLAG_LEFT) && prec_digits < 0) 
    {
        zero_pad_via_width = pad_space;
        pad_space = 0;
    }

    if(!(flags & NEO_FLAG_LEFT)) 
    {
        while(pad_space-- > 0) sink_putc(s, ' ');
    }
    while(zero_pad_via_width-- > 0)
    {
        sink_putc(s, '0');
    }
    while(pad_zero-- > 0) 
    {
        sink_putc(s, '0');
    }
    
    while(blen-- > 0) 
    {
        sink_putc(s, *body++);
    }
    if(flags & NEO_FLAG_LEFT) 
    {
        while(pad_space-- > 0) sink_putc(s, ' ');
    }
}

static int format_core(neo_sink_t* s, const char* fmt, va_list ap)
{
    while(*fmt) 
    {
        char c = *fmt++;
        if(c != '%') 
        {
            sink_putc(s, c); 
            continue; 
        }

        // flags
        unsigned flags = 0;
        for(;;) 
        {
            if(*fmt == '-')      
            { 
                flags |= NEO_FLAG_LEFT; 
                ++fmt; 
            }
            else if(*fmt == '0') 
            { 
                flags |= NEO_FLAG_ZERO; 
                ++fmt; 
            }
            else break;
        }

        // width
        int width = 0;
        if(*fmt == '*') 
        { 
            width = va_arg(ap, int); 
            ++fmt;
            if(width < 0) 
            { 
                flags |= NEO_FLAG_LEFT; 
                width = -width; 
            } 
        }
        else width = parse_uint(&fmt);

        // precision
        int prec = -1;
        if(*fmt == '.') 
        {
            ++fmt;
            if(*fmt == '*') 
            { 
                prec = va_arg(ap, int); 
                ++fmt; 
                if(prec < 0) 
                { 
                    prec = -1; 
                } 
            }
            else prec = parse_uint(&fmt);
        }

        // length modifier
        int len_l = 0;            // 0=int, 1=long, 2=long long
        if(*fmt == 'l') 
        { 
            ++fmt; 
            len_l = 1;
            if(*fmt == 'l') 
            { 
                ++fmt; 
                len_l = 2; 
            } 
        }
        else if(*fmt == 'h') 
        { 
            ++fmt; 
            if(*fmt == 'h') 
            { 
                ++fmt; 
            } 
        }
        else if(*fmt == 'z') 
        { 
            ++fmt; 
        }

        char spec = *fmt ? *fmt++ : 0;
        char numbuf[68];
        int  nlen = 0;
        int  prec_digits = -1;
        int  is_signed = 0;
        unsigned base = 10;
        int  upper = 0;

        switch(spec) 
        {
        case 'c': 
        {
            char ch = (char)va_arg(ap, int);
            numbuf[0] = ch; nlen = 1;
            emit_padded(s, numbuf, nlen, width, flags, -1);
            continue;
        }
        case 's': 
        {
            const char *str = va_arg(ap, const char *);
            if(!str) 
            {
                str = "(null)";
            }
            int slen = 0;
            while(str[slen] && (prec < 0 || slen < prec)) 
            {
                ++slen;
            }
            emit_padded(s, str, slen, width, flags, -1);
            continue;
        }
        case '%': 
        {
            sink_putc(s, '%'); 
            continue;
        }
        case 'd': 
        case 'i': 
        {
            is_signed = 1; 
            base = 10; 
            break;
        }
        case 'u': 
        {
            base = 10; 
            break;
        }
        case 'o': 
        {
            base = 8;  
            break;
        }
        case 'x': 
        {
            base = 16; 
            upper = 0; 
            break;
        }
        case 'X': 
        {
            base = 16; 
            upper = 1; 
            break;
        }
        case 'b': 
        {
            base = 2;  
            break;
        }
        case 'p': 
        {
            uintptr_t v = (uintptr_t)va_arg(ap, void *);
            numbuf[0] = '0'; numbuf[1] = 'x';
            int hl = u64_to_str(v, 16, 0, numbuf + 2);
            // zero-pad to ptr width (8 for 32-bit, 16 for 64-bit)
            int ptr_w = (sizeof(uintptr_t) == 4) ? 8 : 16;
            if(hl < ptr_w) 
            {
                // shift right and zero-pad
                int shift = ptr_w - hl;
                int i;
                for(i = hl - 1; i >= 0; --i) 
                {
                    numbuf[2 + i + shift] = numbuf[2 + i];
                }
                for(i = 0; i < shift; ++i) 
                {
                    numbuf[2 + i] = '0';
                }
                hl = ptr_w;
            }
            nlen = 2 + hl;
            emit_padded(s, numbuf, nlen, width, flags, -1);
            continue;
        }
        default:
        {
            // unknown spec -> emit as-is
            sink_putc(s, '%');
            if(spec) 
            {
                sink_putc(s, spec);
            }
            continue;
        }
        }

        // Integer path.
        uint64_t uv;
        if(is_signed) 
        {
            int64_t sv;
            if(len_l == 2) 
            {
                sv = va_arg(ap, long long);
            }
            else if(len_l == 1) 
            {
                sv = va_arg(ap, long);
            }
            else                 
            {
                sv = va_arg(ap, int);
            }
            if(sv < 0) 
            {
                flags |= NEO_FLAG_NEG; 
                uv = (uint64_t)(-(sv + 1)) + 1ULL; 
            }
            else        
            {
                uv = (uint64_t)sv; 
            }
        }
        else 
        {
            if(len_l == 2) 
            {
                uv = va_arg(ap, unsigned long long);
            }
            else if(len_l == 1) 
            {
                uv = va_arg(ap, unsigned long);
            }
            else                 
            {
                uv = va_arg(ap, unsigned);
            }
        }

        nlen = u64_to_str(uv, base, upper, numbuf);

        // Apply precision (min digit count).  C99: prec=0 with value 0 -> "".
        if(prec >= 0) 
        {
            if(prec == 0 && uv == 0) 
            {
                nlen = 0;
            }
            prec_digits = prec;
        }

        // Sign
        char signch = 0;
        if(flags & NEO_FLAG_NEG) 
        {
            signch = '-';
        }

        // Compose body in numbuf2 to keep emit_padded simple
        char numbuf2[70];
        int  pos = 0;
        if(signch) 
        {
            numbuf2[pos++] = signch;
        }
        int i;
        for(i = 0; i < nlen; ++i) 
        {
            numbuf2[pos++] = numbuf[i];
        }

        // For signed numbers with zero-padding-via-width, the sign must come
        // before the zero pad.  emit_padded() handles plain zero-pad only when
        // prec_digits<0; if we have a sign, we adjust by injecting zeros into
        // the body using prec_digits.
        if((flags & NEO_FLAG_NEG) && (flags & NEO_FLAG_ZERO)
            && !(flags & NEO_FLAG_LEFT) && prec_digits < 0 && width > pos) 
        {
            int extra = width - pos;
            // shift digits right by 'extra'
            int j;
            for(j = pos - 1; j >= 1; --j) 
            {
                numbuf2[j + extra] = numbuf2[j];
            }
            for(j = 0; j < extra; ++j) 
            {
                numbuf2[1 + j] = '0';
            }
            pos += extra;
            flags &= ~NEO_FLAG_ZERO;
        }

        emit_padded(s, numbuf2, pos, width, flags, prec_digits);
    }

    return s->produced;
}

// -------------------------------------------------------------------------
// Public formatter entry points
// -------------------------------------------------------------------------

int neo_vfprintf(neo_putc_fn out, void *ctx, const char *fmt, va_list ap)
{
    neo_sink_t s = {0};
    s.emit = emit_out;
    s.out  = out;
    s.ctx  = ctx;
    return format_core(&s, fmt, ap);
}

int neo_vprintf(const char *fmt, va_list ap)
{
    if (!s_io || !s_io->putc) return 0;
    return neo_vfprintf(s_io->putc, s_io->ctx, fmt, ap);
}

int neo_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = neo_vprintf(fmt, ap);
    va_end(ap);
    return n;
}

int neo_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    neo_sink_t s = {0};
    s.emit = emit_buf;
    s.buf  = buf;
    s.cap  = size;
    int n = format_core(&s, fmt, ap);
    if(buf && size > 0) 
    {
        size_t end = (s.pos < size) ? s.pos : (size - 1);
        buf[end] = '\0';
    }
    return n;
}

int neo_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = neo_vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

// -------------------------------------------------------------------------
// Line input
// -------------------------------------------------------------------------

int neo_read_line(char *buf, size_t max, int echo)
{
    if(!buf || max == 0) return 0;
    size_t n = 0;
    while(n + 1 < max) 
    {
        int c = neo_getc();
        if(c < 0) break;                          // EOF
        if(c == '\r' || c == '\n') {              // end of line
            if(echo) neo_putc('\n');
            break;
        }
        if(c == 0x08 || c == 0x7F) {              // backspace / DEL
            if(n > 0) 
            {
                --n;
                if(echo) 
                {
                    neo_putc(0x08); neo_putc(' '); neo_putc(0x08);
                }
            }
            continue;
        }
        buf[n++] = (char)c;
        if(echo) 
        {
            neo_putc(c);
        }
    }
    buf[n] = '\0';
    return (int)n;
}

