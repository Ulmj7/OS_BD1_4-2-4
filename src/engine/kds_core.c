#include "kds_core.h"
#include <linux/kernel.h>
#include <linux/slab.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
struct strings { char **items; size_t count, capacity; };
struct ids { uint64_t *items; size_t count, capacity; };
struct events { struct strings ops, log; struct ids added, removed, visited, matched; };
struct argument { char *key, *value; };
struct parser { char text[513]; char *word[40]; size_t words; struct argument args[16]; size_t argc; const char *error, *message; };
static void *resize(void *p, size_t n) { void *r = p ? realloc(p, n) : malloc(n); if (!r) { fputs("engine: host memory exhausted\n", stderr); exit(2); } return r; }
static void add_text(struct strings *s, const char *format, ...) {
    if (!format) abort();
    va_list ap, copy; va_start(ap, format); va_copy(copy, ap);
    int n = vsnprintf(NULL, 0, format, copy); va_end(copy);
    if (n < 0) abort();
    char *text = resize(NULL, (size_t)n + 1); vsnprintf(text, (size_t)n + 1, format, ap); va_end(ap);
    if (s->count == s->capacity) { s->capacity = s->capacity ? s->capacity * 2 : 16; s->items = resize(s->items, s->capacity * sizeof(*s->items)); }
    s->items[s->count++] = text;
}
static void add_id(struct ids *s, uint64_t id) {
    if (s->count == s->capacity) { s->capacity = s->capacity ? s->capacity * 2 : 16; s->items = resize(s->items, s->capacity * sizeof(*s->items)); }
    s->items[s->count++] = id;
}
static void capture_log(const char *line, void *context) {
    struct events *e = context;
    add_text(&e->log, "%s", line);
    char *s = e->log.items[e->log.count - 1]; size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = 0;
}
static struct list_head *node_link(const struct kds_list *l, void *node) {
    return l->is_color ? &((struct color *)node)->list : &((struct kds_value *)node)->list;
}
static void *link_node(const struct kds_list *l, const struct list_head *link) {
    for (size_t i = 0; i < l->registered; ++i) if (node_link(l, l->nodes[i]) == link) return l->nodes[i];
    return NULL;
}
static int known(const struct kds_list *l, const struct list_head *p) { return p == &l->head || link_node(l, p) != NULL; }
struct kds_invariant kds_verify(const struct kds_list *l) {
    struct kds_invariant r = { .ok = 0 };
    const char *problem = NULL;
    if (!l || !l->nodes || l->registered > l->max_nodes || l->count > l->max_nodes) problem = "invalid registry or tracked count";
    else if (!known(l, l->head.next) || !known(l, l->head.prev)) problem = "head has unknown/poison pointer";
    else {
        const struct list_head *p = &l->head;
        for (size_t i = 0; i <= l->max_nodes; ++i) {
            if (!known(l, p->next) || !known(l, p->prev)) { problem = "unknown/poison pointer"; break; }
            if (p->next->prev != p || p->prev->next != p) { problem = "next/prev reciprocity failed"; break; }
            p = p->next;
            if (p == &l->head) break;
            ++r.forward;
            if (i == l->max_nodes) problem = "forward walk did not return to head";
        }
        p = l->head.prev;
        while (!problem && p != &l->head && r.backward <= l->max_nodes) {
            if (!known(l, p)) { problem = "unknown backward pointer"; break; }
            ++r.backward; p = p->prev;
        }
        if (!problem && p != &l->head) problem = "backward walk did not return to head";
        if (!problem && (r.forward != r.backward || r.forward != l->count || l->count != l->registered)) problem = "forward/backward/tracked count mismatch";
    }
    r.ok = problem == NULL;
    snprintf(r.detail, sizeof(r.detail), "%s", problem ? problem : "circular; next/prev reciprocal; counts agree");
    return r;
}
int kds_init(struct kds_context *ctx, size_t max_nodes) {
    memset(ctx, 0, sizeof(*ctx));
    if (!max_nodes || max_nodes > 100000) return -1;
    ctx->max_nodes = max_nodes;
    const char *names[] = { "color", "collatz", "stack", "queue" };
    for (size_t i = 0; i < 4; ++i) {
        struct kds_list *l = &ctx->lists[i]; INIT_LIST_HEAD(&l->head);
        l->max_nodes = max_nodes; l->name = names[i]; l->is_color = i == 0;
        l->nodes = calloc(max_nodes, sizeof(*l->nodes));
        if (!l->nodes) { kds_destroy(ctx); return -1; }
    }
    return 0;
}
static void unregister_node(struct kds_list *l, void *node) {
    for (size_t i = 0; i < l->registered; ++i) if (l->nodes[i] == node) { l->nodes[i] = l->nodes[--l->registered]; --l->count; return; }
    abort();
}
static void remove_node(struct kds_list *l, void *node, struct events *e) {
    uint64_t id = kshim_serial_of(node);
    if (e) { add_text(&e->ops, "list_del(&#%" PRIu64 "->list)", id); add_text(&e->ops, "kfree(#%" PRIu64 ")", id); add_id(&e->removed, id); }
    list_del(node_link(l, node)); unregister_node(l, node); kfree(node);
}
static void clear_list(struct kds_list *l, struct events *e) {
    if (l->is_color) {
        struct color *p, *tmp;
        list_for_each_entry_safe(p, tmp, &l->head, list) remove_node(l, p, e);
    } else {
        struct kds_value *p, *tmp;
        list_for_each_entry_safe(p, tmp, &l->head, list) remove_node(l, p, e);
    }
}
void kds_destroy(struct kds_context *ctx) {
    for (size_t i = 0; i < 4; ++i) {
        struct kds_list *l = &ctx->lists[i];
        /* Registry cleanup is bounded even when a test corrupted the links. */
        for (size_t j = 0; j < l->registered; ++j) kfree(l->nodes[j]);
        free(l->nodes); l->nodes = NULL; l->count = l->registered = 0; INIT_LIST_HEAD(&l->head);
    }
}
static void quote(FILE *out, const char *s) {
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        if (*p == '"' || *p == '\\') { fputc('\\', out); fputc(*p, out); }
        else if (*p < 32 || *p >= 127) fprintf(out, "\\u%04x", *p);
        else fputc(*p, out);
    }
    fputc('"', out);
}
static void print_pointer(FILE *out, const struct kds_list *l, const struct list_head *ptr) {
    if (ptr == &l->head) quote(out, "head");
    else { void *node = link_node(l, ptr); if (node) fprintf(out, "%" PRIu64, kshim_serial_of(node)); else quote(out, "?"); }
}
static void print_state(FILE *out, struct kds_context *ctx) {
    fputs("{\"structures\":[", out);
    for (size_t i = 0; i < 4; ++i) {
        struct kds_list *l = &ctx->lists[i]; struct kds_invariant inv = kds_verify(l);
        if (i) fputc(',', out);
        fprintf(out, "{\"name\":\"%s\",\"count\":%zu,\"invariant\":{\"ok\":%s,\"detail\":", l->name, l->count, inv.ok ? "true" : "false");
        quote(out, inv.detail); fprintf(out, "},\"head\":{\"addr\":\"%p\",\"next\":", (void *)&l->head);
        print_pointer(out, l, l->head.next); fputs(",\"prev\":", out); print_pointer(out, l, l->head.prev); fputs("},\"nodes\":[", out);
        struct list_head *p = l->head.next;
        for (size_t j = 0; j < l->registered; ++j) {
            void *node = link_node(l, p); if (!node) break;
            if (j) fputc(',', out);
            fprintf(out, "{\"id\":%" PRIu64 ",\"addr\":\"%p\",\"index\":%zu,\"fields\":{", kshim_serial_of(node), node, j);
            if (l->is_color) { struct color *c = node; fprintf(out, "\"red\":%d,\"green\":%d,\"blue\":%d", c->red, c->green, c->blue); }
            else fprintf(out, "\"value\":%lld", ((struct kds_value *)node)->value);
            fputs("},\"next\":", out); print_pointer(out, l, p->next); fputs(",\"prev\":", out); print_pointer(out, l, p->prev); fputc('}', out); p = p->next;
            if (p == &l->head) break;
        }
        fputs("]}", out);
    }
    struct kshim_stats s = kshim_get_stats();
    fprintf(out, "],\"kmalloc\":{\"allocs\":%zu,\"frees\":%zu,\"live\":%zu,\"live_bytes\":%zu},\"max_nodes\":%zu}", s.allocs, s.frees, s.live, s.live_bytes, ctx->max_nodes);
}
void kds_reject(struct kds_context *ctx, const char *code, const char *message, FILE *out) {
    fputs("{\"ok\":false,\"error\":{\"code\":", out); quote(out, code); fputs(",\"message\":", out); quote(out, message); fputs("},\"state\":", out); print_state(out, ctx); fputs("}\n", out); fflush(out);
}
static void print_strings(FILE *out, const struct strings *s) { fputc('[', out); for (size_t i = 0; i < s->count; ++i) { if (i) fputc(',', out); quote(out, s->items[i]); } fputc(']', out); }
static void print_ids(FILE *out, const struct ids *s) { fputc('[', out); for (size_t i = 0; i < s->count; ++i) { if (i) fputc(',', out); fprintf(out, "%" PRIu64, s->items[i]); } fputc(']', out); }
static void free_events(struct events *e) {
    struct strings *ss[] = { &e->ops, &e->log };
    for (size_t j = 0; j < 2; ++j) { for (size_t i = 0; i < ss[j]->count; ++i) free(ss[j]->items[i]); free(ss[j]->items); }
    free(e->added.items); free(e->removed.items); free(e->visited.items); free(e->matched.items);
}
static void error(struct parser *p, const char *code, const char *message) { if (!p->error) { p->error = code; p->message = message; } }
static int parse(struct parser *p, const char *command) {
    size_t n = strlen(command);
    if (n > 512) { error(p, "E_SYNTAX", "Command exceeds 512 characters."); return 0; }
    memcpy(p->text, command, n + 1);
    for (size_t i = 0; i < n; ++i) if ((unsigned char)p->text[i] < 32 && p->text[i] != '\t') { error(p, "E_SYNTAX", "Control characters are not allowed."); return 0; }
    char *save = NULL;
    for (char *s = strtok_r(p->text, " \t", &save); s; s = strtok_r(NULL, " \t", &save)) {
        if (p->words == 40) { error(p, "E_SYNTAX", "Too many tokens."); return 0; }
        p->word[p->words++] = s;
    }
    if (!p->words) { error(p, "E_SYNTAX", "Empty command."); return 0; }
    return 1;
}
static int arguments(struct parser *p, size_t offset, const char *allowed) {
    for (size_t i = offset; i < p->words; ++i) {
        char *token = p->word[i], *eq = strchr(token, '=');
        if (!eq || eq == token || !eq[1] || strchr(eq + 1, '=')) { error(p, "E_SYNTAX", "Expected key=value."); return 0; }
        *eq = 0;
        for (size_t j = 0; j < p->argc; ++j) if (!strcmp(p->args[j].key, token)) { error(p, "E_SYNTAX", "Duplicate argument."); return 0; }
        char needle[64]; snprintf(needle, sizeof(needle), "|%s|", token);
        if (!strstr(allowed, needle)) { error(p, "E_UNKNOWN_ARG", "Unknown argument."); return 0; }
        if (p->argc == 16) { error(p, "E_SYNTAX", "Too many arguments."); return 0; }
        p->args[p->argc++] = (struct argument){ token, eq + 1 };
    }
    return 1;
}
static const char *arg(struct parser *p, const char *key) { for (size_t i = 0; i < p->argc; ++i) if (!strcmp(key, p->args[i].key)) return p->args[i].value; return NULL; }
static int number(struct parser *p, const char *key, int required, long long low, long long high, long long *out) {
    const char *s = arg(p, key);
    if (!s) { if (required) error(p, "E_MISSING_ARG", "Required argument is missing."); return 0; }
    const char *digits = s; if (*digits == '-' || *digits == '+') ++digits;
    if (!*digits) { error(p, "E_BAD_VALUE", "Expected a decimal integer."); return 0; }
    for (const char *c = digits; *c; ++c) if (!isdigit((unsigned char)*c)) { error(p, "E_BAD_VALUE", "Expected a decimal integer."); return 0; }
    errno = 0; char *end; long long value = strtoll(s, &end, 10);
    if (errno == ERANGE || value < low || value > high) { error(p, "E_RANGE", "Number is outside the allowed range."); return 0; }
    *out = value; return 1;
}
static void *at(struct kds_list *l, size_t index) { struct list_head *p = l->head.next; while (index--) p = p->next; return link_node(l, p); }
static void attach(struct kds_list *l, void *node, struct list_head *after, int tail, struct events *e) {
    uint64_t id = kshim_serial_of(node); struct list_head *link = node_link(l, node);
    add_text(&e->ops, "kmalloc(sizeof(struct %s)) -> #%" PRIu64, l->is_color ? "color" : "kds_value", id);
    INIT_LIST_HEAD(link); add_text(&e->ops, "INIT_LIST_HEAD(&#%" PRIu64 "->list)", id);
    if (tail) { list_add_tail(link, &l->head); add_text(&e->ops, "list_add_tail(&#%" PRIu64 "->list, &%s_list)", id, l->name); }
    else {
        list_add(link, after);
        if (after == &l->head) add_text(&e->ops, "list_add(&#%" PRIu64 "->list, &%s_list)", id, l->name);
        else add_text(&e->ops, "list_add(&#%" PRIu64 "->list, &#%" PRIu64 "->list)", id, kshim_serial_of(link_node(l, after)));
    }
    l->nodes[l->registered++] = node; ++l->count; add_id(&e->added, id);
}
static void log_node(struct kds_list *l, void *node, size_t index, struct events *e) {
    uint64_t id = kshim_serial_of(node); add_id(&e->visited, id);
    if (l->is_color) { struct color *c = node; printk("color[%zu]: red=%d, green=%d, blue=%d\n", index, c->red, c->green, c->blue); }
    else printk("%s[%zu]: value=%lld\n", l->name, index, ((struct kds_value *)node)->value);
}
static void traverse(struct kds_list *l, struct events *e) {
    add_text(&e->ops, "list_for_each_entry(node, &%s_list, list)", l->name);
    size_t index = 0;
    if (l->is_color) { struct color *c; list_for_each_entry(c, &l->head, list) log_node(l, c, index++, e); }
    else { struct kds_value *v; list_for_each_entry(v, &l->head, list) log_node(l, v, index++, e); }
}
static void *allocate(struct kds_list *l, struct parser *p) { void *node = kmalloc(l->is_color ? sizeof(struct color) : sizeof(struct kds_value), GFP_KERNEL); if (!node) error(p, "E_NOMEM", "kmalloc failed; the previous state is preserved."); return node; }
static void color_command(struct kds_context *ctx, struct parser *p, struct events *e) {
    struct kds_list *l = &ctx->lists[0]; const char *op = p->word[1];
    if (!strcmp(op, "traverse") || !strcmp(op, "clear") || !strcmp(op, "example")) {
        if (!arguments(p, 2, "")) return;
        if (!strcmp(op, "traverse")) { traverse(l, e); return; }
        if (!strcmp(op, "clear")) { clear_list(l, e); return; }
        if (l->max_nodes < 4) { error(p, "E_FULL", "Four nodes are required for this example."); return; }
        const int rgb[4][3] = {{138,43,226},{255,0,0},{0,255,0},{0,0,255}};
        struct color *nodes[4] = {0};
        for (size_t i = 0; i < 4; ++i) {
            nodes[i] = allocate(l, p);
            if (!nodes[i]) { for (size_t j = 0; j < i; ++j) kfree(nodes[j]); return; }
            *nodes[i] = (struct color){ .red=rgb[i][0], .green=rgb[i][1], .blue=rgb[i][2] };
        }
        clear_list(l, e); for (size_t i = 0; i < 4; ++i) attach(l, nodes[i], &l->head, 1, e); traverse(l, e); return;
    }
    int adding = !strcmp(op, "add_head") || !strcmp(op, "add_tail") || !strcmp(op, "insert");
    int deleting = !strcmp(op, "delete"), searching = !strcmp(op, "search");
    if (!adding && !deleting && !searching) { error(p, "E_UNKNOWN_CMD", "Unknown color operation."); return; }
    if (!arguments(p, 2, searching || !strcmp(op, "add_head") || !strcmp(op, "add_tail") ? "|red||green||blue|" : "|red||green||blue||index|")) return;
    long long red=0, green=0, blue=0, index=0;
    int hr=number(p,"red",adding,0,255,&red), hg=number(p,"green",adding,0,255,&green), hb=number(p,"blue",adding,0,255,&blue);
    int hi=number(p,"index",!strcmp(op,"insert"),0,LLONG_MAX,&index);
    if (p->error) return;
    if (hi && (hr || hg || hb) && deleting) { error(p,"E_SYNTAX","Use index or color criteria, not both."); return; }
    if (hi && (unsigned long long)index >= l->count) { error(p,"E_RANGE","Index is outside the list."); return; }
    if (adding) {
        if (l->count == l->max_nodes) { error(p,"E_FULL","The node limit has been reached."); return; }
        struct color *c = allocate(l,p); if (!c) return;
        *c=(struct color){.red=(int)red,.green=(int)green,.blue=(int)blue};
        struct list_head *after = hi ? node_link(l,at(l,(size_t)index)) : &l->head;
        attach(l,c,after,!strcmp(op,"add_tail"),e); return;
    }
    if (!hi && !hr && !hg && !hb) { error(p,"E_MISSING_ARG","Provide at least one search criterion."); return; }
    if (hi) { remove_node(l,at(l,(size_t)index),e); return; }
    struct color *c, *tmp; size_t ix=0, matches=0;
    list_for_each_entry(c,&l->head,list) if ((!hr || c->red==red) && (!hg || c->green==green) && (!hb || c->blue==blue)) ++matches;
    if (!matches) { error(p,"E_NOT_FOUND","No matching node."); return; }
    add_text(&e->ops,"%s(node, &color_list, list)",deleting ? "list_for_each_entry_safe" : "list_for_each_entry");
    list_for_each_entry_safe(c,tmp,&l->head,list) {
        log_node(l,c,ix++,e);
        if ((!hr || c->red==red) && (!hg || c->green==green) && (!hb || c->blue==blue)) { add_id(&e->matched,kshim_serial_of(c)); if (deleting) remove_node(l,c,e); }
    }
}
static void collatz_command(struct kds_context *ctx, struct parser *p, struct events *e) {
    struct kds_list *l=&ctx->lists[1]; const char *op=p->word[1];
    if (!strcmp(op,"traverse") || !strcmp(op,"clear")) { if (!arguments(p,2,"")) return; if (!strcmp(op,"clear")) clear_list(l,e); else traverse(l,e); return; }
    if (!strcmp(op,"run")) {
        long long start=0; if (!arguments(p,2,"|start|") || !number(p,"start",1,1,INT_MAX,&start)) return;
        size_t count=1; uint64_t n=(uint64_t)start;
        while (n!=1) {
            if (n & 1) { if (n>((uint64_t)LLONG_MAX-1)/3) { error(p,"E_RANGE","Collatz would overflow signed 64-bit."); return; } n=3*n+1; } else n>>=1;
            if (++count>l->max_nodes) { error(p,"E_FULL","The Collatz sequence exceeds the node limit."); return; }
        }
        void **nodes=resize(NULL,count*sizeof(*nodes)); n=(uint64_t)start;
        for (size_t i=0;i<count;++i) {
            nodes[i]=allocate(l,p);
            if (!nodes[i]) { for (size_t j=0;j<i;++j) kfree(nodes[j]); free(nodes); return; }
            ((struct kds_value *)nodes[i])->value=(long long)n; n=n&1 ? 3*n+1 : n>>1;
        }
        clear_list(l,e); for (size_t i=0;i<count;++i) attach(l,nodes[i],&l->head,1,e); free(nodes); traverse(l,e); return;
    }
    int deleting=!strcmp(op,"delete");
    if (!deleting && strcmp(op,"search")) { error(p,"E_UNKNOWN_CMD","Unknown Collatz operation."); return; }
    long long value=0; if (!arguments(p,2,"|value|") || !number(p,"value",1,LLONG_MIN,LLONG_MAX,&value)) return;
    size_t matches=0,index=0; struct kds_value *v,*tmp;
    list_for_each_entry(v,&l->head,list) if (v->value==value) ++matches;
    if (!matches) { error(p,"E_NOT_FOUND","No matching value."); return; }
    add_text(&e->ops,"%s(node, &collatz_list, list)",deleting ? "list_for_each_entry_safe" : "list_for_each_entry");
    list_for_each_entry_safe(v,tmp,&l->head,list) { log_node(l,v,index++,e); if (v->value==value) { add_id(&e->matched,kshim_serial_of(v)); if (deleting) remove_node(l,v,e); } }
}
static void linear_command(struct kds_context *ctx, struct parser *p, struct events *e, size_t kind) {
    struct kds_list *l=&ctx->lists[kind]; const char *op=p->word[1];
    int adding=!strcmp(op,kind==2?"push":"enqueue"), removing=!strcmp(op,kind==2?"pop":"dequeue");
    if (!adding && !removing && strcmp(op,"peek")) { error(p,"E_UNKNOWN_CMD","Unknown stack/queue operation."); return; }
    if (!arguments(p,2,adding?"|value|":"")) return;
    if (adding) {
        long long value=0; if (!number(p,"value",1,LLONG_MIN,LLONG_MAX,&value)) return;
        if (l->count==l->max_nodes) { error(p,"E_FULL","The node limit has been reached."); return; }
        struct kds_value *v=allocate(l,p); if (!v) return; v->value=value; attach(l,v,&l->head,kind==3,e);
    } else {
        if (list_empty(&l->head)) { error(p,"E_EMPTY","The structure is empty."); return; }
        void *node=at(l,0); add_text(&e->ops,"list_first_entry(&%s_list, struct kds_value, list)",l->name); log_node(l,node,0,e); add_id(&e->matched,kshim_serial_of(node)); if (removing) remove_node(l,node,e);
    }
}
void kds_execute(struct kds_context *ctx, const char *command, FILE *out) {
    struct parser p={0}; struct events e={0}; struct kshim_stats before=kshim_get_stats();
    const char *msg="Command completed.";
    kshim_set_printk_sink(capture_log,&e);
    if (parse(&p,command)) {
        const char *root=p.word[0];
        if (!strcmp(root,"help") || !strcmp(root,"state") || !strcmp(root,"check") || !strcmp(root,"reset")) {
            if (arguments(&p,1,"")) {
                if (!strcmp(root,"reset")) { for (size_t i=0;i<4;++i) clear_list(&ctx->lists[i],&e); kshim_fail_alloc_after(-1); }
                else if (!strcmp(root,"help")) msg="state | check | reset | inject_fail after=N|off | color add_head/add_tail/insert/delete/search/traverse/clear/example | collatz run/search/delete/traverse/clear | stack push/pop/peek | queue enqueue/dequeue/peek";
                else if (!strcmp(root,"check")) for (size_t i=0;i<4;++i) add_text(&e.ops,"kds_verify(&%s_list)",ctx->lists[i].name);
            }
        } else if (!strcmp(root,"inject_fail")) {
            if (p.words==2 && !strcmp(p.word[1],"off")) kshim_fail_alloc_after(-1);
            else if (arguments(&p,1,"|after|")) { long long after=0; if (number(&p,"after",1,0,LLONG_MAX,&after)) kshim_fail_alloc_after(after); }
        } else if (!strcmp(root,"color") || !strcmp(root,"collatz") || !strcmp(root,"stack") || !strcmp(root,"queue")) {
            if (p.words<2) error(&p,"E_MISSING_ARG","Operation is missing.");
            else if (!strcmp(root,"color")) color_command(ctx,&p,&e);
            else if (!strcmp(root,"collatz")) collatz_command(ctx,&p,&e);
            else linear_command(ctx,&p,&e,!strcmp(root,"stack")?2:3);
        } else error(&p,"E_UNKNOWN_CMD","Unknown command.");
    }
    kshim_set_printk_sink(NULL,NULL);
    if (p.error) { kshim_restore_stats(before); kds_reject(ctx,p.error,p.message,out); }
    else {
        fputs("{\"ok\":true,\"cmd\":",out); quote(out,command); fputs(",\"msg\":",out); quote(out,msg);
        fputs(",\"ops\":",out); print_strings(out,&e.ops); fputs(",\"log\":",out); print_strings(out,&e.log);
        fputs(",\"effect\":{\"added\":",out); print_ids(out,&e.added); fputs(",\"removed\":",out); print_ids(out,&e.removed);
        fputs(",\"visited\":",out); print_ids(out,&e.visited); fputs(",\"matched\":",out); print_ids(out,&e.matched);
        fputs("},\"state\":",out); print_state(out,ctx); fputs("}\n",out); fflush(out);
    }
    free_events(&e);
}
