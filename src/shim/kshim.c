#include "kshim.h"
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAGIC UINT64_C(0x4b5348494d4c4956)
union allocation {
    max_align_t alignment;
    struct { uint64_t magic, serial; size_t size; union allocation *next; } h;
};
static union allocation *allocations;
static struct kshim_stats counters;
static uint64_t serial;
static long long fail_after = -1;
static kshim_sink print_sink;
static void *print_context;
static int (*init_fn)(void);
static void (*exit_fn)(void);
static struct { const char *name; int *value; } params[32];
static size_t param_count;
static union allocation *find(const void *ptr) {
    for (union allocation *a = allocations; a; a = a->h.next)
        if ((const void *)(a + 1) == ptr) return a;
    return NULL;
}
static void invalid(const char *action) { fprintf(stderr, "KSHIM: %s invalid or freed pointer\n", action); abort(); }
void *kmalloc(size_t size, unsigned int flags) {
    (void)flags;
    if (fail_after == 0 || size > SIZE_MAX - sizeof(union allocation)) return NULL;
    union allocation *a = malloc(sizeof(*a) + size);
    if (!a) return NULL;
    if (fail_after > 0) --fail_after;
    a->h.magic = MAGIC; a->h.serial = ++serial; a->h.size = size;
    a->h.next = allocations; allocations = a;
    ++counters.allocs; ++counters.live; counters.live_bytes += size;
    memset(a + 1, 0xA5, size);
    return a + 1;
}
void kfree(const void *ptr) {
    if (!ptr) return;
    union allocation **link = &allocations;
    while (*link && (const void *)(*link + 1) != ptr) link = &(*link)->h.next;
    if (!*link || (*link)->h.magic != MAGIC) invalid("free");
    union allocation *a = *link; *link = a->h.next;
    ++counters.frees; --counters.live; counters.live_bytes -= a->h.size;
    memset(a + 1, 0xDD, a->h.size); a->h.magic = 0; free(a);
}
uint64_t kshim_serial_of(const void *ptr) { union allocation *a = find(ptr); if (!a) invalid("serial lookup"); return a->h.serial; }
int kshim_is_live(const void *ptr) { return find(ptr) != NULL; }
struct kshim_stats kshim_get_stats(void) { return counters; }
void kshim_restore_stats(struct kshim_stats stats) {
    if (stats.live != counters.live || stats.live_bytes != counters.live_bytes) invalid("counter rollback");
    counters = stats;
}
void kshim_fail_alloc_after(long long n) { fail_after = n; }
void kshim_set_printk_sink(kshim_sink sink, void *context) { print_sink = sink; print_context = context; }
int printk(const char *format, ...) {
    va_list ap, copy; va_start(ap, format); va_copy(copy, ap);
    int n = vsnprintf(NULL, 0, format, copy); va_end(copy);
    if (n >= 0) {
        char *line = malloc((size_t)n + 1);
        if (line) { vsnprintf(line, (size_t)n + 1, format, ap); if (print_sink) print_sink(line, print_context); else fputs(line, stdout); free(line); }
    }
    va_end(ap); return n;
}
void kshim_register_init(int (*fn)(void)) { init_fn = fn; }
void kshim_register_exit(void (*fn)(void)) { exit_fn = fn; }
void kshim_register_param(const char *name, int *value) { if (param_count >= 32) abort(); params[param_count].name = name; params[param_count++].value = value; }
int kshim_run_init(void) { return init_fn ? init_fn() : 0; }
void kshim_run_exit(void) { if (exit_fn) exit_fn(); }
int kshim_set_param(const char *name, long long value) {
    if (value < INT_MIN || value > INT_MAX) return -1;
    for (size_t i = 0; i < param_count; ++i) if (!strcmp(name, params[i].name)) { *params[i].value = (int)value; return 0; }
    return -1;
}
