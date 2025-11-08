#ifndef COMPILE_H
#define COMPILE_H

#include "vm.h"

typedef struct seq(str_list) str_list_2;

typedef struct compiler {
  val scope;
  vmcodelist code;
  pc_list w_jmps;
  pc_list w_begin;
  str_list_2 objkeys;
} compiler;

#define compiler_add(CODE) seq_append(c->code, CODE)

compiler compiler_new();
void compiler_free_data(compiler *c);
void compiler_free_code(compiler *c);
size_t compile_program(gc_root *gc, compiler *c, statement *node);

#endif // COMPILE_H
