#ifndef PARSE_H
#define PARSE_H

#include "lex.h"

typedef struct parser {
  tokenlist tokens;
  size_t pos;
} parser;

parser parser_init(tokenlist tokens);
statement *parse(parser *p);

statement *parse_block(parser *p);
statement *parse_stmt(parser *p);
expression *parse_expr(parser *p);
expression *parse_primary(parser *p);

#endif // PARSE_H
