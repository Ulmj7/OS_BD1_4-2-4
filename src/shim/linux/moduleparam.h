#ifndef KSHIM_MODULEPARAM_H
#define KSHIM_MODULEPARAM_H
#include <kshim.h>
#define module_param(name, type, mode) static void __attribute__((constructor)) kshim_param_##name(void) { kshim_register_param(#name, &name); }
#endif
