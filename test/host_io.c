#include <stdio.h>
#include <string.h>

#include "host_io.h"

// --- Captured backend (memory buffers, used by unit tests) ----------------

#define HOST_OUT_MAX  (1024 * 16)
static char        s_out[HOST_OUT_MAX];
static size_t      s_out_n;
static const char *s_in_cur;

static int cap_putc(int c, void *ctx)
{
    (void)ctx;
    if(s_out_n + 1 < sizeof(s_out)) 
    {
        s_out[s_out_n++] = (char)c;
    }
    s_out[s_out_n] = '\0';
    return c;
}

static int cap_getc(void *ctx)
{
    (void)ctx;
    if(!s_in_cur || *s_in_cur == '\0') 
    {
        return -1;
    }
    return (unsigned char)*s_in_cur++;
}

static const neo_io_backend_t s_cap_be = 
{
    .putc = cap_putc,
    .getc = cap_getc,
    .ctx  = 0,
};

void host_io_begin(void)
{
    s_out_n = 0;
    s_out[0] = '\0';
    s_in_cur = "";
    neo_io_set_backend(&s_cap_be);
}

void host_io_set_input(const char *s)       
{ 
    s_in_cur = s ? s : ""; 
}
const char *host_io_output(void)             
{ 
    return s_out; 
}
size_t      host_io_output_len(void)         
{ 
    return s_out_n; 
}
void        host_io_install(void)            
{ 
    neo_io_set_backend(&s_cap_be); 
}

// --- Stdio backend (interactive) -----------------------------------------

static int std_putc(int c, void *ctx) 
{ 
    (void)ctx; 
    return fputc(c, stdout); 
}
static int std_getc(void *ctx)        
{ 
    (void)ctx; 
    int c = fgetc(stdin); 
    return c == EOF ? -1 : c; 
}

static const neo_io_backend_t s_std_be = 
{
    .putc = std_putc,
    .getc = std_getc,
    .ctx  = 0,
};

void host_io_install_stdio(void) 
{ 
    neo_io_set_backend(&s_std_be); 
}

