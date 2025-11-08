#include "vals.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

gc_root gc_init() {
  gc_root gc;
  gc.all_gc = (val){.tp = T_STR};
  gc.all_gc.s = malloc(sizeof(str));
  gc.all_gc.s->len = 0;
  gc.all_gc.s->max = 0;
  gc.all_gc.s->data = malloc(1);
  gc.all_gc.s->data[0] = '\0';
  gc.all_gc.s->marked = false;
  gc.all_gc.s->next = NULL;
  gc.len = 0;
  gc.max = 8;
  gc.roots = malloc(sizeof(val) * gc.max);

  gc_init_mt(&gc);
  for (size_t i = 0; i < T_ENV; i++)
    gc_add_root(&gc, gc.metatables[i]);

  return gc;
}

val *gc_mt_find(gc_root *gc, type_t tp, char *name) {
  return object_get(gc->metatables[tp], name);
}

void gc_finalize(gc_root *gc) {
  gc->len -= T_ENV;
  gc_free(gc_collect(gc));
  if (gc_next(&gc->all_gc)) {
    printf("gc_finalize failed\n");
    exit(1);
  }
  free(gc->roots);
  free(gc->all_gc.s->data);
  free(gc->all_gc.s);
}

void gc_recurse(val *v) {
  if (!type_is_gc(v->tp) || gc_marked(v))
    return;
  gc_marked(v) = true;
  switch (v->tp) {
  case T_LIST:
    for (size_t i = 0; i < v->l->len; i++)
      gc_recurse(&v->l->data[i]);
    break;
  case T_OBJ:
  case T_TYPE:
    for (size_t i = 0; i < v->o->len; i++)
      gc_recurse(&v->o->entrys[i].val);
    break;
  case T_METHOD:
  case T_FUNC:
    gc_recurse(&v->fn->env);
    break;
  case T_EXPANDED_METHOD:
    gc_recurse(&v->em->obj);
    gc_recurse(&v->em->method);
    break;
  case T_ENV:
    gc_recurse(&v->env->parent);
    gc_recurse(&v->env->varlist);
    break;
  default:
    break;
  }
}

void gc_debug(gc_root *gc, char *msg) {
  printf("gc_debug: %s\n", msg);
  val *v = &gc->all_gc;
  while (gc_next(v)) {
    printf("  %p: %d %d\n", gc_next(v), gc_next(v)->tp, gc_marked(gc_next(v)));
    v = gc_next(v);
  }
  printf("end gc_debug\n");
}

val *gc_collect(gc_root *gc) {
  for (size_t i = 0; i < gc->len; i++)
    gc_recurse(&gc->roots[i]);
  val *v = &gc->all_gc;
  val *res = NULL, **res_end = &res;
  while (gc_next(v)) {
    if (!gc_marked(gc_next(v))) {
      *res_end = gc_next(v);
      gc_next(v) = gc_next(*res_end);
      gc_next(*res_end) = NULL;
      res_end = &gc_next(*res_end);
    } else {
      v = gc_next(v);
      gc_marked(v) = false;
    }
  }
  return res;
}

void gc_free(val *to_free) {
  for (val *v = to_free; v;) {
    val *next = gc_next(v);
    val_free(v);
    free(v);
    v = next;
  }
}

void gc_add_root(gc_root *gc, val v) {
  if (gc->len == gc->max) {
    gc->max <<= 1;
    gc->roots = realloc(gc->roots, gc->max * sizeof(val));
  }
  gc->roots[gc->len++] = v;
}

char _gc_type_size[] = {
  [T_STR] = sizeof(str),
  [T_LIST] = sizeof(list),
  [T_OBJ] = sizeof(object),
  [T_TYPE] = sizeof(object),
  [T_FUNC] = sizeof(func),
  [T_METHOD] = sizeof(func),
  [T_EXPANDED_METHOD] = sizeof(expanded_method),
  [T_ENV] = sizeof(env),
};

val gc_add(gc_root *gc, type_t tp) {
  val *v = malloc(sizeof(val));
  val_ptr(v) = malloc(_gc_type_size[tp]);
  v->tp = tp;
  gc_marked(v) = false;
  gc_next(v) = gc_next(&gc->all_gc);
  gc_next(&gc->all_gc) = v;
  return *v;
}

void val_free(val *v) {
  switch (v->tp) {
  case T_STR:
    free(v->s->data);
    break;
  case T_LIST:
    free(v->l->data);
    break;
  case T_OBJ:
  case T_TYPE:
    free(v->o->entrys);
    free(v->o->table);
    break;
  default:
    break;
  }

  if (type_is_gc(v->tp))
    free(val_ptr(v));
}

val val_str(gc_root *gc, char *base, size_t len) {
  val v = gc_add(gc, T_STR);
  v.s->len = len;
  v.s->max = 8;
  while (v.s->max < len)
    v.s->max <<= 1;
  v.s->data = malloc(v.s->max + 1);
  memcpy(v.s->data, base, len + 1);
  v.s->data[len] = '\0';
  return v;
}

val val_list(gc_root *gc, size_t reserve) {
  val v = gc_add(gc, T_LIST);
  v.l->len = 0;
  v.l->max = 1;
  while (v.l->max < reserve)
    v.l->max <<= 1;
  v.l->data = malloc(v.l->max * sizeof(val));
  return v;
}

val val_obj(gc_root *gc) {
  val v = gc_add(gc, T_OBJ);
  v.o->meta = val_nil;
  v.o->len = 0;
  v.o->max = 8;
  v.o->mod = 8;
  v.o->entrys = malloc(v.o->max * sizeof(struct obj_entry));
  v.o->table = malloc(v.o->mod * sizeof(size_t));
  memset(v.o->table, 0xff, v.o->mod * sizeof(size_t));
  return v;
}

val val_type(gc_root *gc) {
  val v = gc_add(gc, T_TYPE);
  v.o->meta = val_nil;
  v.o->len = 0;
  v.o->max = 8;
  v.o->mod = 8;
  v.o->entrys = malloc(v.o->max * sizeof(struct obj_entry));
  v.o->table = malloc(v.o->mod * sizeof(size_t));
  memset(v.o->table, 0xff, v.o->mod * sizeof(size_t));
  return v;
}

val val_func(gc_root *gc, size_t pc, size_t reserve, val env) {
  val v = gc_add(gc, T_FUNC);
  v.fn->pc = pc;
  v.fn->reserve = reserve;
  v.fn->env = env;
  return v;
}

val val_method(gc_root *gc, size_t pc, size_t reserve, val env) {
  val v = gc_add(gc, T_METHOD);
  v.fn->pc = pc;
  v.fn->reserve = reserve;
  v.fn->env = env;
  return v;
}

val val_expanded_method(gc_root *gc, val obj, val method) {
  val v = gc_add(gc, T_EXPANDED_METHOD);
  v.em->obj = obj;
  v.em->method = method;
  return v;
}

val val_env(gc_root *gc, val parent, val varlist) {
  val v = gc_add(gc, T_ENV);
  v.env->parent = parent;
  v.env->varlist = varlist;
  return v;
}

void str_append(val s, char *c, size_t len) {
  if (s.s->len + len > s.s->max) {
    while (s.s->max < s.s->len + len)
      s.s->max <<= 1;
    s.s->data = realloc(s.s->data, s.s->max + 1);
  }
  memcpy(s.s->data + s.s->len, c, len);
  s.s->len += len;
  s.s->data[s.s->len] = '\0';
}

// 多么简洁，不需要绑定gc_Object！
void list_append(val l, val v) {
  if (l.l->len == l.l->max) {
    l.l->max <<= 1;
    l.l->data = realloc(l.l->data, l.l->max * sizeof(val));
  }
  l.l->data[l.l->len++] = v;
}

void list_reserve(val l, size_t r) {
  if (l.l->max < r) {
    while (l.l->max < r)
      l.l->max <<= 1;
    l.l->data = realloc(l.l->data, l.l->max * sizeof(val));
  }
}

size_t get_str_hash(char *str, size_t len) {
  size_t hash = 0;
  for (size_t i = 0; i < len && str[i]; i++)
    hash = hash * 31 + str[i];
  return hash;
}

void _object_insert(object *obj, char *key, val val) {
  size_t hash = get_str_hash(key, -1);
  size_t pos = hash % obj->mod;
  if (obj->len >= obj->max) {
    obj->max <<= 1;
    obj->entrys = realloc(obj->entrys, obj->max * sizeof(struct obj_entry));
  }
  size_t i = pos;
  do {
    if (obj->table[i] == -1) {
      obj->entrys[obj->len].key = key;
      obj->entrys[obj->len].val = val;
      obj->entrys[obj->len].hash = hash;
      obj->table[i] = obj->len++;
      return;
    } else if (obj->entrys[obj->table[i]].hash == hash &&
               !strcmp(obj->entrys[obj->table[i]].key, key)) {
      obj->entrys[obj->table[i]].val = val;
      return;
    }
    i = (i + 1) % obj->mod;
  } while (i != pos);
  exit(1);
}

void _object_expand(object *obj) {
  obj->mod <<= 1;
  obj->table = realloc(obj->table, obj->mod * sizeof(struct obj_entry));
  memset(obj->table, 0xff, obj->mod * sizeof(size_t));
  size_t len = obj->len;
  obj->len = 0;
  for (size_t i = 0; i < len; i++)
    _object_insert(obj, obj->entrys[i].key, obj->entrys[i].val);
}

void object_insert(val obj, char *key, val val) {
  if (obj.o->len * 2 > obj.o->mod)
    _object_expand(obj.o);
  _object_insert(obj.o, key, val);
}

val *_object_get(val obj, char *key) {
  size_t hash = get_str_hash(key, -1);
  size_t pos = hash % obj.o->mod;
  size_t i = pos;
  // printf("Start looking for key %s with hash %llu at pos %llu\n", key, hash, pos);
  do {
    if (obj.o->table[i] == -1)
      return NULL;
    else if (obj.o->entrys[obj.o->table[i]].hash == hash &&
             !strcmp(obj.o->entrys[obj.o->table[i]].key, key))
      return &obj.o->entrys[obj.o->table[i]].val;
    i = (i + 1) % obj.o->mod;
  } while (i != pos);
  return NULL;
}

val *object_get(val obj, char *key) {
  while (obj.tp == T_OBJ || obj.tp == T_TYPE) {
    val *v = _object_get(obj, key);
    if (v)
      return v;
    obj = obj.o->meta;
  }
  return NULL;
}

bool val_eq(val a, val b) {
  if (a.tp != b.tp)
    return false;
  switch (a.tp) {
  case T_NIL:
    return true;
  case T_INT:
    return a.i == b.i;
  case T_FLOAT:
    return a.f == b.f;
  case T_STR:
    return !strcmp(a.s->data, b.s->data);
  default:
    printf("Unsupported type %d for val_eq\n", a.tp);
    exit(1);
  }
}

bool val_lt(val a, val b) {
  if (a.tp == b.tp) {
    if (a.tp == T_INT)
      return a.i < b.i;
    if (a.tp == T_FLOAT)
      return a.f < b.f;
    if (a.tp == T_STR)
      return strcmp(a.s->data, b.s->data) < 0;
  }
  printf("Unsupported type %d for comparison\n", a.tp);
  exit(1);
}

bool val_le(val a, val b) {
  if (a.tp == b.tp) {
    if (a.tp == T_INT)
      return a.i <= b.i;
    if (a.tp == T_FLOAT)
      return a.f <= b.f;
    if (a.tp == T_STR)
      return strcmp(a.s->data, b.s->data) <= 0;
  }
  printf("Unsupported type %d for comparison\n", a.tp);
  exit(1);
}

val val_add(val a, val b) {
  if (a.tp == b.tp) {
    if (a.tp == T_INT)
      return val_int(a.i + b.i);
    if (a.tp == T_FLOAT)
      return val_float(a.f + b.f);
  }
  printf("Unsupported type %d for addition\n", a.tp);
  exit(1);
}

val val_sub(val a, val b) {
  if (a.tp == b.tp) {
    if (a.tp == T_INT)
      return val_int(a.i - b.i);
    if (a.tp == T_FLOAT)
      return val_float(a.f - b.f);
  }
  printf("Unsupported type %d for subtraction\n", a.tp);
  exit(1);
}

val val_mul(val a, val b) {
  if (a.tp == b.tp) {
    if (a.tp == T_INT)
      return val_int(a.i * b.i);
    if (a.tp == T_FLOAT)
      return val_float(a.f * b.f);
  }
  printf("Unsupported type %d for multiplication\n", a.tp);
  exit(1);
}

val val_div(val a, val b) {
  if (a.tp == b.tp) {
    if (a.tp == T_INT)
      return val_int(a.i / b.i);
    if (a.tp == T_FLOAT)
      return val_float(a.f / b.f);
  }
  printf("Unsupported type %d for division\n", a.tp);
  exit(1);
}

val val_mod(val a, val b) {
  if (a.tp == b.tp && a.tp == T_INT)
    return val_int(a.i % b.i);
  printf("Unsupported type %d for modulo\n", a.tp);
  exit(1);
}

val val_lsh(val a, val b) {
  if (a.tp == b.tp && a.tp == T_INT)
    return val_int(a.i << b.i);
  printf("Unsupported type %d for left shift\n", a.tp);
  exit(1);
}

val val_rsh(val a, val b) {
  if (a.tp == b.tp && a.tp == T_INT)
    return val_int(a.i >> b.i);
  printf("Unsupported type %d for right shift\n", a.tp);
  exit(1);
}

val val_band(val a, val b) {
  if (a.tp == b.tp && a.tp == T_INT)
    return val_int(a.i & b.i);
  printf("Unsupported type %d for bitwise and\n", a.tp);
  exit(1);
}

val val_bor(val a, val b) {
  if (a.tp == b.tp && a.tp == T_INT)
    return val_int(a.i | b.i);
  printf("Unsupported type %d for bitwise or\n", a.tp);
  exit(1);
}

val val_xor(val a, val b) {
  if (a.tp == b.tp && a.tp == T_INT)
    return val_int(a.i ^ b.i);
  printf("Unsupported type %d for bitwise xor\n", a.tp);
  exit(1);
}

bool val_bool(val a) {
  switch (a.tp) {
  case T_NIL:
    return false;
  case T_INT:
    return (bool)a.i;
  case T_FLOAT:
    return (bool)a.f;
  case T_STR:
    return (bool)a.s->len;
  case T_LIST:
    return (bool)a.l->len;
  case T_OBJ:
  case T_TYPE:
    return (bool)a.o->len;
  case T_CFUNC:
  case T_FUNC:
  case T_METHOD:
  case T_EXPANDED_METHOD:
    return true;
  default:
    printf("Unsupported type %d for val_bool\n", a.tp);
    exit(1);
  }
}

val val_neg(val a) {
  if (a.tp == T_INT)
    return val_int(-a.i);
  if (a.tp == T_FLOAT)
    return val_float(-a.f);
  printf("Unsupported type %d for negation\n", a.tp);
  exit(1);
}

val val_inv(val a) {
  if (a.tp == T_INT)
    return val_int(~a.i);
  printf("Unsupported type %d for bitwise not\n", a.tp);
  exit(1);
}

void val_print(val a) {
  switch (a.tp) {
  case T_NIL:
    printf("nil");
    break;
  case T_INT:
    printf("%lld", a.i);
    break;
  case T_FLOAT:
    printf("%lf", a.f);
    break;
  case T_STR:
    printf("%s", a.s->data);
    break;
  default:
    printf("Unsupported type %d for val_print\n", a.tp);
    exit(1);
  }
}

void val_debug(val a) {
  switch (a.tp) {
  case T_NIL:
    printf("nil");
    break;
  case T_INT:
    printf("%lld", a.i);
    break;
  case T_FLOAT:
    printf("%lf", a.f);
    break;
  case T_STR:
    printf("<str>");
    break;
  case T_LIST:
    printf("<list>");
    break;
  case T_OBJ:
    printf("<obj>");
    break;
  case T_TYPE:
    printf("<type>");
    break;
  case T_CFUNC:
    printf("<c function>");
    break;
  case T_FUNC:
    printf("<function>");
    break;
  case T_METHOD:
    printf("<method>");
    break;
  case T_EXPANDED_METHOD:
    printf("<expanded method>");
    break;
  case T_ENV:
    printf("<env>");
    break;
  default:
    printf("Unsupported type %d for val_debug\n", a.tp);
    exit(1);
  }
}
