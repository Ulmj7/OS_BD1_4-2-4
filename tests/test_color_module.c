#include "module_support.h"
#include "../src/kernel/color_list.c"

static const char expected[] =
    "color[0]: red=138, green=43, blue=226\n"
    "color[1]: red=255, green=0, blue=0\n"
    "color[2]: red=0, green=255, blue=0\n"
    "color[3]: red=0, green=0, blue=255\n";

static void valid(struct output_buffer *out) {
    reset_output(out);
    struct kshim_stats before = kshim_get_stats();
    assert(kshim_run_init() == 0);
    assert(strcmp(out->text, expected) == 0);
    const int rgb[4][3] = {{138,43,226}, {255,0,0}, {0,255,0}, {0,0,255}};
    struct color *node; size_t i = 0;
    list_for_each_entry(node, &color_list, list) {
        assert(i < 4);
        assert(node->red == rgb[i][0] && node->green == rgb[i][1] && node->blue == rgb[i][2]);
        assert(node->list.next->prev == &node->list && node->list.prev->next == &node->list);
        ++i;
    }
    assert(i == 4);
    struct kshim_stats during = kshim_get_stats();
    assert(during.allocs - before.allocs == 4 && during.live == 4);
    assert(during.live_bytes == 4 * sizeof(struct color));
    kshim_run_exit();
    assert(strcmp(out->text, expected) == 0 && list_empty(&color_list));
    assert(kshim_get_stats().frees - before.frees == 4);
    no_leaks();
}

int main(void) {
    struct output_buffer out;
    valid(&out);
    for (int fail = 0; fail < 4; ++fail) {
        reset_output(&out);
        struct kshim_stats before = kshim_get_stats();
        kshim_fail_alloc_after(fail);
        assert(kshim_run_init() == -ENOMEM);
        assert(strcmp(out.text, "color: kmalloc failed\n") == 0);
        assert(list_empty(&color_list));
        struct kshim_stats after = kshim_get_stats();
        assert(after.allocs - before.allocs == (size_t)fail);
        assert(after.frees - before.frees == (size_t)fail);
        no_leaks();
        kshim_run_exit(); no_leaks();
    }
    kshim_fail_alloc_after(4); valid(&out);
    kshim_fail_alloc_after(-1); kshim_set_printk_sink(NULL, NULL);
    puts("PASS real color module: exact printk, 4 colors, violet RGB, 4 ENOMEM positions + success boundary, no leaks");
    return 0;
}
