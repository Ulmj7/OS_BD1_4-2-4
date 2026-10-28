#ifndef KSHIM_INIT_H
#define KSHIM_INIT_H
#include <kshim.h>
#define __init
#define __exit
#define module_init(fn) static void __attribute__((constructor)) kshim_init_##fn(void) { kshim_register_init(fn); }
#define module_exit(fn) static void __attribute__((constructor)) kshim_exit_##fn(void) { kshim_register_exit(fn); }
#endif
