#include "debug.h"

#ifdef DEBUG_MODE

void *D_ptrs[65536];
char *D_args[65536];
size_t D_lins[65536];
size_t D_len = 0;

#endif
