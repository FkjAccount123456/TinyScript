#ifndef LEX_H
#define LEX_H

#include "ast.h"
#include "seq.h"

typedef enum kw_token {
  X_IF,
  X_ELSE,
  X_WHILE,
  X_VAR,
  X_FUNC,
  X_METHOD,
  X_TYPE,
  X_RETURN,
  X_BREAK,
  X_CONTINUE,
  X_NIL,
} kw_token;

typedef enum sym_token {
  X_LPAREN,
  X_RPAREN,
  X_LSQBR,
  X_RSQBR,
  X_LBRACE,
  X_RBRACE,
  X_COMMA,
  X_COLON,
  X_SEMI,
  X_DOT,
  X_ASSIGN,
} sym_token;

typedef enum token_type {
  K_EOF,

  K_INT,
  K_FLOAT,
  K_STR,
  K_ID,

  K_KW,
  K_OP,
  K_SYM,
} token_type;

typedef struct token {
  token_type tp;
  union {
    intval i;
    double f;
    char *s;
    kw_token kw;
    sym_token sym;
    operator_t op;
  };
} token;

typedef struct tokenlist {
  token *v;
  size_t len, max, Tsize;
  str_list strs;
} tokenlist;

tokenlist tokenize(char *code);

void token_print(token t);

#endif // LEX_H
