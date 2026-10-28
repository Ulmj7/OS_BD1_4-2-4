#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <kds_core.h>

static char *execute(struct kds_context *ctx, const char *cmd) {
    char *text = NULL; size_t size = 0;
    FILE *out = open_memstream(&text, &size);
    assert(out);
    kds_execute(ctx, cmd, out);
    assert(fclose(out) == 0);
    assert(size > 0 && text[size - 1] == '\n');
    assert(strchr(text, '\n') == text + size - 1);
    return text;
}

static char *success(struct kds_context *ctx, const char *cmd) {
    char *text = execute(ctx, cmd);
    if (strncmp(text, "{\"ok\":true,", 11)) fprintf(stderr, "unexpected reply for %s: %s", cmd, text);
    assert(strncmp(text, "{\"ok\":true,", 11) == 0);
    for (size_t i = 0; i < 4; ++i) assert(kds_verify(&ctx->lists[i]).ok);
    return text;
}

static void run(struct kds_context *ctx, const char *cmd) { free(success(ctx, cmd)); }

static void error(struct kds_context *ctx, const char *cmd, const char *code) {
    char *before = success(ctx, "state");
    char *text = execute(ctx, cmd);
    char needle[80];
    snprintf(needle, sizeof(needle), "\"code\":\"%s\"", code);
    if (!strstr(text, needle)) fprintf(stderr, "expected %s for %s: %s", code, cmd, text);
    assert(strncmp(text, "{\"ok\":false,", 12) == 0 && strstr(text, needle));
    assert(strcmp(strstr(before, "\"state\":"), strstr(text, "\"state\":")) == 0);
    free(before); free(text);
}

static void *node_at(struct kds_list *list, size_t index) {
    struct list_head *p = list->head.next;
    assert(index < list->count);
    while (index--) p = p->next;
    if (list->is_color) return list_entry(p, struct color, list);
    return list_entry(p, struct kds_value, list);
}

static void check_values(struct kds_list *list, const long long *expected, size_t count) {
    assert(list->count == count && list->registered == count);
    struct kds_invariant inv = kds_verify(list);
    assert(inv.ok && inv.forward == count && inv.backward == count);
    for (size_t i = 0; i < count; ++i) assert(((struct kds_value *)node_at(list, i))->value == expected[i]);
}

static void effect(const char *reply, const char *name, uint64_t first) {
    char needle[100];
    snprintf(needle, sizeof(needle), "\"%s\":[%" PRIu64 "]", name, first);
    assert(strstr(reply, needle));
}

static void commands(void) {
    struct kds_context ctx;
    assert(kds_init(&ctx, 2048) == 0);
    run(&ctx, "help"); run(&ctx, "state"); run(&ctx, "check");
    run(&ctx, "color add_tail red=1 green=2 blue=3");
    struct color *a = node_at(&ctx.lists[0], 0);
    uint64_t id = kshim_serial_of(a);
    assert(a->red == 1 && a->green == 2 && a->blue == 3);
    run(&ctx, "color add_head red=4 green=5 blue=6");
    assert(((struct color *)node_at(&ctx.lists[0], 0))->red == 4);
    assert(node_at(&ctx.lists[0], 1) == a);
    run(&ctx, "color insert index=0 red=1 green=8 blue=9");
    assert(ctx.lists[0].count == 3 && node_at(&ctx.lists[0], 2) == a);
    struct color *middle = node_at(&ctx.lists[0], 1);
    assert(middle->red == 1 && middle->green == 8 && middle->blue == 9);
    char *reply = success(&ctx, "color search red=1 green=2");
    effect(reply, "matched", id); free(reply);
    run(&ctx, "color traverse");
    run(&ctx, "color delete index=0");
    run(&ctx, "color delete red=1");
    assert(ctx.lists[0].count == 0);
    run(&ctx, "color example");
    assert(ctx.lists[0].count == 4);
    const int expected[4][3] = {{138,43,226}, {255,0,0}, {0,255,0}, {0,0,255}};
    for (size_t i = 0; i < 4; ++i) {
        struct color *c = node_at(&ctx.lists[0], i);
        assert(c->red == expected[i][0] && c->green == expected[i][1] && c->blue == expected[i][2]);
    }
    run(&ctx, "color clear");
    run(&ctx, "stack push value=10"); run(&ctx, "stack push value=-2");
    const long long stack[] = {-2, 10}; check_values(&ctx.lists[2], stack, 2);
    id = kshim_serial_of(node_at(&ctx.lists[2], 0));
    reply = success(&ctx, "stack peek"); effect(reply, "matched", id); free(reply);
    reply = success(&ctx, "stack pop"); effect(reply, "removed", id); free(reply);
    check_values(&ctx.lists[2], stack + 1, 1);
    run(&ctx, "queue enqueue value=10"); run(&ctx, "queue enqueue value=-2");
    const long long queue[] = {10, -2}; check_values(&ctx.lists[3], queue, 2);
    id = kshim_serial_of(node_at(&ctx.lists[3], 0));
    reply = success(&ctx, "queue peek"); effect(reply, "matched", id); free(reply);
    reply = success(&ctx, "queue dequeue"); effect(reply, "removed", id); free(reply);
    check_values(&ctx.lists[3], queue + 1, 1);
    run(&ctx, "collatz run start=25");
    const long long collatz25[] = {25,76,38,19,58,29,88,44,22,11,34,17,52,26,13,40,20,10,5,16,8,4,2,1};
    check_values(&ctx.lists[1], collatz25, 24);
    id = kshim_serial_of(node_at(&ctx.lists[1], 6));
    reply = success(&ctx, "collatz search value=88"); effect(reply, "matched", id); free(reply);
    run(&ctx, "collatz traverse");
    reply = success(&ctx, "collatz delete value=88"); effect(reply, "removed", id); free(reply);
    assert(ctx.lists[1].count == 23);
    run(&ctx, "collatz clear");
    error(&ctx, "", "E_SYNTAX");
    error(&ctx, "no_such_command", "E_UNKNOWN_CMD");
    error(&ctx, "color add_tail red=1 green=2 blue=3 extra=1", "E_UNKNOWN_ARG");
    error(&ctx, "color add_tail red=1 green=2 blue=3 red=4", "E_SYNTAX");
    error(&ctx, "color add_tail red=1 green=2", "E_MISSING_ARG");
    error(&ctx, "color add_tail red=abc green=2 blue=3", "E_BAD_VALUE");
    error(&ctx, "color add_tail red=256 green=2 blue=3", "E_RANGE");
    error(&ctx, "color insert index=-1 red=1 green=2 blue=3", "E_RANGE");
    error(&ctx, "collatz search value=99999", "E_NOT_FOUND");
    error(&ctx, "collatz run start=2147483648", "E_RANGE");
    error(&ctx, "collatz run start=0", "E_RANGE");
    error(&ctx, "collatz run start=-1", "E_RANGE");
    char boundary[514]; memset(boundary, ' ', 512); memcpy(boundary, "state", 5); boundary[512] = 0;
    run(&ctx, boundary); boundary[512] = ' '; boundary[513] = 0;
    error(&ctx, boundary, "E_SYNTAX");
    run(&ctx, "reset");
    error(&ctx, "stack pop", "E_EMPTY"); error(&ctx, "queue dequeue", "E_EMPTY");
    kds_destroy(&ctx);
}

static void allocation_and_limits(void) {
    struct kds_context ctx;
    assert(kds_init(&ctx, 3) == 0);
    run(&ctx, "stack push value=1"); run(&ctx, "stack push value=2"); run(&ctx, "stack push value=3");
    error(&ctx, "stack push value=4", "E_FULL");
    error(&ctx, "color example", "E_FULL"); error(&ctx, "collatz run start=25", "E_FULL");
    run(&ctx, "collatz run start=1"); error(&ctx, "collatz run start=15", "E_FULL");
    kds_destroy(&ctx);
    assert(kds_init(&ctx, 2048) == 0);
    run(&ctx, "collatz run start=1"); run(&ctx, "color add_tail red=1 green=2 blue=3");
    const char *commands[] = {"color example", "collatz run start=25", "color add_head red=4 green=5 blue=6", "stack push value=8", "queue enqueue value=9"};
    const int allocations[] = {4,24,1,1,1};
    for (size_t i = 0; i < sizeof(commands) / sizeof(*commands); ++i) {
        for (int fail = 0; fail < allocations[i]; ++fail) {
            kshim_fail_alloc_after(fail);
            error(&ctx, commands[i], "E_NOMEM");
            assert(kshim_get_stats().live == 2);
            kshim_fail_alloc_after(-1);
        }
    }
    run(&ctx, "inject_fail after=0"); error(&ctx, "queue enqueue value=1", "E_NOMEM");
    run(&ctx, "inject_fail off"); run(&ctx, "queue enqueue value=1");
    run(&ctx, "reset"); assert(kshim_get_stats().live == 0);
    kds_destroy(&ctx);
}

static void corrupted_lists(void) {
    struct kds_context ctx;
    assert(kds_init(&ctx, 8) == 0);
    for (int i = 0; i < 3; ++i) run(&ctx, "color add_tail red=1 green=2 blue=3");
    struct kds_list *l = &ctx.lists[0];
    struct list_head *links[4] = {&l->head};
    struct list_head saved[4];
    for (size_t i = 1; i < 4; ++i) links[i] = &((struct color *)node_at(l, i - 1))->list;
    for (size_t i = 0; i < 4; ++i) saved[i] = *links[i];
    for (int scenario = 0; scenario < 10; ++scenario) {
        switch (scenario) {
        case 0: links[1]->next = NULL; break;
        case 1: links[2]->prev = NULL; break;
        case 2: ++l->count; break;
        case 3: links[1]->next = LIST_POISON1; break;
        case 4: links[3]->prev = LIST_POISON2; break;
        case 5: links[3]->next = links[1]; links[1]->prev = links[3]; break;
        case 6: links[2]->next = links[2]; links[2]->prev = links[2]; break;
        case 7: l->head.next = (struct list_head *)(uintptr_t)0xdeadbeef; break;
        case 8: l->head.prev = NULL; break;
        case 9: l->count = l->max_nodes + 1; break;
        }
        struct kds_invariant inv = kds_verify(l);
        assert(!inv.ok && inv.detail[0]);
        assert(inv.forward <= l->max_nodes + 1 && inv.backward <= l->max_nodes + 1);
        char *text = execute(&ctx, "state");
        assert(strstr(text, "\"invariant\":{\"ok\":false,"));
        if (scenario == 0 || scenario == 1 || scenario == 3 || scenario == 4 || scenario == 7 || scenario == 8)
            assert(strstr(text, "\"next\":\"?\"") || strstr(text, "\"prev\":\"?\""));
        free(text);
        for (size_t i = 0; i < 4; ++i) *links[i] = saved[i];
        l->count = 3;
        assert(kds_verify(l).ok);
    }
    /* Destruction must free the registry even if a link remains poisoned. */
    links[1]->next = LIST_POISON1;
    kds_destroy(&ctx);
    assert(!kds_verify(NULL).ok);
}

int main(void) {
    alarm(20);
    commands(); allocation_and_limits(); corrupted_lists();
    struct kshim_stats s = kshim_get_stats();
    assert(s.allocs == s.frees && s.live == 0 && s.live_bytes == 0);
    puts("PASS core commands, IDs, atomic errors, limits, ENOMEM sweep, 512-byte boundary, 10 corrupt lists");
    alarm(0);
    return 0;
}
