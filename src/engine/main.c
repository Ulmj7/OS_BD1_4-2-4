#include "kds_core.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    size_t max_nodes=2048;
    if (argc!=1) {
        if (argc!=3 || strcmp(argv[1],"--max-nodes")) { fputs("usage: kds_engine [--max-nodes 1..100000]\n",stderr); return 2; }
        char *end; errno=0; unsigned long value=strtoul(argv[2],&end,10);
        if (errno || !argv[2][0] || *end || value<1 || value>100000) { fputs("invalid --max-nodes\n",stderr); return 2; }
        max_nodes=(size_t)value;
    }
    struct kds_context ctx;
    if (kds_init(&ctx,max_nodes)) { fputs("engine initialization failed\n",stderr); return 2; }
    char line[514]; size_t used=0; int overflow=0, embedded_nul=0, ch;
    while ((ch=fgetc(stdin))!=EOF) {
        if (ch!='\n') {
            if (!ch) embedded_nul=1;
            if (used<513) line[used++]=(char)ch; else overflow=1;
            continue;
        }
        if (used && line[used-1]=='\r') --used;
        line[used]=0;
        if (overflow || used>512) kds_reject(&ctx,"E_SYNTAX","Line exceeds 512 characters.",stdout);
        else if (embedded_nul) kds_reject(&ctx,"E_SYNTAX","NUL byte in command.",stdout);
        else { char *p=line; while (*p==' ' || *p=='\t') ++p; if (*p && *p!='#') kds_execute(&ctx,line,stdout); }
        used=0; overflow=embedded_nul=0;
    }
    if (used || overflow) {
        line[used]=0;
        if (overflow || used>512 || embedded_nul) kds_reject(&ctx,"E_SYNTAX","Invalid final command line.",stdout);
        else { char *p=line; while (*p==' ' || *p=='\t') ++p; if (*p && *p!='#') kds_execute(&ctx,line,stdout); }
    }
    kds_destroy(&ctx);
    struct kshim_stats stats=kshim_get_stats();
    if (stats.live) { fprintf(stderr,"LEAK: live=%zu\n",stats.live); return 3; }
    return ferror(stdin) || ferror(stdout) ? 2 : 0;
}
