#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/types.h>
static int start = 25;
module_param(start, int, 0);
MODULE_PARM_DESC(start, "Positive starting integer for the Collatz sequence");
struct collatz_node { u64 value; struct list_head list; };
static LIST_HEAD(collatz_list);
static void collatz_exit(void)
{
    struct collatz_node *entry, *next;
    list_for_each_entry_safe(entry, next, &collatz_list, list) {
        list_del(&entry->list);
        kfree(entry);
    }
}
static int __init collatz_init(void)
{
    u64 value;
    unsigned int index = 0;
    struct collatz_node *entry;
    if (start < 1) {
        printk(KERN_ERR "collatz: start must be positive\n");
        return -EINVAL;
    }
    value = (u64)start;
    for (;;) {
        entry = kmalloc(sizeof(*entry), GFP_KERNEL);
        if (!entry) {
            collatz_exit();
            printk(KERN_ERR "collatz: kmalloc failed\n");
            return -ENOMEM;
        }
        entry->value = value;
        INIT_LIST_HEAD(&entry->list);
        list_add_tail(&entry->list, &collatz_list);
        if (value == 1)
            break;
        if (value & 1) {
            /* Compile-time constant division; no runtime 64-bit division. */
            if (value > (~(u64)0 - 1) / 3) {
                collatz_exit();
                printk(KERN_ERR "collatz: unsigned 64-bit overflow\n");
                return -EINVAL;
            }
            value = 3 * value + 1;
        } else {
            value >>= 1;
        }
    }
    list_for_each_entry(entry, &collatz_list, list)
        printk(KERN_INFO "collatz[%u]: value=%llu\n", index++, (unsigned long long)entry->value);
    return 0;
}
module_init(collatz_init);
module_exit(collatz_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("F.CSM302 BD1: Collatz sequence in a circular linked list");
