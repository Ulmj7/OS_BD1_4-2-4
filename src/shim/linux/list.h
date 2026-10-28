#ifndef KSHIM_LIST_H
#define KSHIM_LIST_H
#include <stddef.h>
#include <stdint.h>
struct list_head { struct list_head *next, *prev; };
#define LIST_POISON1 ((struct list_head *)(uintptr_t)0x100)
#define LIST_POISON2 ((struct list_head *)(uintptr_t)0x122)
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)
static inline void INIT_LIST_HEAD(struct list_head *head) { head->next = head; head->prev = head; }
static inline void kshim_list_link(struct list_head *node, struct list_head *prev, struct list_head *next) {
    node->prev = prev; node->next = next; prev->next = node; next->prev = node;
}
static inline void list_add(struct list_head *node, struct list_head *head) { kshim_list_link(node, head, head->next); }
static inline void list_add_tail(struct list_head *node, struct list_head *head) { kshim_list_link(node, head->prev, head); }
static inline void list_del(struct list_head *node) {
    node->prev->next = node->next; node->next->prev = node->prev;
    node->next = LIST_POISON1; node->prev = LIST_POISON2;
}
static inline int list_empty(const struct list_head *head) { return head->next == head; }
#define list_entry(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#define list_first_entry(head, type, member) list_entry((head)->next, type, member)
/* Link cursors avoid manufacturing a container pointer for the sentinel. */
#define list_for_each_entry(pos, head, member) \
    for (struct list_head *_ks_it = (head)->next; \
         _ks_it != (head) && ((pos) = list_entry(_ks_it, __typeof__(*(pos)), member), 1); \
         _ks_it = _ks_it->next)
#define list_for_each_entry_reverse(pos, head, member) \
    for (struct list_head *_ks_it = (head)->prev; \
         _ks_it != (head) && ((pos) = list_entry(_ks_it, __typeof__(*(pos)), member), 1); \
         _ks_it = _ks_it->prev)
#define list_for_each_entry_safe(pos, tmp, head, member) \
    for (struct list_head *_ks_it = (head)->next, *_ks_next; \
         _ks_it != (head) && (_ks_next = _ks_it->next, \
         (pos) = list_entry(_ks_it, __typeof__(*(pos)), member), \
         (tmp) = _ks_next == (head) ? NULL : list_entry(_ks_next, __typeof__(*(tmp)), member), (void)(tmp), 1); \
         _ks_it = _ks_next)
#endif
