#ifndef KSHIM_KERNEL_H
#define KSHIM_KERNEL_H
#include <kshim.h>
#include <linux/types.h>
#include <errno.h>
#include <limits.h>
#define KERN_INFO ""
#define KERN_ERR ""
#define KERN_WARNING ""
#define pr_info(...) printk(__VA_ARGS__)
#define pr_err(...) printk(__VA_ARGS__)
#define U64_MAX UINT64_MAX
#endif
