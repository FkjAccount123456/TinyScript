#include <stdlib.h>

#define seq(T)                                                                 \
  {                                                                            \
    T *v;                                                                      \
    size_t len, max, Tsize;                                                    \
  }

#define seq_init(T)                                                            \
  ({                                                                           \
    T name;                                                                    \
    name.len = 0, name.max = 8, name.Tsize = sizeof(T),                        \
    name.v = malloc(name.Tsize * 8);                                           \
    name;                                                                      \
  })

#define seq_append(name, val)                                                  \
  do {                                                                         \
    if ((name).len == (name).max) {                                            \
      (name).max *= 2;                                                         \
      (name).v = realloc((name).v, (name).Tsize * (name).max);                 \
    }                                                                          \
    (name).v[(name).len++] = val;                                              \
  } while (0)

#define seq_pop(name) ((name).v[--(name).len])

#define seq_free(name) (free((name).v))
