#include "ast.h"
#include <stdlib.h>

void stmt_free(statement *stmt) {
  switch (stmt->tp) {
  case S_NOOP:
  case S_BREAK:
  case S_CONTINUE:
    break;
  case S_BLOCK:
    for (size_t i = 0; i < stmt->block.len; i++)
      stmt_free(stmt->block.v[i]);
    free(stmt->block.v);
    break;
  case S_EXPR:
    expr_free(stmt->expr);
    break;
  case S_ASSIGN:
    expr_free(stmt->assign.left);
    expr_free(stmt->assign.right);
    break;
  case S_IF:
    for (size_t i = 0; i < stmt->if_stmt.cases.len; i++) {
      expr_free(stmt->if_stmt.cases.v[i].cond);
      stmt_free(stmt->if_stmt.cases.v[i].body);
    }
    free(stmt->if_stmt.cases.v);
    if (stmt->if_stmt.else_case)
      stmt_free(stmt->if_stmt.else_case);
    break;
  case S_WHILE:
    expr_free(stmt->while_stmt.cond);
    stmt_free(stmt->while_stmt.body);
    break;
  case S_VARDECL:
    for (size_t i = 0; i < stmt->vardecl.len; i++)
      expr_free(stmt->vardecl.v[i].init);
    free(stmt->vardecl.v);
    break;
  case S_FUNCDEF:
    free(stmt->funcdef.params.v);
    stmt_free(stmt->funcdef.body);
    break;
  case S_TYPESTMT:
    for (size_t i = 0; i < stmt->typestmt.decls.len; i++)
      expr_free(stmt->typestmt.decls.v[i].init);
    free(stmt->typestmt.decls.v);
    break;
  case S_RETURN:
    expr_free(stmt->return_stmt);
    break;
  }
  free(stmt);
}

void expr_free(expression *expr) {
  switch (expr->tp) {
  case E_NIL:
  case E_INT:
  case E_FLOAT:
  case E_STR:
  case E_ID:
    break;
  case E_BINARY:
    expr_free(expr->binary_expr.left);
    expr_free(expr->binary_expr.right);
    break;
  case E_UNARY:
    expr_free(expr->unary_expr.expr);
    break;
  case E_TERNARY:
    expr_free(expr->ternary_expr.cond);
    expr_free(expr->ternary_expr.t);
    expr_free(expr->ternary_expr.f);
    break;
  case E_LIST:
    for (size_t i = 0; i < expr->list_expr.len; i++)
      expr_free(expr->list_expr.v[i]);
    free(expr->list_expr.v);
    break;
  case E_FUNC:
    stmt_free(expr->func_expr.body);
    free(expr->func_expr.params.v);
    break;
  case E_TYPE:
    for (size_t i = 0; i < expr->type_expr.parents.len; i++)
      expr_free(expr->type_expr.parents.v[i]);
    free(expr->type_expr.parents.v);
    for (size_t i = 0; i < expr->type_expr.decls.len; i++)
      expr_free(expr->type_expr.decls.v[i].init);
    free(expr->type_expr.decls.v);
    break;
  case E_OBJECT:
    for (size_t i = 0; i < expr->object_expr.len; i++)
      expr_free(expr->object_expr.v[i].init);
    free(expr->object_expr.v);
    break;
  case E_INITOBJ:
    for (size_t i = 0; i < expr->initobj_expr.decls.len; i++)
      expr_free(expr->initobj_expr.decls.v[i].init);
    free(expr->initobj_expr.decls.v);
    expr_free(expr->initobj_expr.type);
    break;
  case E_METHOD:
    stmt_free(expr->method_expr.body);
    free(expr->method_expr.params.v);
    break;
  case E_CALL:
    expr_free(expr->call_expr.func);
    for (size_t i = 0; i < expr->call_expr.args.len; i++)
      expr_free(expr->call_expr.args.v[i]);
    free(expr->call_expr.args.v);
    break;
  case E_ATTR:
    expr_free(expr->attr_expr.base);
    break;
  case E_INDEX:
    expr_free(expr->index_expr.base);
    expr_free(expr->index_expr.index);
    break;
  }
  free(expr);
}

const char op_prio[] = {
    [O_MUL] = 100, [O_DIV] = 100, [O_MOD] = 100, [O_ADD] = 99,  [O_SUB] = 99,
    [O_LSH] = 98,  [O_RSH] = 98,  [O_EQ] = 97,   [O_NE] = 97,   [O_LT] = 96,
    [O_LE] = 96,   [O_GT] = 96,   [O_GE] = 96,   [O_BAND] = 95, [O_BOR] = 94,
    [O_XOR] = 93,  [O_AND] = 92,  [O_OR] = 91,
};
