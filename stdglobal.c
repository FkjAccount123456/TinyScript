#include "stdglobal.h"
#include <stdio.h>
#include <stdlib.h>

val g_print(gc_root *gc, size_t argc, val *argv) {
  for (size_t i = 0; i < argc; i++)
    val_print(gc, argv[i]);
  return val_nil;
}

val g_println(gc_root *gc, size_t argc, val *argv) {
  for (size_t i = 0; i < argc; i++)
    val_print(gc, argv[i]);
  putchar('\n');
  return val_nil;
}

val g_getchar(gc_root *gc, size_t argc, val *argv) {
  if (argc > 0) {
    printf("getchar: too many arguments\n");
    exit(1);
  }
  char c = getchar();
  return val_int(c);
}

val g_ord(gc_root *gc, size_t argc, val *argv) {
  if (argc != 1 || argv[0].tp != T_STR || argv[0].s->len != 1) {
    printf("ord: invalid arguments\n");
    exit(1);
  }
  char c = argv[0].s->data[0];
  return val_int(c);
}

val g_chr(gc_root *gc, size_t argc, val *argv) {
  if (argc != 1 || argv[0].tp != T_INT) {
    printf("chr: invalid arguments\n");
    exit(1);
  }
  intval i = argv[0].i;
  if (i < 0 || i > 255) {
    printf("chr: invalid argument\n");
    exit(1);
  }
  char c = i;
  return val_str(gc, &c, 1);
}

val g_len(gc_root *gc, size_t argc, val *argv) {
  if (argc != 1) {
    printf("len: invalid argument\n");
    exit(1);
  }
  if (argv[0].tp == T_STR)
    return val_int(argv[0].s->len);
  else if (argv[0].tp == T_LIST)
    return val_int(argv[0].l->len);
  else if (argv[0].tp == T_TYPE)
    return val_int(argv[0].o->len);
  else if (argv[0].tp == T_OBJ)
    return val_int(argv[0].o->len);
  else {
    printf("len: invalid argument\n");
    exit(1);
  }
}

val g_typeof(gc_root *gc, size_t argc, val *argv) {
  if (argc != 1) {
    printf("type: invalid argument\n");
    exit(1);
  }
  return val_int(argv[0].tp);
}
