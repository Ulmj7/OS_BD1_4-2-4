#ifndef TEST_MODULE_SUPPORT_H
#define TEST_MODULE_SUPPORT_H
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <kshim.h>
struct output_buffer { char text[65536]; size_t length; };
static void capture(const char *text, void *context) {
    struct output_buffer *out = context;
    size_t n = strlen(text);
    assert(out->length + n < sizeof(out->text));
    memcpy(out->text + out->length, text, n + 1);
    out->length += n;
}
static void reset_output(struct output_buffer *out) {
    out->length = 0; out->text[0] = 0;
    kshim_set_printk_sink(capture, out);
}
static void no_leaks(void) {
    struct kshim_stats s = kshim_get_stats();
    assert(s.allocs == s.frees && s.live == 0 && s.live_bytes == 0);
}
#endif
