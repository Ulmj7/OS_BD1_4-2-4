#include "module_support.h"
#include "reference_collatz.h"
#include "../src/kernel/collatz.c"

static void valid(const struct collatz_reference *reference, struct output_buffer *out) {
    reset_output(out);
    struct kshim_stats before = kshim_get_stats();
    assert(kshim_run_init() == 0);
    struct collatz_node *node;
    size_t i = 0, offset = 0;
    unsigned long long peak = 0;
    char expected[65536];
    list_for_each_entry(node, &collatz_list, list) {
        assert(i < reference->terms && node->value == reference->values[i]);
        assert(node->list.next->prev == &node->list && node->list.prev->next == &node->list);
        if (node->value > peak) peak = node->value;
        int n = snprintf(expected + offset, sizeof(expected) - offset,
                         "collatz[%zu]: value=%llu\n", i, reference->values[i]);
        assert(n > 0 && (size_t)n < sizeof(expected) - offset);
        offset += (size_t)n; ++i;
    }
    assert(i == reference->terms && peak == reference->peak);
    assert(strcmp(out->text, expected) == 0);
    struct kshim_stats during = kshim_get_stats();
    assert(during.allocs - before.allocs == reference->terms);
    assert(during.live == reference->terms && during.live_bytes == reference->terms * sizeof(struct collatz_node));
    kshim_run_exit();
    assert(strcmp(out->text, expected) == 0 && list_empty(&collatz_list));
    assert(kshim_get_stats().frees - before.frees == reference->terms);
    no_leaks();
}

int main(void) {
    struct output_buffer out;
    assert(start == 25);
    valid(&references[2], &out); /* default parameter, before calling its setter */
    assert(kshim_set_param("missing", 25) != 0);
    assert(kshim_set_param("start", 2147483648LL) != 0);
    size_t failures = 0;
    for (size_t case_no = 0; case_no < sizeof(references) / sizeof(*references); ++case_no) {
        const struct collatz_reference *reference = &references[case_no];
        assert(kshim_set_param("start", reference->input) == 0);
        assert(start == reference->input);
        kshim_fail_alloc_after(-1); valid(reference, &out);
        for (size_t fail = 0; fail < reference->terms; ++fail) {
            reset_output(&out);
            struct kshim_stats before = kshim_get_stats();
            kshim_fail_alloc_after((long long)fail);
            assert(kshim_run_init() == -ENOMEM);
            assert(strcmp(out.text, "collatz: kmalloc failed\n") == 0);
            assert(list_empty(&collatz_list));
            struct kshim_stats after = kshim_get_stats();
            assert(after.allocs - before.allocs == fail && after.frees - before.frees == fail);
            no_leaks(); kshim_run_exit(); no_leaks(); ++failures;
        }
        /* With exactly terms successful allocations, initialization succeeds. */
        kshim_fail_alloc_after((long long)reference->terms); valid(reference, &out);
        printf("PASS collatz module start=%d terms=%zu steps=%zu peak=%llu, all ENOMEM positions\n",
               reference->input, reference->terms, reference->terms - 1, reference->peak);
    }
    const int invalid[] = {0, -5};
    kshim_fail_alloc_after(-1);
    for (size_t i = 0; i < 2; ++i) {
        assert(kshim_set_param("start", invalid[i]) == 0);
        reset_output(&out);
        struct kshim_stats before = kshim_get_stats();
        assert(kshim_run_init() == -EINVAL);
        assert(strcmp(out.text, "collatz: start must be positive\n") == 0);
        assert(kshim_get_stats().allocs == before.allocs);
        assert(list_empty(&collatz_list)); no_leaks();
    }
    kshim_set_printk_sink(NULL, NULL);
    printf("PASS real Collatz module: default 25, 7 references, %zu injected failures, 2 invalid starts; zero leaks\n", failures);
    return 0;
}
