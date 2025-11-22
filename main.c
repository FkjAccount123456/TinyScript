#include "compile.h"
#include "stdglobal.h"
#include <stdio.h>
#include <assert.h>

gc_root gc;

void init() {
  gc = gc_init();
}

void finalize() {
  gc_free(gc_collect(&gc));
  gc_finalize(&gc);
}

void test_gc() {
  val l = val_list(&gc, 8);
  printf("l: %p\n", gc_next(&gc.all_gc));
  val s = val_str(&gc, "hello", 5);
  printf("s: %p\n", gc_next(&gc.all_gc));
  list_append(l, s);
  gc_add_root(&gc, l);
  gc_debug(&gc, "initial");
  gc_free(gc_collect(&gc));
  gc_debug(&gc, "after collect 1");
  l.l->len--;
  gc_free(gc_collect(&gc));
  gc_debug(&gc, "after collect 2");
  gc.len--;
  gc_free(gc_collect(&gc));
  gc_debug(&gc, "after collect 3");
  gc_finalize(&gc);
}

void _test_table_items(val table) {
  for (size_t i = 0; i < table.o->len; i++) {
    printf("%s: %lld, ", table.o->entrys[i].key, table.o->entrys[i].val.i);
  }
  puts("");
  for (size_t i = 0; i < table.o->len; i++) {
    val *v = object_get(table, table.o->entrys[i].key);
    val ans = table.o->entrys[i].val;
    assert(v->tp == T_INT && ans.tp == T_INT && v->i == ans.i);
  }
  printf("table items test passed\n");
}

void test_table() {
  val obj = val_obj(&gc);

  gc_add_root(&gc, obj);

  object_insert(obj, "Aa", val_int(1));
  object_insert(obj, "BB", val_int(2));
  object_insert(obj, "C#", val_int(3));
  _test_table_items(obj);

  object_insert(obj, "Ab", val_int(4));
  object_insert(obj, "BC", val_int(5));
  object_insert(obj, "Cd", val_int(6));
  _test_table_items(obj);

  object_insert(obj, "Ae", val_int(7));
  object_insert(obj, "BF", val_int(8));
  object_insert(obj, "Cg", val_int(9));
  object_insert(obj, "Dh", val_int(10));
  object_insert(obj, "Ei", val_int(11));
  object_insert(obj, "Fj", val_int(12));
  object_insert(obj, "Gk", val_int(13));
  object_insert(obj, "Hl", val_int(14));
  object_insert(obj, "Im", val_int(15));
  object_insert(obj, "Jn", val_int(16));
  object_insert(obj, "Ko", val_int(17));
  object_insert(obj, "Lp", val_int(18));
  object_insert(obj, "Qr", val_int(19));
  object_insert(obj, "Ss", val_int(20));
  object_insert(obj, "Tt", val_int(21));
  _test_table_items(obj);

  puts("replace");
  object_insert(obj, "BB", val_int(22));
  _test_table_items(obj);

  printf("%zu\n", obj.o->len);

  gc.len--;

  gc_free(gc_collect(&gc));
  gc_finalize(&gc);
}

raw_str read_file(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("Error: file not found\n");
    exit(-1);
  }
  raw_str res = seq_init(raw_str);
  char c;
  while ((c = fgetc(fp)) != EOF) {
    seq_append(res, c);
  }
  seq_append(res, 0);
  fclose(fp);
  return res;
}

void run_code(char *code) {
  val extglobal = val_obj(&gc);
  gc_add_root(&gc, extglobal);
  object_insert(extglobal, "print", val_cfunc(g_print));
  object_insert(extglobal, "println", val_cfunc(g_println));
  object_insert(extglobal, "getchar", val_cfunc(g_getchar));
  object_insert(extglobal, "ord", val_cfunc(g_ord));
  object_insert(extglobal, "chr", val_cfunc(g_chr));
  object_insert(extglobal, "len", val_cfunc(g_len));
  object_insert(extglobal, "typeof", val_cfunc(g_typeof));

  tokenlist tokens = tokenize(code);
  parser p = parser_init(tokens);
  statement *ast = parse(&p);
  compiler c = compiler_new();
  size_t reserve = compile_program(&gc, &c, ast);
  compiler_free_data(&c);
  // for (size_t i = 0; i < c.code.len; i++)
  //   printf("%zu ", i), bytecode_print(c.code.v[i]), puts("");
  // puts("");
  run(&gc, c.code, reserve, extglobal, c.objkeys);
  compiler_free_code(&c);
  stmt_free(ast);
  free(tokens.v);
  for (size_t i = 0; i < tokens.strs.len; i++)
    free(tokens.strs.v[i]);
  free(tokens.strs.v);

  gc.len--;
}

void run_file(char *filename) {
  run_code(read_file(filename).v);
}

int main(int argc, char **argv) {
  init();
  if (argc == 2)
    run_file(argv[1]);
  finalize();
  return 0;
}
