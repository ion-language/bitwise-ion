// @lang: c99

#include "buf.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

void buf__free(void* buf)
{
    BufHeader* hdr = buf__hdr(buf);
    free(hdr);
}

void* buf__grow(void* buf, size_t element_n, size_t element_size)
{
    size_t old_cap=0, old_len=0;
    if (buf) old_cap=buf__hdr(buf)->cap, old_len=buf__hdr(buf)->len;
    size_t needed_cap = old_len + element_n;
    size_t next_cap = 1+2*old_cap;
    size_t new_cap = needed_cap<next_cap? next_cap:needed_cap;
    if (new_cap > old_cap)
    {
        size_t needed_size = new_cap*element_size + offsetof(struct BufHeader, bytes);
        char* memory = (char*)(buf?buf__hdr(buf):NULL);
        memory = realloc(memory, needed_size);
        BufHeader *new_header = (BufHeader*)memory;
        if (buf == NULL) {
            new_header->magic = BufHeader_Magic;
            new_header->element_size = element_size;
            new_header->len = 0;
        }
        assert(new_header->magic == BufHeader_Magic);
        assert(new_header->element_size == element_size);
        new_header->cap = new_cap;
        buf = memory + offsetof(struct BufHeader, bytes);
    }
    return buf;
}

char* buf__printf(char* b, char const* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int char_n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (char_n < 0) perror("vsnprintf:"), exit('c');
    b = buf__grow(b, char_n+1, sizeof *b);
    va_start(args, fmt);
    char_n = vsnprintf(&b[buf_len(b)], char_n+1, fmt, args);
    va_end(args);
    if (char_n >= 0) {
        buf__hdr(b)->len += char_n;
    }
    return b;
}

void test_buf(int argc, char** argv)
{
    char* b = NULL;
    buf__assert(b);
    assert(buf_cap(b) == 0);
    assert(buf_len(b) == 0);
    assert(buf_end(b) == b);
    buf_free(b);

    buf_push(b, 'a');
    assert(buf_len(b) == 1);
    assert(buf_end(b) - b == buf_len(b));
    assert(b[0] == 'a');

    buf_printf(b, "Hello, %s", "World");
    buf_printf(b, "\n");
    buf_printf(b, "%d", 42);
    char const* reference = "aHello, World\n42";
    assert(0 == strcmp(reference, b));
    assert(buf_len(b) == strlen(reference));

    buf_free(b);
    assert(b == NULL);
}