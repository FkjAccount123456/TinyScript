#ifndef STDGLOBAL_H
#define STDGLOBAL_H

#include "vals.h"

val g_print(gc_root *gc, size_t argc, val *argv);
val g_println(gc_root *gc, size_t argc, val *argv);
val g_getchar(gc_root *gc, size_t argc, val *argv);
val g_ord(gc_root *gc, size_t argc, val *argv);
val g_chr(gc_root *gc, size_t argc, val *argv);
val g_len(gc_root *gc, size_t argc, val *argv);
val g_typeof(gc_root *gc, size_t argc, val *argv);

#endif // STDGLOBAL_H
