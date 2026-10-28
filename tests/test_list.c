#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kshim.h>
#include <linux/list.h>
#include <linux/slab.h>

enum { CAPACITY = 64, OPERATIONS = 5000 };
struct item { int value; struct list_head link; };
static uint32_t seed = UINT32_C(0xc011a72);
static uint32_t random_next(void) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return seed;
}

static void verify(struct list_head *head, struct item **model, size_t count) {
    assert(!!list_empty(head) == (count == 0));
    assert(head->next == (count ? &model[0]->link : head));
    assert(head->prev == (count ? &model[count - 1]->link : head));
    if (count) assert(list_first_entry(head, struct item, link) == model[0]);
    struct item *item;
    size_t i = 0;
    list_for_each_entry(item, head, link) {
        assert(i < count && item == model[i]);
        assert(list_entry(&item->link, struct item, link) == item);
        assert(item->link.next->prev == &item->link);
        assert(item->link.prev->next == &item->link);
        assert(kshim_serial_of(item) > 0);
        ++i;
    }
    assert(i == count);
    list_for_each_entry_reverse(item, head, link) {
        assert(i > 0 && item == model[i - 1]);
        --i;
    }
    assert(i == 0);
    struct kshim_stats stats = kshim_get_stats();
    assert(stats.live == count);
    assert(stats.allocs - stats.frees == count);
    assert(stats.live_bytes == count * sizeof(struct item));
}

static void delete_item(struct item *item) {
    list_del(&item->link);
    assert(item->link.next == LIST_POISON1 && item->link.prev == LIST_POISON2);
    kfree(item);
}

int main(void) {
    LIST_HEAD(head);
    struct item *model[CAPACITY] = {0};
    size_t count = 0;
    puts("list: seed=0x0c011a72 operations=5000, invariants after every operation");
    verify(&head, model, count);
    struct item *item, *tmp;
    list_for_each_entry_safe(item, tmp, &head, link) { assert(0 && "empty safe iteration"); }
    for (int step = 0; step < OPERATIONS; ++step) {
        unsigned int action = random_next() % 6;
        if ((!count || action < 3) && count < CAPACITY) {
            item = kmalloc(sizeof(*item), GFP_KERNEL);
            assert(item);
            for (size_t i = 0; i < sizeof(*item); ++i) assert(((unsigned char *)item)[i] == 0xA5);
            item->value = step;
            INIT_LIST_HEAD(&item->link);
            assert(item->link.next == &item->link && item->link.prev == &item->link);
            size_t at;
            if (!count || action == 0) {
                at = 0; list_add(&item->link, &head);
            } else if (action == 1) {
                at = count; list_add_tail(&item->link, &head);
            } else {
                at = random_next() % count + 1;
                list_add(&item->link, &model[at - 1]->link);
            }
            memmove(model + at + 1, model + at, (count - at) * sizeof(*model));
            model[at] = item; ++count;
        } else if (count && action == 4) {
            size_t out = 0;
            list_for_each_entry_safe(item, tmp, &head, link) {
                if (item->value % 3 == 0) delete_item(item);
                else model[out++] = item;
            }
            count = out;
        } else if (count) {
            size_t at = random_next() % count;
            delete_item(model[at]);
            memmove(model + at, model + at + 1, (count - at - 1) * sizeof(*model));
            --count;
        }
        verify(&head, model, count);
    }
    list_for_each_entry_safe(item, tmp, &head, link) { delete_item(item); }
    verify(&head, model, 0);
    struct kshim_stats stats = kshim_get_stats();
    assert(stats.allocs == stats.frees && stats.live == 0 && stats.live_bytes == 0);
    puts("PASS list API + seeded differential model + zero leaks");
    return 0;
}
