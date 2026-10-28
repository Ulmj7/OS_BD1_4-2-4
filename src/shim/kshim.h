/* Public test API: include <kshim.h>, then a real module .c in the test TU.
 * kshim_run_init()/kshim_run_exit() invoke module_init/module_exit registrations.
 * kshim_set_param("start", value) sets a registered int module parameter.
 * kshim_get_stats() reports allocations; kshim_fail_alloc_after(n) allows n
 * successful allocations before failure, or disables injection with n < 0.
 * kshim_set_printk_sink(callback, context) replaces the default stdout sink.
 * Never pass freed pointers to kshim_serial_of: invalid/double frees abort.
 */
#ifndef KSHIM_H
#define KSHIM_H
#include <stddef.h>
#include <stdint.h>
struct kshim_stats { size_t allocs, frees, live, live_bytes; };
typedef void (*kshim_sink)(const char *, void *);
void *kmalloc(size_t size, unsigned int flags);
void kfree(const void *ptr);
uint64_t kshim_serial_of(const void *ptr);
int kshim_is_live(const void *ptr);
struct kshim_stats kshim_get_stats(void);
void kshim_restore_stats(struct kshim_stats stats);
void kshim_fail_alloc_after(long long n);
void kshim_set_printk_sink(kshim_sink sink, void *context);
int printk(const char *format, ...) __attribute__((format(printf, 1, 2)));
void kshim_register_init(int (*fn)(void));
void kshim_register_exit(void (*fn)(void));
void kshim_register_param(const char *name, int *value);
int kshim_run_init(void);
void kshim_run_exit(void);
int kshim_set_param(const char *name, long long value);
#endif
