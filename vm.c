#include "vm.h"
#include "vals.h"
#include <stdio.h>
#include <string.h>

void run(gc_root *gc, vmcodelist codelist, size_t reserve, val extglobal, str_list_2 objkeys) {
  vmcode *bytecode = codelist.v;
  size_t pc = 0;

  val stack = val_list(gc, 64);
  gc_add_root(gc, stack);

  val env = val_env(gc, val_nil, val_list(gc, 64));

  val envstack = val_list(gc, 8);
  pc_list pcstack = seq_init(pc_list);

  typedef struct seq(bool) bool_list;
  bool_list cons_stack = seq_init(bool_list);

  list_append(envstack, env);
  gc_add_root(gc, envstack);

  while (env.env->varlist.l->len < reserve)
    list_append(env.env->varlist, val_nil);

  size_t run_cnt = 0;
  while (pc < codelist.len && bytecode[pc].head != C_EXIT) {
    vmcode code = bytecode[pc];
    // for (size_t i = 0; i < stack.l->len; i++)
    //   val_debug(stack.l->data[i]), putchar(' ');
    // puts("");
    // printf("%zu ", pc), bytecode_print(code), printf("\n");
    switch (code.head) {
    case C_EXIT:
    case C_NOOP:
      break;
    case C_PUSHI:
      list_append(stack, val_int(code.i));
      break;
    case C_PUSHF:
      list_append(stack, val_float(code.f));
      break;
    case C_PUSHN:
      list_append(stack, val_nil);
      break;
    case C_PUSHS:
      list_append(stack, val_str(gc, code.s, strlen(code.s)));
      break;
    case C_POP:
      stack.l->len--;
      break;
    case C_BINARY: {
      val b = stack.l->data[--stack.l->len];
      val a = stack.l->data[--stack.l->len];
      switch (code.op) {
      case O_ADD:
        list_append(stack, val_add(a, b));
        break;
      case O_SUB:
        list_append(stack, val_sub(a, b));
        break;
      case O_MUL:
        list_append(stack, val_mul(a, b));
        break;
      case O_DIV:
        list_append(stack, val_div(a, b));
        break;
      case O_MOD:
        list_append(stack, val_mod(a, b));
        break;
      case O_LSH:
        list_append(stack, val_lsh(a, b));
        break;
      case O_RSH:
        list_append(stack, val_rsh(a, b));
        break;
      case O_LT:
        list_append(stack, val_int(val_lt(a, b)));
        break;
      case O_LE:
        list_append(stack, val_int(val_le(a, b)));
        break;
      case O_GT:
        list_append(stack, val_int(!val_le(a, b)));
        break;
      case O_GE:
        list_append(stack, val_int(!val_lt(a, b)));
        break;
      case O_EQ:
        list_append(stack, val_int(val_eq(a, b)));
        break;
      case O_NE:
        list_append(stack, val_int(!val_eq(a, b)));
        break;
      case O_BAND:
        list_append(stack, val_band(a, b));
        break;
      case O_BOR:
        list_append(stack, val_bor(a, b));
        break;
      case O_XOR:
        list_append(stack, val_xor(a, b));
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
        stack.l->data[stack.l->len - 1] =
            val_neg(stack.l->data[stack.l->len - 1]);
        break;
      case O_NOT:
        stack.l->data[stack.l->len - 1] =
            val_int(val_bool(stack.l->data[stack.l->len - 1]));
        break;
      case O_INV:
        stack.l->data[stack.l->len - 1] =
            val_inv(stack.l->data[stack.l->len - 1]);
        break;
      default:
        printf("Unsupported operator %d\n", code.op);
        exit(1);
      }
      break;
    }
    case C_BUILDLIST: {
      val l = val_list(gc, code.l);
      for (size_t i = stack.l->len - code.l; i < stack.l->len; i++)
        list_append(l, stack.l->data[i]);
      stack.l->len -= code.l;
      list_append(stack, l);
      break;
    }
    case C_BUILDOBJ: {
      val d = val_obj(gc);
      for (size_t i = stack.l->len - objkeys.v[code.l].len, j = 0; i < stack.l->len;
           i++, j++)
        object_insert(d, objkeys.v[code.l].v[j], stack.l->data[i]);
      stack.l->len -= objkeys.v[code.l].len;
      list_append(stack, d);
      break;
    }
    case C_INITOBJ: {
      val d = val_obj(gc);
      for (size_t i = stack.l->len - objkeys.v[code.l].len, j = 0; i < stack.l->len;
           i++, j++)
        object_insert(d, objkeys.v[code.l].v[j], stack.l->data[i]);
      stack.l->len -= objkeys.v[code.l].len;
      d.o->meta = stack.l->data[stack.l->len - 1];
      stack.l->data[stack.l->len - 1] = d;
      break;
    }
    case C_BUILDTYPE: {
      val d = val_type(gc);
      for (size_t i = stack.l->len - objkeys.v[code.l].len, j = 0; i < stack.l->len;
           i++, j++)
        object_insert(d, objkeys.v[code.l].v[j], stack.l->data[i]);
      stack.l->len -= objkeys.v[code.l].len;
      val parents = val_list(gc, objkeys.v[code.l].Tsize);
      for (size_t i = stack.l->len - objkeys.v[code.l].Tsize; i < stack.l->len; i++)
        list_append(parents, stack.l->data[i]);
      stack.l->len -= objkeys.v[code.l].Tsize;
      if (objkeys.v[code.l].Tsize)
        d.o->meta = parents;
      list_append(stack, d);
      break;
    }
    case C_BUILDFUNC: {
      list_append(stack, val_func(gc, pc + 2, code.l, env));
      break;
    }
    case C_BUILDMETHOD: {
      list_append(stack, val_method(gc, pc + 2, code.l, env));
      break;
    }
    case C_INDEX: {
      val index = stack.l->data[--stack.l->len];
      val base = stack.l->data[--stack.l->len];
      if (base.tp == T_LIST || index.tp == T_INT) {
        // 没有必要做越界检查，反正也没有错误处理机制
        list_append(stack, base.l->data[index.i]);
      } else if (base.tp == T_STR && index.tp == T_INT) {
        list_append(stack, val_int(base.s->data[index.i]));
      } else if ((base.tp == T_OBJ || base.tp == T_TYPE) && index.tp == T_STR) {
        val *v = object_get(base, index.s->data);
        if (!v) {
          printf("Cannot find key %s in object\n", index.s->data);
          exit(1);
        }
        list_append(stack, *v);
      } else {
        printf("Unsupported index operation\n");
        exit(1);
      }
      break;
    }
    case C_SETINDEX: {
      val rval = stack.l->data[--stack.l->len];
      val index = stack.l->data[--stack.l->len];
      val base = stack.l->data[--stack.l->len];
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
      val funcobj = stack.l->data[--stack.l->len];
      val arglist = val_list(gc, code.l);
      bool is_cons = false;
      while (1) {
        if (funcobj.tp == T_EXPANDED_METHOD) {
          list_insert(arglist, 0, funcobj.em->obj);
          funcobj = funcobj.em->method;
        } else if (funcobj.tp == T_TYPE && !is_cons) {
          val *cons = object_get(funcobj, "__init__");
          val d = val_obj(gc);
          d.o->meta = funcobj;
          if (!cons) {
            stack.l->len -= code.l;
            list_append(stack, d);
            goto call_end; // goto好东西啊（
          }
          list_insert(arglist, 0, d);
          funcobj = *cons;
          is_cons = true;
        } else {
          break;
        }
      }
      if (funcobj.tp == T_FUNC || funcobj.tp == T_METHOD) {
        seq_append(cons_stack, is_cons);
        func *fn = funcobj.fn;
        val new_env = val_env(gc, fn->env, arglist);
        for (size_t i = stack.l->len - code.l; i < stack.l->len; i++)
          list_append(new_env.env->varlist, stack.l->data[i]);
        while (new_env.env->varlist.l->len < fn->reserve)
          list_append(new_env.env->varlist, val_nil);
        stack.l->len -= code.l;
        list_append(envstack, new_env);
        seq_append(pcstack, pc);
        env = new_env;
        pc = fn->pc;
      } else if (funcobj.tp == T_CFUNC || funcobj.tp == T_CMETHOD) {
        for (size_t i = stack.l->len - code.l; i < stack.l->len; i++)
          list_append(arglist, stack.l->data[i]);
        stack.l->len -= code.l;
        list_append(stack, funcobj.cf(gc, arglist.l->len, arglist.l->data));
      } else {
        printf("Unsupported function call (funcobj type %d)\n", funcobj.tp);
        exit(1);
      }
call_end:
      break;
    }
    case C_RET: {
      if (envstack.l->len == 0) {
        printf("Return outside function\n");
        exit(1);
      }
      if (cons_stack.v[--cons_stack.len]) {
        stack.l->data[stack.l->len - 1] = env.env->varlist.l->data[0];
      }
      pc = seq_pop(pcstack);
      env = envstack.l->data[--envstack.l->len - 1];
      break;
    }
    case C_JMP: {
      pc = code.l;
      break;
    }
    case C_JZ: {
      if (val_bool(stack.l->data[--stack.l->len]))
        pc = code.l;
      break;
    }
    case C_JNZ: {
      if (!val_bool(stack.l->data[--stack.l->len]))
        pc = code.l;
      break;
    }
    case C_JZNOPOP: {
      if (val_bool(stack.l->data[stack.l->len - 1]))
        pc = code.l;
      break;
    }
    case C_JNZNOPOP: {
      if (!val_bool(stack.l->data[stack.l->len - 1]))
        pc = code.l;
      break;
    }
    case C_LOADV: {
      val curenv = env;
      for (unsigned int i = 0; i < code.v.scope; i++)
        curenv = curenv.env->parent;
      list_append(stack, curenv.env->varlist.l->data[code.v.pos]);
      break;
    }
    case C_SETV: {
      val rval = stack.l->data[--stack.l->len];
      val curenv = env;
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
      list_append(stack, *v);
      break;
    }
    case C_LOADEXT_SHORTSTR: {
      val *v = object_get_shortstr(extglobal, code.l);
      if (!v) {
        printf("Cannot find key in external scope\n");
        exit(1);
      }
      list_append(stack, *v);
      break;
    }
    case C_ATTR: {
      val obj = stack.l->data[--stack.l->len];
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
      list_append(stack, res);
      break;
    }
    case C_ATTR_SHORTSTR: {
      val obj = stack.l->data[--stack.l->len];
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
      list_append(stack, res);
      break;
    }
    case C_SETATTR: {
      val rval = stack.l->data[--stack.l->len];
      val obj = stack.l->data[--stack.l->len];
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
    pc++;
    run_cnt++;
    if (run_cnt > 200) {
      // 早期实现先不管析构函数了
      gc_free(gc_collect(gc));
      run_cnt = 0;
    }
  }

  gc->len -= 2;
  gc_free(gc_collect(gc));
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
