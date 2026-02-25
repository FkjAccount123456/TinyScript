#ifndef DEBUG_H
#define DEBUG_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #define DEBUG_MODE

#ifdef DEBUG_MODE

extern void *D_ptrs[];
extern char *D_args[];
extern size_t D_lins[];
extern size_t D_len;

#define malloc(n)                                                              \
  ({                                                                           \
    void *D_ptr = malloc(n);                                                   \
    printf("malloc %p %s %zu\n", D_ptr, #n, __LINE__);                         \
    D_args[D_len] = #n;                                                        \
    D_lins[D_len] = __LINE__;                                                  \
    D_ptrs[D_len++] = D_ptr;                                                   \
    D_ptr;                                                                     \
  })

#define calloc(n, s)                                                           \
  ({                                                                           \
    void *D_ptr = calloc(n, s);                                                \
    printf("calloc %p %s %s %zu\n", D_ptr, #n, #s, __LINE__);                  \
    D_args[D_len] = #n;                                                        \
    D_lins[D_len] = __LINE__;                                                  \
    D_ptrs[D_len++] = D_ptr;                                                   \
    D_ptr;                                                                     \
  })

#define realloc(P, n)                                                          \
  ({                                                                           \
    size_t D_i;                                                                \
    void *D_ptr = P, *_x;                                                      \
    for (D_i = 0; D_i < D_len; D_i++)                                          \
      if (D_ptrs[D_i] == D_ptr) {                                              \
        _x = realloc(D_ptr, n);                                                \
        D_ptrs[D_i] = _x;                                                      \
        break;                                                                 \
      }                                                                        \
    if (D_i == D_len) {                                                        \
      printf("Wrong realloc %p\n", D_ptr);                                     \
      exit(1);                                                                 \
    }                                                                          \
    printf("realloc %p %s %zu\n", D_ptr, #P, __LINE__);                        \
    _x;                                                                        \
  })

#define free(P)                                                                \
  ({                                                                           \
    size_t D_i;                                                                \
    void *D_ptr = P;                                                           \
    for (D_i = 0; D_i < D_len; D_i++)                                          \
      if (D_ptrs[D_i] == D_ptr) {                                              \
        memmove(D_ptrs + D_i, D_ptrs + D_i + 1,                                \
                sizeof(void *) * (D_len - D_i - 1));                           \
        memmove(D_args + D_i, D_args + D_i + 1,                                \
                sizeof(void *) * (D_len - D_i - 1));                           \
        memmove(D_lins + D_i, D_lins + D_i + 1,                                \
                sizeof(void *) * (D_len - D_i - 1));                           \
        free(D_ptr);                                                           \
        break;                                                                 \
      }                                                                        \
    if (D_i == D_len) {                                                        \
      printf("Wrong free %p\n", D_ptr);                                        \
      exit(1);                                                                 \
    }                                                                          \
    printf("free %p %s %zu\n", D_ptr, #P, __LINE__);                           \
    D_len--;                                                                   \
  })

#endif // DEBUG_MODE

#endif // DEBUG_H
