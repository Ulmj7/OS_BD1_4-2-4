/* Public engine API: kds_init(&ctx, max_nodes), kds_execute(&ctx, command, out),
 * kds_destroy(&ctx). Each execute writes one JSON line. kds_verify(&ctx.lists[i])
 * safely checks registered pointers, even with corrupted next/prev/count.
 * list indices: 0=color, 1=collatz, 2=stack, 3=queue. Do not alter the registry.
 * Grammar: duplicate keys => E_SYNTAX; unknown keys => E_UNKNOWN_ARG;
 * missing keys => E_MISSING_ARG; non-decimal => E_BAD_VALUE; numeric overflow,
 * RGB outside 0..255 or negative/out-of-bounds index => E_RANGE.
 * Failed commands preserve state, including allocation counters; temporary
 * allocation serials may be skipped. inject_fail uses after=N or bare off.
 */
#ifndef KDS_CORE_H
#define KDS_CORE_H
#include <linux/list.h>
#include <kshim.h>
#include <stdio.h>
struct color { int red; int blue; int green; struct list_head list; };
struct kds_value { long long value; struct list_head list; };
struct kds_list {
    struct list_head head;
    size_t count, registered, max_nodes;
    void **nodes;
    const char *name;
    int is_color;
};
struct kds_context { struct kds_list lists[4]; size_t max_nodes; };
struct kds_invariant { int ok; char detail[160]; size_t forward, backward; };
int kds_init(struct kds_context *ctx, size_t max_nodes);
void kds_destroy(struct kds_context *ctx);
struct kds_invariant kds_verify(const struct kds_list *list);
void kds_execute(struct kds_context *ctx, const char *command, FILE *out);
void kds_reject(struct kds_context *ctx, const char *code, const char *message, FILE *out);
#endif
