#ifndef VALS_H
#define VALS_H

#include <stdbool.h>
#include <stddef.h>
#include "seq.h"

typedef enum : unsigned char {
  T_NIL,
  T_INT,
  T_FLOAT,
  T_CFUNC,
  T_CMETHOD,

  T_STR,
  T_LIST,
  T_OBJ,
  T_TYPE,
  T_FUNC,
  T_METHOD,
  T_EXPANDED_METHOD,
  T_ENV,
} type_t;

#define type_is_gc(tp) (tp >= T_STR)

typedef struct val val;
typedef struct gc_root gc_root;

typedef struct str str;
typedef struct list list;
typedef struct object object;
typedef struct func func;
typedef struct expanded_method expanded_method;
typedef struct env env;
typedef long long intval;
typedef double floatval;
typedef val (*cfunc)(gc_root *, size_t, val *);
typedef struct varpos {
  unsigned int scope, pos;
} varpos;

typedef struct val {
  union {
    intval i;
    floatval f;
    cfunc cf;

    str *s;
    list *l;
    object *o;
    func *fn;
    expanded_method *em;
    env *env;
  };
  type_t tp;
} val;

#define val_nil ((val){.tp = T_NIL})
#define val_int(I) ((val){.tp = T_INT, .i = I})
#define val_float(F) ((val){.tp = T_FLOAT, .f = F})
#define val_cfunc(CF) ((val){.tp = T_CFUNC, .cf = CF})
#define val_cmethod(CF) ((val){.tp = T_CMETHOD, .cf = CF})

typedef struct seq(val) raw_vals;

typedef struct gc_root {
  val all_gc;
  val *roots;
  size_t len, max;
  val metatables[T_ENV];
} gc_root;

void gc_init_mt(gc_root *gc);
val *gc_mt_find(gc_root *gc, type_t tp, char *name);
gc_root gc_init();
void gc_finalize(gc_root *gc);
void gc_recurse(val *v);
void gc_debug(gc_root *gc, char *msg);
val *gc_collect(gc_root *gc);
void gc_free(val *to_free);
void gc_add_root(gc_root *gc, val v);
val gc_add(gc_root *gc, type_t tp);
void val_free(val *v);

#define GC_COMMON bool marked; val *next
#define val_ptr(v) ((val_common *)(v))->ptr
#define gc_marked(v) (((gc_common *)val_ptr(v))->marked)
#define gc_next(v) (((gc_common *)val_ptr(v))->next)

typedef struct gc_common {
  GC_COMMON;
} gc_common;

typedef struct val_common {
  void *ptr;
  type_t tp;
} val_common;

typedef struct str {
  GC_COMMON;
  char *data;
  size_t len, max;
} str;

typedef struct list {
  GC_COMMON;
  val *data;
  size_t len, max;
} list;

struct obj_entry {
  size_t hash;
  char *key;
  val val;
};

typedef struct object {
  GC_COMMON;
  val meta;
  size_t *table;
  struct obj_entry *entrys;
  size_t len, max, mod;
} object;

typedef struct env {
  GC_COMMON;
  val parent;
  val varlist;
} env;

typedef struct func {
  GC_COMMON;
  size_t pc, reserve;
  val env;
} func;

typedef struct expanded_method {
  GC_COMMON;
  val obj;
  val method;
} expanded_method;

val val_str(gc_root *gc, char *base, size_t len);
val val_list(gc_root *gc, size_t reserve);
val val_obj(gc_root *gc);
val val_type(gc_root *gc);
val val_func(gc_root *gc, size_t pc, size_t reserve, val env);
val val_method(gc_root *gc, size_t pc, size_t reserve, val env);
val val_expanded_method(gc_root *gc, val obj, val method);
val val_env(gc_root *gc, val parent, val varlist);

void str_append(val s, char *c, size_t len);

void list_append(val l, val v);
void list_reserve(val l, size_t r);

size_t get_str_hash(char *str, size_t len);

void object_insert(val obj, char *key, val val);
val *_object_get(val obj, char *key);
val *object_get(val obj, char *key);

// and和or需要短路运算，不在此定义

bool val_eq(val a, val b);
bool val_lt(val a, val b);
bool val_le(val a, val b);

val val_add(val a, val b);
val val_sub(val a, val b);
val val_mul(val a, val b);
val val_div(val a, val b);
val val_mod(val a, val b);
val val_lsh(val a, val b);
val val_rsh(val a, val b);
val val_band(val a, val b);
val val_bor(val a, val b);
val val_xor(val a, val b);

bool val_bool(val a);
val val_neg(val a);
val val_inv(val a);

void val_print(val a);
void val_debug(val a);

#endif // VALS_H
