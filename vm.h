#ifndef VM_H
#define VM_H

#include "parse.h"

typedef enum opcode {
  C_EXIT,
  C_NOOP,
  C_PUSHI,
  C_PUSHF,
  C_PUSHN,
  C_PUSHS,
  C_POP,
  C_BINARY,
  C_UNARY,
  C_BUILDLIST,
  C_BUILDOBJ,
  C_INITOBJ,
  C_BUILDTYPE,
  C_BUILDFUNC,
  C_BUILDMETHOD,
  C_INDEX,
  C_SETINDEX,
  C_ATTR,
  C_ATTR_SHORTSTR,
  C_SETATTR,
  C_CALL,
  C_RET,
  C_JMP,
  C_JZ,
  C_JNZ,
  C_JZNOPOP,
  C_JNZNOPOP,
  C_LOADV,
  C_SETV,
  C_LOADEXT,
  C_LOADEXT_SHORTSTR,
} opcode;

typedef struct vmcode {
  opcode head;
  union {
    intval i;
    floatval f;
    char *s;
    size_t l;
    operator_t op;
    varpos v;
  };
} vmcode;

typedef struct seq(str_list) str_list_2;

typedef struct seq(size_t) pc_list;
typedef struct seq(bool) bool_list;

#define bytecode_new(tp) ((vmcode){.head = tp})
#define bytecode_init(tp, attr, val) ((vmcode){.head = tp, .attr = val})

typedef struct seq(vmcode) vmcodelist;

typedef struct vm {
  gc_root *gc;
  vmcode *bytecode;
  val extglobal;
  str_list_2 objkeys;
  pc_list pcstack;
  bool_list cons_stack;
  size_t len, pc, run_cnt;
  val stack, env, envstack;
} vm;

// extglobal是一个object
void vm_init(vm *vm, gc_root *gc, vmcodelist codelist, size_t reserve,
             val extglobal, val globalenv, str_list_2 objkeys);
void vm_free(vm *vm);

void vm_singlestep(vm *vm);
void vm_startcall(vm *vm, size_t nargs);
void vm_call(vm *vm, size_t nargs);
void vm_gc_collect(vm *vm);
void vm_run(vm *vm);

void bytecode_print(vmcode code);

#endif // VM_H
