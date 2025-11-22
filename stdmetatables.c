#include "vals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

val stdmt_str_append(gc_root *gc, size_t argc, val *argv) {
  if (argc != 2 || argv[0].tp != T_STR || argv[1].tp != T_STR) {
    printf("Invalid arguments for str.append\n");
    exit(1);
  }
  str_append(argv[0], argv[1].s->data, argv[1].s->len);
  return val_nil;
}

val stdmt_list_append(gc_root *gc, size_t argc, val *argv) {
  if (argc < 2 || argv[0].tp != T_LIST) {
    printf("Invalid arguments for list.append\n");
    exit(1);
  }
  for (size_t i = 1; i < argc; i++)
    list_append(argv[0], argv[i]);
  return val_nil;
}

val stdmt_list_insert(gc_root *gc, size_t argc, val *argv) {
  if (argc < 3 || argv[0].tp != T_LIST || argv[1].tp != T_INT) {
    printf("Invalid arguments for list.insert\n");
    exit(1);
  }
  list_insert(argv[0], argv[1].i, argv[2]);
  return val_nil;
}

val stdmt_list_reserve(gc_root *gc, size_t argc, val *argv) {
  if (argc != 2 || argv[0].tp != T_LIST || argv[1].tp != T_INT) {
    printf("Invalid arguments for list.reserve\n");
    exit(1);
  }
  list_reserve(argv[0], argv[1].i);
  return val_nil;
}

val stdmt_list_extend(gc_root *gc, size_t argc, val *argv) {
  if (argc != 2 || argv[0].tp != T_LIST || argv[1].tp != T_LIST) {
    printf("Invalid arguments for list.extend\n");
    exit(1);
  }
  list_reserve(argv[0], argv[0].l->len + argv[1].l->len);
  memcpy(argv[0].l->data + argv[0].l->len, argv[1].l->data, argv[1].l->len * sizeof(val));
  argv[0].l->len += argv[1].l->len;
  return val_nil;
}

val stdmt_object_contains(gc_root *gc, size_t argc, val *argv) {
  if (argc != 2 || argv[0].tp != T_OBJ && argv[0].tp != T_TYPE || argv[1].tp != T_STR) {
    printf("Invalid arguments for object.contains\n");
    exit(1);
  }
  val *v = object_get(argv[0], argv[1].s->data);
  return val_int(v != 0);
}

val stdmt_object_keys(gc_root *gc, size_t argc, val *argv) {
  if (argc != 1 || argv[0].tp != T_OBJ && argv[0].tp != T_TYPE) {
    printf("Invalid arguments for object.keys\n");
    exit(1);
  }
  val keys = val_list(gc, argv[0].o->len);
  for (size_t i = 0; i < argv[0].o->len; i++)
    list_append(keys, val_str(gc, argv[0].o->entrys[i].key, strlen(argv[0].o->entrys[i].key)));
  return keys;
}

val stdmt_object_values(gc_root *gc, size_t argc, val *argv) {
  if (argc != 1 || argv[0].tp != T_OBJ && argv[0].tp != T_TYPE) {
    printf("Invalid arguments for object.values\n");
    exit(1);
  }
  val values = val_list(gc, argv[0].o->len);
  for (size_t i = 0; i < argv[0].o->len; i++)
    list_append(values, argv[0].o->entrys[i].val);
  return values;
}

void gc_init_mt(gc_root *gc) {
  for (size_t i = 0; i < T_ENV; i++)
    gc->metatables[i] = val_obj(gc);

#define add_mt(T, N, MT) object_insert(gc->metatables[T], #N, val_cmethod(MT))

  add_mt(T_STR, append, stdmt_str_append);

  add_mt(T_LIST, append, stdmt_list_append);
  add_mt(T_LIST, insert, stdmt_list_insert);
  add_mt(T_LIST, reserve, stdmt_list_reserve);
  add_mt(T_LIST, extend, stdmt_list_extend);

  add_mt(T_OBJ, contains, stdmt_object_contains);
  add_mt(T_OBJ, keys, stdmt_object_keys);
  add_mt(T_OBJ, values, stdmt_object_values);

  add_mt(T_TYPE, contains, stdmt_object_contains);
  add_mt(T_TYPE, keys, stdmt_object_keys);
  add_mt(T_TYPE, values, stdmt_object_values);

#undef add_mt
}
