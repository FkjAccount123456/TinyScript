#include "compile.h"
#include <stdio.h>

compiler compiler_new() {
  compiler c;
  c.code = seq_init(vmcodelist);
  c.w_jmps = seq_init(pc_list);
  c.w_begin = seq_init(pc_list);
  c.objkeys = seq_init(str_list_2);
  return c;
}

void compiler_free_data(compiler *c) {
  free(c->w_jmps.v);
  free(c->w_begin.v);
}

void compiler_free_code(compiler *c) {
  free(c->code.v);
  for (size_t i = 0; i < c->objkeys.len; i++)
    free(c->objkeys.v[i].v);
  free(c->objkeys.v);
}

void compile_hoist(statement *node, str_list *output) {
  if (node->tp == S_BLOCK) {
    for (size_t i = 0; i < node->block.len; i++)
      compile_hoist(node->block.v[i], output);
  } else if (node->tp == S_VARDECL) {
    for (size_t i = 0; i < node->vardecl.len; i++)
      seq_append(*output, node->vardecl.v[i].name);
  } else if (node->tp == S_FUNCDEF) {
    seq_append(*output, node->funcdef.name);
  } else if (node->tp == S_TYPESTMT) {
    seq_append(*output, node->typestmt.name);
  }
}

varpos compile_find_var(compiler *c, char *name) {
  val scope = c->scope;
  unsigned int scnt = 0;
  while (scope.tp == T_OBJ) {
    val *v = _object_get(scope, name);
    if (v)
      return (varpos){scnt, v->i};
    scope = scope.o->meta;
    scnt++;
  }
  return (varpos){-1, -1};
}

void compile_expr(gc_root *gc, compiler *c, expression *node);

void compile_stmt(gc_root *gc, compiler *c, statement *node) {
  switch (node->tp) {
  case S_NOOP:
    break;
  case S_BLOCK:
    for (size_t i = 0; i < node->block.len; i++)
      compile_stmt(gc, c, node->block.v[i]);
    break;
  case S_EXPR:
    compile_expr(gc, c, node->expr);
    compiler_add(bytecode_new(C_POP));
    break;
  case S_ASSIGN: {
    expression *l = node->assign.left, *r = node->assign.right;
    if (l->tp == E_ID) {
      compile_expr(gc, c, r);
      varpos v = compile_find_var(c, l->id_expr);
      if (v.scope == -1) {
        printf("Undefined variable: %s\n", l->id_expr);
        exit(1);
      }
      compiler_add(bytecode_init(C_SETV, v, v));
    } else if (l->tp == E_ATTR) {
      compile_expr(gc, c, l->attr_expr.base);
      compile_expr(gc, c, r);
      compiler_add(bytecode_init(C_SETATTR, s, l->attr_expr.attr));
    } else if (l->tp == E_INDEX) {
      compile_expr(gc, c, l->index_expr.base);
      compile_expr(gc, c, l->index_expr.index);
      compile_expr(gc, c, r);
      compiler_add(bytecode_new(C_SETINDEX));
    } else {
      printf("Invalid l-value of assignment\n");
      exit(1);
    }
    break;
  }
  case S_IF: {
    // 和integer3.3 Rain1.1 betterlangv2一样照搬的betterlangv1（
    pc_list jmps = seq_init(pc_list);
    size_t jnz = 0;
    for (size_t i = 0; i < node->if_stmt.cases.len; i++) {
      case_t case_node = node->if_stmt.cases.v[i];
      compile_expr(gc, c, case_node.cond);
      jnz = c->code.len;
      compiler_add(bytecode_new(C_JNZ));
      compile_stmt(gc, c, case_node.body);
      seq_append(jmps, c->code.len);
      compiler_add(bytecode_new(C_JMP));
      c->code.v[jnz].l = c->code.len - 1;
    }
    if (node->if_stmt.else_case)
      compile_stmt(gc, c, node->if_stmt.else_case);
    for (size_t i = 0; i < jmps.len; i++)
      c->code.v[jmps.v[i]].l = c->code.len;
    free(jmps.v);
    break;
  }
  case S_WHILE: {
    seq_append(c->w_jmps, 0);
    seq_append(c->w_begin, c->code.len - 1);
    compile_expr(gc, c, node->while_stmt.cond);
    seq_append(c->w_jmps, c->code.len);
    compiler_add(bytecode_new(C_JNZ));
    compile_stmt(gc, c, node->while_stmt.body);
    compiler_add(bytecode_init(C_JMP, l, c->w_begin.v[--c->w_begin.len]));
    while (c->w_jmps.len && c->w_jmps.v[--c->w_jmps.len] != 0)
      c->code.v[c->w_jmps.v[c->w_jmps.len]].l = c->code.len - 1;
    break;
  }
  case S_VARDECL: {
    for (size_t i = 0; i < node->vardecl.len; i++) {
      decl_t decl = node->vardecl.v[i];
      compile_expr(gc, c, decl.init);
      varpos v = compile_find_var(c, decl.name);
      compiler_add(bytecode_init(C_SETV, v, v));
    }
    break;
  }
  case S_FUNCDEF: {
    str_list new_vars = seq_init(str_list);
    for (size_t i = 0; i < node->funcdef.params.len; i++)
      seq_append(new_vars, node->funcdef.params.v[i]);
    compile_hoist(node->funcdef.body, &new_vars);
    compiler_add(bytecode_init(C_BUILDFUNC, l, new_vars.len));
    varpos v = compile_find_var(c, node->funcdef.name);
    compiler_add(bytecode_init(C_SETV, v, v));
    size_t pos = c->code.len;
    compiler_add(bytecode_new(C_JMP));
    val new_scope = val_obj(gc);
    new_scope.o->meta = c->scope;
    for (size_t i = 0; i < new_vars.len; i++)
      object_insert(new_scope, new_vars.v[i], val_int(i));
    c->scope = new_scope;
    compile_stmt(gc, c, node->funcdef.body);
    if (c->code.v[c->code.len - 1].head != C_RET) {
      compiler_add(bytecode_new(C_PUSHN));
      compiler_add(bytecode_new(C_RET));
    }
    c->code.v[pos].l = c->code.len - 1;
    c->scope = c->scope.o->meta;
    seq_free(new_vars);
    break;
  }
  case S_TYPESTMT: {
    seq_append(c->objkeys, seq_init(str_list));
    for (size_t i = 0; i < node->typestmt.parents.len; i++)
      compile_expr(gc, c, node->typestmt.parents.v[i]);
    for (size_t i = 0; i < node->typestmt.decls.len; i++) {
      decl_t decl = node->typestmt.decls.v[i];
      seq_append(c->objkeys.v[c->objkeys.len - 1], decl.name);
      compile_expr(gc, c, decl.init);
    }
    c->objkeys.v[c->objkeys.len - 1].Tsize = node->typestmt.parents.len;
    compiler_add(bytecode_init(C_BUILDTYPE, kl, &c->objkeys.v[c->objkeys.len - 1]));
    varpos v = compile_find_var(c, node->typestmt.name);
    compiler_add(bytecode_init(C_SETV, v, v));
    break;
  }
  case S_RETURN: {
    compile_expr(gc, c, node->return_stmt);
    compiler_add(bytecode_new(C_RET));
    break;
  }
  case S_BREAK: {
    seq_append(c->w_jmps, c->code.len);
    compiler_add(bytecode_new(C_JMP));
    break;
  }
  case S_CONTINUE: {
    compiler_add(bytecode_init(C_JMP, l, c->w_begin.v[c->w_begin.len - 1]));
    break;
  }
  default:
    printf("Invalid statement type\n");
    exit(1);
  }
}

void compile_expr(gc_root *gc, compiler *c, expression *node) {
  switch (node->tp) {
  case E_INT:
    compiler_add(bytecode_init(C_PUSHI, i, node->int_expr));
    break;
  case E_FLOAT:
    compiler_add(bytecode_init(C_PUSHF, f, node->float_expr));
    break;
  case E_STR:
    compiler_add(bytecode_init(C_PUSHS, s, node->str_expr));
    break;
  case E_NIL:
    compiler_add(bytecode_new(C_PUSHN));
    break;
  case E_ID: {
    varpos v = compile_find_var(c, node->id_expr);
    if (v.scope == -1)
      compiler_add(bytecode_init(C_LOADEXT, s, node->id_expr));
    else
      compiler_add(bytecode_init(C_LOADV, v, v));
    break;
  }
  case E_BINARY: {
    if (node->binary_expr.op == O_OR || node->binary_expr.op == O_AND) {
      compile_expr(gc, c, node->binary_expr.left);
      size_t jnz = c->code.len;
      compiler_add(bytecode_new(node->binary_expr.op == O_OR ? C_JZNOPOP : C_JNZNOPOP));
      compiler_add(bytecode_new(C_POP));
      compile_expr(gc, c, node->binary_expr.right);
      c->code.v[jnz].l = c->code.len - 1;
    } else {
      compile_expr(gc, c, node->binary_expr.left);
      compile_expr(gc, c, node->binary_expr.right);
      compiler_add(bytecode_init(C_BINARY, op, node->binary_expr.op));
    }
    break;
  }
  case E_UNARY: {
    compile_expr(gc, c, node->unary_expr.expr);
    compiler_add(bytecode_init(C_UNARY, op, node->unary_expr.op));
    break;
  }
  case E_TERNARY: {
    compile_expr(gc, c, node->ternary_expr.cond);
    size_t jnz = c->code.len;
    compiler_add(bytecode_new(C_JNZ));
    compile_expr(gc, c, node->ternary_expr.t);
    size_t jmp = c->code.len;
    c->code.v[jnz].l = c->code.len;
    compiler_add(bytecode_new(C_JMP));
    compile_expr(gc, c, node->ternary_expr.f);
    c->code.v[jmp].l = c->code.len - 1;
    break;
  }
  case E_LIST: {
    for (size_t i = 0; i < node->list_expr.len; i++)
      compile_expr(gc, c, node->list_expr.v[i]);
    compiler_add(bytecode_init(C_BUILDLIST, l, node->list_expr.len));
    break;
  }
  case E_OBJECT: {
    seq_append(c->objkeys, seq_init(str_list));
    for (size_t i = 0; i < node->object_expr.len; i++) {
      decl_t decl = node->object_expr.v[i];
      seq_append(c->objkeys.v[c->objkeys.len - 1], decl.name);
      compile_expr(gc, c, decl.init);
    }
    compiler_add(bytecode_init(C_BUILDOBJ, kl, &c->objkeys.v[c->objkeys.len - 1]));
    break;
  }
  case E_INITOBJ: {
    seq_append(c->objkeys, seq_init(str_list));
    compile_expr(gc, c, node->initobj_expr.type);
    for (size_t i = 0; i < node->initobj_expr.decls.len; i++) {
      decl_t decl = node->initobj_expr.decls.v[i];
      seq_append(c->objkeys.v[c->objkeys.len - 1], decl.name);
      compile_expr(gc, c, decl.init);
    }
    compiler_add(bytecode_init(C_INITOBJ, kl, &c->objkeys.v[c->objkeys.len - 1]));
    break;
  }
  case E_TYPE: {
    seq_append(c->objkeys, seq_init(str_list));
    for (size_t i = 0; i < node->type_expr.parents.len; i++)
      compile_expr(gc, c, node->type_expr.parents.v[i]);
    for (size_t i = 0; i < node->type_expr.decls.len; i++) {
      decl_t decl = node->type_expr.decls.v[i];
      seq_append(c->objkeys.v[c->objkeys.len - 1], decl.name);
      compile_expr(gc, c, decl.init);
    }
    c->objkeys.v[c->objkeys.len - 1].Tsize = node->type_expr.parents.len;
    compiler_add(bytecode_init(C_BUILDTYPE, kl, &c->objkeys.v[c->objkeys.len - 1]));
    break;
  }
  case E_METHOD:
  case E_FUNC: {
    str_list new_vars = seq_init(str_list);
    for (size_t i = 0; i < node->func_expr.params.len; i++)
      seq_append(new_vars, node->func_expr.params.v[i]);
    compile_hoist(node->func_expr.body, &new_vars);
    compiler_add(bytecode_init(node->tp == E_METHOD ? C_BUILDMETHOD : C_BUILDFUNC, l, new_vars.len));
    compiler_add(bytecode_new(C_NOOP));
    size_t pos = c->code.len;
    compiler_add(bytecode_new(C_JMP));
    val new_scope = val_obj(gc);
    new_scope.o->meta = c->scope;
    for (size_t i = 0; i < new_vars.len; i++)
      object_insert(new_scope, new_vars.v[i], val_int(i));
    c->scope = new_scope;
    compile_stmt(gc, c, node->func_expr.body);
    if (c->code.v[c->code.len - 1].head != C_RET) {
      compiler_add(bytecode_new(C_PUSHN));
      compiler_add(bytecode_new(C_RET));
    }
    c->code.v[pos].l = c->code.len - 1;
    c->scope = c->scope.o->meta;
    seq_free(new_vars);
    break;
  }
  case E_CALL: {
    for (size_t i = 0; i < node->call_expr.args.len; i++)
      compile_expr(gc, c, node->call_expr.args.v[i]);
    compile_expr(gc, c, node->call_expr.func);
    compiler_add(bytecode_init(C_CALL, l, node->call_expr.args.len));
    break;
  }
  case E_ATTR: {
    compile_expr(gc, c, node->attr_expr.base);
    compiler_add(bytecode_init(C_ATTR, s, node->attr_expr.attr));
    break;
  }
  case E_INDEX: {
    compile_expr(gc, c, node->index_expr.base);
    compile_expr(gc, c, node->index_expr.index);
    compiler_add(bytecode_new(C_INDEX));
    break;
  }
  }
}

size_t compile_program(gc_root *gc, compiler *c, statement *node) {
  str_list new_vars = seq_init(str_list);
  compile_hoist(node, &new_vars);
  val new_scope = val_obj(gc);
  new_scope.o->meta = val_nil;
  for (size_t i = 0; i < new_vars.len; i++)
    object_insert(new_scope, new_vars.v[i], val_int(i));
  c->scope = new_scope;
  compile_stmt(gc, c, node);
  c->scope = val_nil;
  free(new_vars.v);
  return new_vars.len;
}
