#include "parse.h"
#include <stdio.h>
#include <stdlib.h>

#define match_tp(TP) (p->tokens.v[p->pos].tp == TP)

#define match(TP, FIELD, VAL)                                                  \
  (p->tokens.v[p->pos].tp == TP && p->tokens.v[p->pos].FIELD == VAL)

#define eat(...)                                                               \
  ({                                                                           \
    if (!match(__VA_ARGS__)) {                                                 \
      printf("Unexpected token ");                                             \
      token_print(p->tokens.v[p->pos]);                                        \
      putchar(' ');                                                            \
      puts(#__VA_ARGS__);                                                      \
      exit(1);                                                                 \
    }                                                                          \
    next();                                                                    \
  })

#define eat_tp(...)                                                            \
  ({                                                                           \
    if (!match_tp(__VA_ARGS__)) {                                              \
      printf("Unexpected token ");                                             \
      token_print(p->tokens.v[p->pos]);                                        \
      putchar(' ');                                                            \
      puts(#__VA_ARGS__);                                                      \
      exit(1);                                                                 \
    }                                                                          \
    next();                                                                    \
  })

#define next() (p->tokens.v[p->pos++])

decllist _parse_dict(parser *p) {
  decllist res = seq_init(decllist);
  eat(K_SYM, sym, X_LBRACE);
  while (!match(K_SYM, sym, X_RBRACE)) {
    char *name = eat_tp(K_ID).s;
    eat(K_SYM, sym, X_COLON);
    expression *init = parse_expr(p);
    decl_t decl = {name, init};
    seq_append(res, decl);
    if (!match(K_SYM, sym, X_RBRACE))
      eat(K_SYM, sym, X_COMMA);
  }
  next();
  return res;
}

struct typedecl_t _parse_type(parser *p) {
  exprlist parents = seq_init(exprlist);
  if (match(K_SYM, sym, X_LPAREN)) {
    next();
    while (!match(K_SYM, sym, X_RPAREN)) {
      seq_append(parents, parse_expr(p));
      if (!match(K_SYM, sym, X_RPAREN))
        eat(K_SYM, sym, X_COMMA);
    }
    next();
  }
  decllist decls = _parse_dict(p);
  return (struct typedecl_t){parents, decls};
}

struct funcdecl_t _parse_func(parser *p) {
  str_list params = seq_init(str_list);
  eat(K_SYM, sym, X_LPAREN);
  while (!match(K_SYM, sym, X_RPAREN)) {
    seq_append(params, eat_tp(K_ID).s);
    if (!match(K_SYM, sym, X_RPAREN))
      eat(K_SYM, sym, X_COMMA);
  }
  next();
  statement *body = parse_block(p);
  return (struct funcdecl_t){params, body};
}

parser parser_init(tokenlist tokens) {
  return (parser){.tokens = tokens, .pos = 0};
}

statement *parse(parser *p) {
  stmtlist stmts = seq_init(stmtlist);
  while (!match_tp(K_EOF)) {
    seq_append(stmts, parse_stmt(p));
  }
  statement *res = malloc(sizeof(statement));
  res->tp = S_BLOCK;
  res->block = stmts;
  return res;
}

statement *parse_block(parser *p) {
  eat(K_SYM, sym, X_LBRACE);
  stmtlist stmts = seq_init(stmtlist);
  while (!match(K_SYM, sym, X_RBRACE)) {
    seq_append(stmts, parse_stmt(p));
  }
  eat(K_SYM, sym, X_RBRACE);
  statement *res = malloc(sizeof(statement));
  res->tp = S_BLOCK;
  res->block = stmts;
  return res;
}

statement *parse_stmt(parser *p) {
  statement *res = NULL;
  if (match(K_SYM, sym, X_SEMI)) {
    next();
    res = malloc(sizeof(statement));
    res->tp = S_NOOP;
  } else if (match(K_KW, kw, X_IF)) {
    next();
    caselist cases = seq_init(caselist);
    statement *else_case = NULL;
    case_t cur_case;
    eat(K_SYM, sym, X_LPAREN);
    cur_case.cond = parse_expr(p);
    eat(K_SYM, sym, X_RPAREN);
    cur_case.body = parse_block(p);
    seq_append(cases, cur_case);
    while (match(K_KW, kw, X_ELSE)) {
      next();
      if (match(K_KW, kw, X_IF)) {
        next();
        eat(K_SYM, sym, X_LPAREN);
        cur_case.cond = parse_expr(p);
        eat(K_SYM, sym, X_RPAREN);
        cur_case.body = parse_block(p);
        seq_append(cases, cur_case);
      } else {
        else_case = parse_block(p);
        break;
      }
    }
    res = malloc(sizeof(statement));
    res->tp = S_IF;
    res->if_stmt.cases = cases;
    res->if_stmt.else_case = else_case;
  } else if (match(K_KW, kw, X_WHILE)) {
    next();
    eat(K_SYM, sym, X_LPAREN);
    expression *cond = parse_expr(p);
    eat(K_SYM, sym, X_RPAREN);
    statement *body = parse_block(p);
    res = malloc(sizeof(statement));
    res->tp = S_WHILE;
    res->while_stmt.cond = cond;
    res->while_stmt.body = body;
  } else if (match(K_KW, kw, X_VAR)) {
    next();
    decllist decls = seq_init(decllist);
    while (!match(K_SYM, sym, X_SEMI)) {
      decl_t decl = {eat_tp(K_ID).s, NULL};
      if (match(K_SYM, sym, X_ASSIGN)) {
        next();
        decl.init = parse_expr(p);
      } else {
        decl.init = malloc(sizeof(expression));
        decl.init->tp = E_NIL;
      }
      seq_append(decls, decl);
      if (!match(K_SYM, sym, X_SEMI))
        eat(K_SYM, sym, X_COMMA);
    }
    next();
    res = malloc(sizeof(statement));
    res->tp = S_VARDECL;
    res->vardecl = decls;
  } else if (match(K_KW, kw, X_FUNC) && p->tokens.v[p->pos + 1].tp == K_ID) {
    next();
    char *name = eat_tp(K_ID).s;
    struct funcdecl_t decl = _parse_func(p);
    res = malloc(sizeof(statement));
    res->tp = S_FUNCDEF;
    res->funcdef.name = name;
    res->funcdef.params = decl.params;
    res->funcdef.body = decl.body;
  } else if (match(K_KW, kw, X_TYPE) && p->tokens.v[p->pos + 1].tp == K_ID) {
    next();
    char *name = eat_tp(K_ID).s;
    struct typedecl_t decl = _parse_type(p);
    res = malloc(sizeof(statement));
    res->tp = S_TYPESTMT;
    res->typestmt.name = name;
    res->typestmt.parents = decl.parents;
    res->typestmt.decls = decl.decls;
  } else if (match(K_KW, kw, X_RETURN)) {
    next();
    expression *expr = parse_expr(p);
    res = malloc(sizeof(statement));
    res->tp = S_RETURN;
    res->return_stmt = expr;
    eat(K_SYM, sym, X_SEMI);
  } else if (match(K_KW, kw, X_BREAK)) {
    next();
    res = malloc(sizeof(statement));
    res->tp = S_BREAK;
    eat(K_SYM, sym, X_SEMI);
  } else if (match(K_KW, kw, X_CONTINUE)) {
    next();
    res = malloc(sizeof(statement));
    res->tp = S_CONTINUE;
    eat(K_SYM, sym, X_SEMI);
  } else {
    expression *expr = parse_expr(p);
    if (match(K_SYM, sym, X_ASSIGN)) {
      if (expr->tp != E_ID && expr->tp != E_ATTR && expr->tp != E_INDEX) {
        printf("Invalid lvalue\n");
        exit(1);
      }
      next();
      expression *right = parse_expr(p);
      res = malloc(sizeof(statement));
      res->tp = S_ASSIGN;
      res->assign.left = expr;
      res->assign.right = right;
      eat(K_SYM, sym, X_SEMI);
    } else {
      res = malloc(sizeof(statement));
      res->tp = S_EXPR;
      res->expr = expr;
      eat(K_SYM, sym, X_SEMI);
    }
  }
  return res;
}

expression *parse_expr(parser *p) {
  exprlist res = seq_init(exprlist);
  op_list stack = seq_init(op_list);
  seq_append(res, parse_primary(p));
  while (match_tp(K_OP) && p->tokens.v[p->pos].op <= O_XOR) {
    operator_t op = next().op;
    while (stack.len && op_prio[stack.v[stack.len - 1]] >= op_prio[op]) {
      expression *right = res.v[--res.len];
      expression *left = res.v[--res.len];
      expression *expr = malloc(sizeof(expression));
      expr->tp = E_BINARY;
      expr->binary_expr.op = stack.v[--stack.len];
      expr->binary_expr.left = left;
      expr->binary_expr.right = right;
      seq_append(res, expr);
    }
    seq_append(stack, op);
    seq_append(res, parse_primary(p));
  }
  while (stack.len) {
    expression *right = res.v[--res.len];
    expression *left = res.v[--res.len];
    expression *expr = malloc(sizeof(expression));
    expr->tp = E_BINARY;
    expr->binary_expr.op = stack.v[--stack.len];
    expr->binary_expr.left = left;
    expr->binary_expr.right = right;
    seq_append(res, expr);
  }
  expression *res_expr = res.v[0];
  free(res.v);
  free(stack.v);
  return res_expr;
}

expression *parse_primary(parser *p) {
  expression *res = NULL;
  if (match_tp(K_EOF)) {
    printf("Unexpected EOF\n");
    exit(1);
  } else if (match_tp(K_INT)) {
    res = malloc(sizeof(expression));
    res->tp = E_INT;
    res->int_expr = next().i;
  } else if (match_tp(K_FLOAT)) {
    res = malloc(sizeof(expression));
    res->tp = E_FLOAT;
    res->float_expr = next().f;
  } else if (match_tp(K_STR)) {
    res = malloc(sizeof(expression));
    res->tp = E_STR;
    res->str_expr = next().s;
  } else if (match(K_KW, kw, X_NIL)) {
    next();
    res = malloc(sizeof(expression));
    res->tp = E_NIL;
  } else if (match_tp(K_ID)) {
    res = malloc(sizeof(expression));
    res->tp = E_ID;
    res->id_expr = next().s;
  } else if (match(K_SYM, sym, X_LPAREN)) {
    next();
    res = parse_expr(p);
    eat(K_SYM, sym, X_RPAREN);
  } else if (match(K_SYM, sym, X_LSQBR)) {
    next();
    exprlist items = seq_init(exprlist);
    while (!match(K_SYM, sym, X_RSQBR)) {
      seq_append(items, parse_expr(p));
      if (!match(K_SYM, sym, X_RSQBR))
        eat(K_SYM, sym, X_COMMA);
    }
    next();
    res = malloc(sizeof(expression));
    res->tp = E_LIST;
    res->list_expr = items;
  } else if (match(K_SYM, sym, X_LBRACE)) {
    res = malloc(sizeof(expression));
    res->tp = E_OBJECT;
    res->object_expr = _parse_dict(p);
  } else if (match(K_KW, kw, X_TYPE)) {
    next();
    res = malloc(sizeof(expression));
    res->tp = E_TYPE;
    res->type_expr = _parse_type(p);
  } else if (match_tp(K_OP) && (p->tokens.v[p->pos].op == O_ADD ||
                                p->tokens.v[p->pos].op == O_SUB ||
                                p->tokens.v[p->pos].op == O_NOT ||
                                p->tokens.v[p->pos].op == O_INV)) {
    res = malloc(sizeof(expression));
    res->tp = E_UNARY;
    res->unary_expr.op = next().op;
    if (res->unary_expr.op == O_SUB) {
      res->unary_expr.op = O_NEG;
    } else if (res->unary_expr.op == O_ADD) {
      res->unary_expr.op = O_POS;
    }
    res->unary_expr.expr = parse_primary(p);
  } else if (match(K_KW, kw, X_FUNC) || match(K_KW, kw, X_METHOD)) {
    kw_token kw = next().kw;
    res = malloc(sizeof(expression));
    res->tp = kw == X_FUNC ? E_FUNC : E_METHOD;
    res->func_expr = _parse_func(p);
  } else {
    printf("Unexpected token\n");
    exit(1);
  }

  while (1) {
    if (match(K_SYM, sym, X_LSQBR)) {
      next();
      expression *nxt = malloc(sizeof(expression));
      nxt->tp = E_INDEX;
      nxt->index_expr.base = res;
      nxt->index_expr.index = parse_expr(p);
      res = nxt;
      eat(K_SYM, sym, X_RSQBR);
    } else if (match(K_SYM, sym, X_DOT)) {
      next();
      expression *nxt = malloc(sizeof(expression));
      nxt->tp = E_ATTR;
      nxt->attr_expr.base = res;
      nxt->attr_expr.attr = eat_tp(K_ID).s;
      res = nxt;
    } else if (match(K_SYM, sym, X_LPAREN)) {
      next();
      exprlist args = seq_init(exprlist);
      while (!match(K_SYM, sym, X_RPAREN)) {
        seq_append(args, parse_expr(p));
        if (!match(K_SYM, sym, X_RPAREN))
          eat(K_SYM, sym, X_COMMA);
      }
      next();
      expression *nxt = malloc(sizeof(expression));
      nxt->tp = E_CALL;
      nxt->call_expr.func = res;
      nxt->call_expr.args = args;
      res = nxt;
    } else if (match(K_SYM, sym, X_LBRACE)) {
      decllist d = _parse_dict(p);
      expression *nxt = malloc(sizeof(expression));
      nxt->tp = E_INITOBJ;
      nxt->initobj_expr.type = res;
      nxt->initobj_expr.decls = d;
      res = nxt;
    } else {
      break;
    }
  }

  return res;
}
