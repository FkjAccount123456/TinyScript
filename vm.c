#include "vm.h"
#include "vals.h"
#include <stdio.h>
#include <string.h>

void vm_init(vm *vm, gc_root *gc, vmcodelist codelist, size_t reserve,
             val extglobal, val globalenv, str_list_2 objkeys) {
  vm->gc = gc;
  vm->bytecode = codelist.v;
  vm->len = codelist.len;
  vm->pc = vm->run_cnt = 0;
  vm->extglobal = extglobal;
  vm->objkeys = objkeys;

  vm->stack = val_list(gc, 64);
  gc_add_root(gc, vm->stack);

  vm->env = globalenv;

  vm->envstack = val_list(gc, 8);
  vm->pcstack = seq_init(pc_list);
  vm->cons_stack = seq_init(bool_list);

  list_append(vm->envstack, vm->env);
  gc_add_root(gc, vm->envstack);

  while (vm->env.env->varlist.l->len < reserve)
    list_append(vm->env.env->varlist, val_nil);
}

void vm_free(vm *vm) {
  vm->gc->len -= 2;
  free(vm->pcstack.v);
  free(vm->cons_stack.v);
  vm_gc_collect(vm);
}

void vm_singlestep(vm *vm) {
  gc_root *gc = vm->gc;
  vmcode *bytecode = vm->bytecode;
  val extglobal = vm->extglobal;
  str_list_2 objkeys = vm->objkeys;
  size_t len = vm->len;

  if (!(vm->pc < len && bytecode[vm->pc].head != C_EXIT))
    return;

  vmcode code = bytecode[vm->pc];
  // for (size_t i = 0; i < vm->stack.l->len; i++)
  //   val_debug(vm->stack.l->data[i]), putchar(' ');
  // puts("");
  // printf("%zu ", vm->pc), bytecode_print(code), printf("\n");
  switch (code.head) {
  case C_EXIT:
  case C_NOOP:
    break;
  case C_PUSHI:
    list_append(vm->stack, val_int(code.i));
    break;
  case C_PUSHF:
    list_append(vm->stack, val_float(code.f));
    break;
  case C_PUSHN:
    list_append(vm->stack, val_nil);
    break;
  case C_PUSHS:
    list_append(vm->stack, val_str(gc, code.s, strlen(code.s)));
    break;
  case C_POP:
    vm->stack.l->len--;
    break;
  case C_BINARY: {
    val b = vm->stack.l->data[--vm->stack.l->len];
    val a = vm->stack.l->data[--vm->stack.l->len];
    switch (code.op) {
    case O_ADD:
      list_append(vm->stack, val_add(a, b));
      break;
    case O_SUB:
      list_append(vm->stack, val_sub(a, b));
      break;
    case O_MUL:
      list_append(vm->stack, val_mul(a, b));
      break;
    case O_DIV:
      list_append(vm->stack, val_div(a, b));
      break;
    case O_MOD:
      list_append(vm->stack, val_mod(a, b));
      break;
    case O_LSH:
      list_append(vm->stack, val_lsh(a, b));
      break;
    case O_RSH:
      list_append(vm->stack, val_rsh(a, b));
      break;
    case O_LT:
      list_append(vm->stack, val_int(val_lt(a, b)));
      break;
    case O_LE:
      list_append(vm->stack, val_int(val_le(a, b)));
      break;
    case O_GT:
      list_append(vm->stack, val_int(!val_le(a, b)));
      break;
    case O_GE:
      list_append(vm->stack, val_int(!val_lt(a, b)));
      break;
    case O_EQ:
      list_append(vm->stack, val_int(val_eq(a, b)));
      break;
    case O_NE:
      list_append(vm->stack, val_int(!val_eq(a, b)));
      break;
    case O_BAND:
      list_append(vm->stack, val_band(a, b));
      break;
    case O_BOR:
      list_append(vm->stack, val_bor(a, b));
      break;
    case O_XOR:
      list_append(vm->stack, val_xor(a, b));
      break;
    default:
      printf("Unsupported operator %d\n", code.op);
      exit(1);
    }
    break;
  }
  case C_UNARY: {
    switch (code.op) {
    case O_POS:
      break;
    case O_NEG:
      vm->stack.l->data[vm->stack.l->len - 1] =
          val_neg(vm->stack.l->data[vm->stack.l->len - 1]);
      break;
    case O_NOT:
      vm->stack.l->data[vm->stack.l->len - 1] =
          val_int(val_bool(vm->stack.l->data[vm->stack.l->len - 1]));
      break;
    case O_INV:
      vm->stack.l->data[vm->stack.l->len - 1] =
          val_inv(vm->stack.l->data[vm->stack.l->len - 1]);
      break;
    default:
      printf("Unsupported operator %d\n", code.op);
      exit(1);
    }
    break;
  }
  case C_BUILDLIST: {
    val l = val_list(gc, code.l);
    for (size_t i = vm->stack.l->len - code.l; i < vm->stack.l->len; i++)
      list_append(l, vm->stack.l->data[i]);
    vm->stack.l->len -= code.l;
    list_append(vm->stack, l);
    break;
  }
  case C_BUILDOBJ: {
    val d = val_obj(gc);
    for (size_t i = vm->stack.l->len - objkeys.v[code.l].len, j = 0;
         i < vm->stack.l->len; i++, j++)
      object_insert(d, objkeys.v[code.l].v[j], vm->stack.l->data[i]);
    vm->stack.l->len -= objkeys.v[code.l].len;
    list_append(vm->stack, d);
    break;
  }
  case C_INITOBJ: {
    val d = val_obj(gc);
    for (size_t i = vm->stack.l->len - objkeys.v[code.l].len, j = 0;
         i < vm->stack.l->len; i++, j++)
      object_insert(d, objkeys.v[code.l].v[j], vm->stack.l->data[i]);
    vm->stack.l->len -= objkeys.v[code.l].len;
    d.o->meta = vm->stack.l->data[vm->stack.l->len - 1];
    vm->stack.l->data[vm->stack.l->len - 1] = d;
    break;
  }
  case C_BUILDTYPE: {
    val d = val_type(gc);
    for (size_t i = vm->stack.l->len - objkeys.v[code.l].len, j = 0;
         i < vm->stack.l->len; i++, j++)
      object_insert(d, objkeys.v[code.l].v[j], vm->stack.l->data[i]);
    vm->stack.l->len -= objkeys.v[code.l].len;
    val parents = val_list(gc, objkeys.v[code.l].Tsize);
    for (size_t i = vm->stack.l->len - objkeys.v[code.l].Tsize;
         i < vm->stack.l->len; i++)
      list_append(parents, vm->stack.l->data[i]);
    vm->stack.l->len -= objkeys.v[code.l].Tsize;
    if (objkeys.v[code.l].Tsize)
      d.o->meta = parents;
    list_append(vm->stack, d);
    break;
  }
  case C_BUILDFUNC: {
    list_append(vm->stack, val_func(gc, vm->pc + 2, code.l, vm->env));
    break;
  }
  case C_BUILDMETHOD: {
    list_append(vm->stack, val_method(gc, vm->pc + 2, code.l, vm->env));
    break;
  }
  case C_INDEX: {
    val index = vm->stack.l->data[--vm->stack.l->len];
    val base = vm->stack.l->data[--vm->stack.l->len];
    if (base.tp == T_LIST || index.tp == T_INT) {
      // 没有必要做越界检查，反正也没有错误处理机制
      list_append(vm->stack, base.l->data[index.i]);
    } else if (base.tp == T_STR && index.tp == T_INT) {
      list_append(vm->stack, val_int(base.s->data[index.i]));
    } else if ((base.tp == T_OBJ || base.tp == T_TYPE) && index.tp == T_STR) {
      val *v = object_get(base, index.s->data);
      if (!v) {
        printf("Cannot find key %s in object\n", index.s->data);
        exit(1);
      }
      list_append(vm->stack, *v);
    } else {
      printf("Unsupported index operation\n");
      exit(1);
    }
    break;
  }
  case C_SETINDEX: {
    val rval = vm->stack.l->data[--vm->stack.l->len];
    val index = vm->stack.l->data[--vm->stack.l->len];
    val base = vm->stack.l->data[--vm->stack.l->len];
    if (base.tp == T_LIST && index.tp == T_INT) {
      base.l->data[index.i] = rval;
    } else if (base.tp == T_STR && index.tp == T_INT && rval.tp == T_INT) {
      base.s->data[index.i] = rval.i;
    } else if ((base.tp == T_OBJ || base.tp == T_TYPE) && index.tp == T_STR) {
      object_insert(base, index.s->data, rval);
    } else {
      printf("Unsupported setindex operation\n");
      exit(1);
    }
    break;
  }
  case C_CALL: {
    vm_startcall(vm, code.l);
    break;
  }
  case C_RET: {
    if (vm->envstack.l->len == 0) {
      printf("Return outside function\n");
      exit(1);
    }
    if (vm->cons_stack.v[--vm->cons_stack.len]) {
      vm->stack.l->data[vm->stack.l->len - 1] = vm->env.env->varlist.l->data[0];
    }
    vm->pc = seq_pop(vm->pcstack);
    vm->env = vm->envstack.l->data[--vm->envstack.l->len - 1];
    break;
  }
  case C_JMP: {
    vm->pc = code.l;
    break;
  }
  case C_JZ: {
    if (val_bool(vm->stack.l->data[--vm->stack.l->len]))
      vm->pc = code.l;
    break;
  }
  case C_JNZ: {
    if (!val_bool(vm->stack.l->data[--vm->stack.l->len]))
      vm->pc = code.l;
    break;
  }
  case C_JZNOPOP: {
    if (val_bool(vm->stack.l->data[vm->stack.l->len - 1]))
      vm->pc = code.l;
    break;
  }
  case C_JNZNOPOP: {
    if (!val_bool(vm->stack.l->data[vm->stack.l->len - 1]))
      vm->pc = code.l;
    break;
  }
  case C_LOADV: {
    val curenv = vm->env;
    for (unsigned int i = 0; i < code.v.scope; i++)
      curenv = curenv.env->parent;
    list_append(vm->stack, curenv.env->varlist.l->data[code.v.pos]);
    break;
  }
  case C_SETV: {
    val rval = vm->stack.l->data[--vm->stack.l->len];
    val curenv = vm->env;
    for (unsigned int i = 0; i < code.v.scope; i++)
      curenv = curenv.env->parent;
    curenv.env->varlist.l->data[code.v.pos] = rval;
    break;
  }
  case C_LOADEXT: {
    val *v = object_get(extglobal, code.s);
    if (!v) {
      printf("Cannot find key %s in external scope\n", code.s);
      exit(1);
    }
    list_append(vm->stack, *v);
    break;
  }
  case C_LOADEXT_SHORTSTR: {
    val *v = object_get_shortstr(extglobal, code.l);
    if (!v) {
      printf("Cannot find key in external scope\n");
      exit(1);
    }
    list_append(vm->stack, *v);
    break;
  }
  case C_ATTR: {
    val obj = vm->stack.l->data[--vm->stack.l->len];
    val *v = NULL;
    bool from_mt = false;
    if (obj.tp == T_OBJ || obj.tp == T_TYPE)
      v = object_get(obj, code.s);
    if (!v) {
      v = gc_mt_find(gc, obj.tp, code.s);
      from_mt = true;
      if (!v) {
        printf("Cannot find attribute %s in object\n", code.s);
        exit(1);
      }
    }
    val res = *v;
    if ((res.tp == T_CMETHOD || res.tp == T_METHOD ||
         res.tp == T_EXPANDED_METHOD) &&
        (obj.tp != T_TYPE || from_mt))
      res = val_expanded_method(gc, obj, res);
    list_append(vm->stack, res);
    break;
  }
  case C_ATTR_SHORTSTR: {
    val obj = vm->stack.l->data[--vm->stack.l->len];
    val *v = NULL;
    bool from_mt = false;
    if (obj.tp == T_OBJ || obj.tp == T_TYPE)
      v = object_get_shortstr(obj, code.l);
    if (!v) {
      v = gc_mt_find_shortstr(gc, obj.tp, code.l);
      from_mt = true;
      if (!v) {
        printf("Cannot find attribute shortstr %zu in object\n", code.l);
        exit(1);
      }
    }
    val res = *v;
    if ((res.tp == T_CMETHOD || res.tp == T_METHOD ||
         res.tp == T_EXPANDED_METHOD) &&
        (obj.tp != T_TYPE || from_mt))
      res = val_expanded_method(gc, obj, res);
    list_append(vm->stack, res);
    break;
  }
  case C_SETATTR: {
    val rval = vm->stack.l->data[--vm->stack.l->len];
    val obj = vm->stack.l->data[--vm->stack.l->len];
    if (!(obj.tp == T_OBJ || obj.tp == T_TYPE)) {
      printf("Cannot set attribute for non-object\n");
      exit(1);
    }
    object_insert(obj, code.s, rval);
    break;
  }
  default:
    printf("Unsupported opcode %d\n", code.head);
    exit(1);
  }
  vm->pc++;
  vm->run_cnt++;

  if (vm->run_cnt >= 200) {
    vm->run_cnt = 0;
    vm_gc_collect(vm);
  }
}

void vm_startcall(vm *vm, size_t nargs) {
  val funcobj = vm->stack.l->data[--vm->stack.l->len];
  val arglist = val_list(vm->gc, nargs);
  bool is_cons = false;
  while (1) {
    if (funcobj.tp == T_EXPANDED_METHOD) {
      list_insert(arglist, 0, funcobj.em->obj);
      funcobj = funcobj.em->method;
    } else if (funcobj.tp == T_TYPE && !is_cons) {
      val *cons = object_get(funcobj, "__init__");
      val d = val_obj(vm->gc);
      d.o->meta = funcobj;
      if (!cons) {
        vm->stack.l->len -= nargs;
        list_append(vm->stack, d);
        return;
      }
      list_insert(arglist, 0, d);
      funcobj = *cons;
      is_cons = true;
    } else {
      break;
    }
  }
  if (funcobj.tp == T_FUNC || funcobj.tp == T_METHOD) {
    seq_append(vm->cons_stack, is_cons);
    func *fn = funcobj.fn;
    val new_env = val_env(vm->gc, fn->env, arglist);
    for (size_t i = vm->stack.l->len - nargs; i < vm->stack.l->len; i++)
      list_append(new_env.env->varlist, vm->stack.l->data[i]);
    while (new_env.env->varlist.l->len < fn->reserve)
      list_append(new_env.env->varlist, val_nil);
    vm->stack.l->len -= nargs;
    list_append(vm->envstack, new_env);
    seq_append(vm->pcstack, vm->pc);
    vm->env = new_env;
    vm->pc = fn->pc;
  } else if (funcobj.tp == T_CFUNC || funcobj.tp == T_CMETHOD) {
    for (size_t i = vm->stack.l->len - nargs; i < vm->stack.l->len; i++)
      list_append(arglist, vm->stack.l->data[i]);
    vm->stack.l->len -= nargs;
    list_append(vm->stack, funcobj.cf(vm->gc, arglist.l->len, arglist.l->data));
  } else {
    printf("Unsupported function call (funcobj type %d)\n", funcobj.tp);
    exit(1);
  }
}

void vm_call(vm *vm, size_t nargs) {
  size_t env_len = vm->envstack.l->len;
  vm_startcall(vm, nargs);
  vm->pc++;
  while (vm->envstack.l->len > env_len) {
    vm_singlestep(vm);
    vm->run_cnt--;
  }
}

void vm_gc_collect(vm *vm) {
  val *to_free = gc_collect(vm->gc);
  for (val *i = to_free; i; i = gc_next(i)) {
    if (i->tp != T_OBJ)
      continue;
    val *del = object_get(*i, "__del__");
    if (!del || type_is_func(i->tp))
      continue;
    list_append(vm->stack, val_expanded_method(vm->gc, *i, *del));
    vm_call(vm, 0);
  }
  gc_free(to_free);
}

void vm_run(vm *vm) {
  while (vm->pc < vm->len && vm->bytecode[vm->pc].head != C_EXIT)
    vm_singlestep(vm);
}

void bytecode_print(vmcode code) {
  switch (code.head) {
  case C_EXIT:
    printf("EXIT");
    break;
  case C_NOOP:
    printf("NOOP");
    break;
  case C_PUSHI:
    printf("PUSHI %lld", code.i);
    break;
  case C_PUSHF:
    printf("PUSHF %f", code.f);
    break;
  case C_PUSHN:
    printf("PUSHN");
    break;
  case C_PUSHS:
    printf("PUSHS (%s)", code.s);
    break;
  case C_POP:
    printf("POP");
    break;
  case C_BINARY:
    printf("BINARY %d", code.op);
    break;
  case C_UNARY:
    printf("UNARY %d", code.op);
    break;
  case C_BUILDLIST:
    printf("BUILDLIST %zu", code.l);
    break;
  case C_BUILDOBJ:
    printf("BUILDOBJ");
    break;
  case C_INITOBJ:
    printf("INITOBJ");
    break;
  case C_BUILDTYPE:
    printf("BUILDTYPE");
    break;
  case C_BUILDFUNC:
    printf("BUILDFUNC %zu", code.l);
    break;
  case C_BUILDMETHOD:
    printf("BUILDMETHOD %zu", code.l);
    break;
  case C_INDEX:
    printf("INDEX");
    break;
  case C_SETINDEX:
    printf("SETINDEX");
    break;
  case C_ATTR:
    printf("ATTR %s", code.s);
    break;
  case C_ATTR_SHORTSTR:
    printf("ATTR_SHORTSTR %zu", code.l);
    break;
  case C_SETATTR:
    printf("SETATTR %s", code.s);
    break;
  case C_CALL:
    printf("CALL %zu", code.l);
    break;
  case C_RET:
    printf("RET");
    break;
  case C_JMP:
    printf("JMP %zu", code.l);
    break;
  case C_JZ:
    printf("JZ %zu", code.l);
    break;
  case C_JNZ:
    printf("JNZ %zu", code.l);
    break;
  case C_JZNOPOP:
    printf("JZNOPOP %zu", code.l);
    break;
  case C_JNZNOPOP:
    printf("JNZNOPOP %zu", code.l);
    break;
  case C_LOADV:
    printf("LOADV %u:%u", code.v.scope, code.v.pos);
    break;
  case C_SETV:
    printf("SETV %u:%u", code.v.scope, code.v.pos);
    break;
  case C_LOADEXT:
    printf("LOADEXT %s", code.s);
    break;
  case C_LOADEXT_SHORTSTR:
    printf("LOADEXT_SHORTSTR %zu", code.l);
    break;
  default:
    printf("Unknown opcode %d", code.head);
    break;
  }
}
