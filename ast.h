#ifndef AST_H
#define AST_H

#include "vals.h"
#include "seq.h"

typedef struct statement statement;
typedef struct expression expression;

typedef enum stmt_type {
  S_NOOP,
  S_BLOCK,
  S_EXPR,
  S_ASSIGN,
  S_IF,
  S_WHILE,
  S_VARDECL,
  S_FUNCDEF,
  S_TYPESTMT,
  S_RETURN,
  S_BREAK,
  S_CONTINUE,
} stmt_type;

typedef enum expr_type {
  E_NIL,
  E_INT,
  E_FLOAT,
  E_STR,
  E_ID,
  E_BINARY,
  E_UNARY,
  E_TERNARY,
  E_LIST,
  E_FUNC,
  E_TYPE,
  E_OBJECT,
  E_INITOBJ,
  E_METHOD,
  E_CALL,
  E_ATTR,
  E_INDEX,
} expr_type;

typedef enum operator_t {
  O_ADD,
  O_SUB,
  O_MUL,
  O_DIV,
  O_MOD,
  O_LSH,
  O_RSH,
  O_LT,
  O_LE,
  O_GT,
  O_GE,
  O_EQ,
  O_NE,
  O_AND,
  O_OR,
  O_BAND,
  O_BOR,
  O_XOR,

  O_POS,
  O_NEG,
  O_NOT,
  O_INV,
} operator_t;

typedef struct seq(statement *) stmtlist;
typedef struct seq(expression *) exprlist;

typedef struct {
  char *name;
  expression *init;
} decl_t;

typedef struct {
  expression *cond;
  statement *body;
} case_t;

typedef struct seq(decl_t) decllist;
typedef struct seq(case_t) caselist;

typedef struct seq(char) raw_str;
typedef struct seq(char *) str_list;
typedef struct seq(operator_t) op_list;

extern const char op_prio[];

typedef struct statement {
  stmt_type tp;
  union {
    stmtlist block;
    expression *expr;
    struct {
      expression *left, *right;
    } assign;
    struct {
      caselist cases;
      statement *else_case;
    } if_stmt;
    struct {
      expression *cond;
      statement *body;
    } while_stmt;
    decllist vardecl;
    struct {
      char *name;
      str_list params;
      statement *body;
    } funcdef;
    struct {
      char *name;
      exprlist parents;
      decllist decls;
    } typestmt;
    expression *return_stmt;
  };
} statement;

typedef struct expression {
  expr_type tp;
  union {
    intval int_expr;
    floatval float_expr;
    char *str_expr;
    char *id_expr;
    struct {
      operator_t op;
      expression *left, *right;
    } binary_expr;
    struct {
      operator_t op;
      expression *expr;
    } unary_expr;
    struct {
      expression *cond;
      expression *t, *f;
    } ternary_expr;
    exprlist list_expr;
    struct funcdecl_t {
      str_list params;
      statement *body;
    } func_expr;
    struct funcdecl_t method_expr;
    struct typedecl_t {
      exprlist parents;
      decllist decls;
    } type_expr;
    decllist object_expr;
    struct {
      expression *type;
      decllist decls;
    } initobj_expr;
    struct {
      expression *func;
      exprlist args;
    } call_expr;
    struct {
      expression *base;
      char *attr;
    } attr_expr;
    struct {
      expression *base;
      expression *index;
    } index_expr;
  };
} expression;

void stmt_free(statement *stmt);
void expr_free(expression *expr);

#endif // AST_H
