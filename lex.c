#include "lex.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

tokenlist tokenize(char *code) {
  tokenlist l = seq_init(tokenlist);
  l.strs = seq_init(str_list);
  size_t pos = 0, len = strlen(code);

  while (pos < len) {
    while (pos < len && (code[pos] == ' ' || code[pos] == '\t' ||
                         code[pos] == '\n' || code[pos] == '\r' ||
                         pos + 1 < len && code[pos] == '/' &&
                             (code[pos + 1] == '/' || code[pos + 1] == '*'))) {
      if (code[pos] == ' ' || code[pos] == '\t' || code[pos] == '\n' ||
          code[pos] == '\r') {
        pos++;
      } else if (code[pos] == '/' && code[pos + 1] == '/') {
        while (pos < len && code[pos] != '\n')
          pos++;
        pos++;
      } else {
        pos += 2;
        while (pos < len &&
               !(code[pos] == '*' && pos + 1 < len && code[pos + 1] == '/'))
          pos++;
        pos += 2;
      }
    }

    if (pos >= len) {
      break;
    } else if (isdigit(code[pos])) {
      raw_str num = seq_init(raw_str);
      bool is_float = false;
      while (pos < len && (isdigit(code[pos]) || code[pos] == '.')) {
        if (code[pos] == '.')
          is_float = true;
        seq_append(num, code[pos++]);
      }
      seq_append(num, 0);
      token t;
      if (is_float)
        t.tp = K_FLOAT, t.f = atof(num.v);
      else
        t.tp = K_INT, t.i = atol(num.v);
      seq_append(l, t);
      free(num.v);
    } else if (isalpha(code[pos]) || code[pos] == '_') {
      raw_str id = seq_init(raw_str);
      while (pos < len && (isalnum(code[pos]) || code[pos] == '_')) {
        seq_append(id, code[pos++]);
      }
      seq_append(id, 0);
      token t = {.tp = K_ID, .s = id.v};
      if (strcmp(id.v, "if") == 0)
        t.tp = K_KW, t.kw = X_IF;
      else if (strcmp(id.v, "else") == 0)
        t.tp = K_KW, t.kw = X_ELSE;
      else if (strcmp(id.v, "while") == 0)
        t.tp = K_KW, t.kw = X_WHILE;
      else if (strcmp(id.v, "var") == 0)
        t.tp = K_KW, t.kw = X_VAR;
      else if (strcmp(id.v, "func") == 0)
        t.tp = K_KW, t.kw = X_FUNC;
      else if (strcmp(id.v, "method") == 0)
        t.tp = K_KW, t.kw = X_METHOD;
      else if (strcmp(id.v, "type") == 0)
        t.tp = K_KW, t.kw = X_TYPE;
      else if (strcmp(id.v, "return") == 0)
        t.tp = K_KW, t.kw = X_RETURN;
      else if (strcmp(id.v, "break") == 0)
        t.tp = K_KW, t.kw = X_BREAK;
      else if (strcmp(id.v, "continue") == 0)
        t.tp = K_KW, t.kw = X_CONTINUE;
      else if (strcmp(id.v, "nil") == 0)
        t.tp = K_KW, t.kw = X_NIL;
      if (t.tp == K_KW)
        free(id.v);
      seq_append(l, t);
    } else if (code[pos] == '"' || code[pos] == '\'') {
      char x = code[pos++];
      raw_str s = seq_init(raw_str);
      while (pos < len && code[pos] != x) {
        if (code[pos] == '\\') {
          if (pos >= len) {
            printf("Unexpect EOF in string literal\n");
            exit(1);
          }
          pos++;
          switch (code[pos]) {
          case 'r':
            seq_append(s, '\r');
            break;
          case 't':
            seq_append(s, '\t');
            break;
          case 'a':
            seq_append(s, '\a');
            break;
          case 'f':
            seq_append(s, '\f');
            break;
          case 'v':
            seq_append(s, '\v');
            break;
          case 'b':
            seq_append(s, '\b');
            break;
          case 'n':
            seq_append(s, '\n');
            break;
          case '\\':
            seq_append(s, '\\');
            break;
          case '\'':
            seq_append(s, '\'');
            break;
          case '\"':
            seq_append(s, '\"');
            break;
          case 'x': {
            char ch = 0;
            for (char i = 0; i < 2; i++) {
              pos++;
              if (pos >= len) {
                printf("Unexpect EOF in string literal\n");
                exit(1);
              }
              ch <<= 4;
              if ('0' <= code[pos] && code[pos] <= '9')
                ch |= code[pos] - '0';
              else if ('a' <= code[pos] && code[pos] <= 'f')
                ch |= code[pos] - 'a' + 10;
              else if ('A' <= code[pos] && code[pos] <= 'F')
                ch |= code[pos] - 'A' + 10;
              else {
                printf("Invalid hex character '%c'\n", code[pos]);
                exit(1);
              }
            }
            break;
          }
          default:
            printf("Unknown escape character '%c'\n", code[pos]);
            exit(1);
          }
          pos++;
        } else {
          seq_append(s, code[pos++]);
        }
      }
      seq_append(s, 0);
      if (pos >= len) {
        printf("Unexpect EOF in string literal\n");
        exit(1);
      }
      pos++;
      token t = {.tp = K_STR, .s = s.v};
      seq_append(l, t);
      seq_append(l.strs, s.v);
    } else {
      token t;
      switch (code[pos]) {
      case '+':
        t.tp = K_OP, t.op = O_ADD;
        break;
      case '-':
        t.tp = K_OP, t.op = O_SUB;
        break;
      case '*':
        t.tp = K_OP, t.op = O_MUL;
        break;
      case '/':
        t.tp = K_OP, t.op = O_DIV;
        break;
      case '%':
        t.tp = K_OP, t.op = O_MOD;
        break;
      case '=':
        if (pos + 1 < len && code[pos + 1] == '=') {
          t.tp = K_OP, t.op = O_EQ;
          pos++;
        } else {
          t.tp = K_SYM, t.sym = X_ASSIGN;
        }
        break;
      case '!':
        if (pos + 1 < len && code[pos + 1] == '=') {
          t.tp = K_OP, t.op = O_NE;
          pos++;
        } else {
          t.tp = K_OP, t.op = O_NOT;
        }
        break;
      case '>':
        if (pos + 1 < len && code[pos + 1] == '=') {
          t.tp = K_OP, t.op = O_GE;
          pos++;
        } else if (pos + 1 < len && code[pos + 1] == '>') {
          t.tp = K_OP, t.op = O_RSH;
          pos++;
        } else {
          t.tp = K_OP, t.op = O_GT;
        }
        break;
      case '<':
        if (pos + 1 < len && code[pos + 1] == '=') {
          t.tp = K_OP, t.op = O_LE;
          pos++;
        } else if (pos + 1 < len && code[pos + 1] == '<') {
          t.tp = K_OP, t.op = O_LSH;
          pos++;
        } else {
          t.tp = K_OP, t.op = O_LT;
        }
        break;
      case '&':
        if (pos + 1 < len && code[pos + 1] == '&') {
          t.tp = K_OP, t.op = O_AND;
          pos++;
        } else {
          t.tp = K_OP, t.op = O_BAND;
        }
        break;
      case '|':
        if (pos + 1 < len && code[pos + 1] == '|') {
          t.tp = K_OP, t.op = O_OR;
          pos++;
        } else {
          t.tp = K_OP, t.op = O_BOR;
        }
        break;
      case '^':
        t.tp = K_OP, t.op = O_XOR;
        break;
      case '~':
        t.tp = K_OP, t.op = O_INV;
        break;
      case '(':
        t.tp = K_SYM, t.sym = X_LPAREN;
        break;
      case ')':
        t.tp = K_SYM, t.sym = X_RPAREN;
        break;
      case '[':
        t.tp = K_SYM, t.sym = X_LSQBR;
        break;
      case ']':
        t.tp = K_SYM, t.sym = X_RSQBR;
        break;
      case '{':
        t.tp = K_SYM, t.sym = X_LBRACE;
        break;
      case '}':
        t.tp = K_SYM, t.sym = X_RBRACE;
        break;
      case ':':
        t.tp = K_SYM, t.sym = X_COLON;
        break;
      case ',':
        t.tp = K_SYM, t.sym = X_COMMA;
        break;
      case ';':
        t.tp = K_SYM, t.sym = X_SEMI;
        break;
      case '.':
        t.tp = K_SYM, t.sym = X_DOT;
        break;
      default:
        printf("Unknown character '%c'\n", code[pos]);
        exit(1);
      }
      pos++;
      seq_append(l, t);
    }
  }

  seq_append(l, (token){.tp = K_EOF});
  return l;
}

void token_print(token t) {
  switch (t.tp) {
  case K_EOF:
    printf("(EOF)");
    break;
  case K_INT:
    printf("(INT %lld)", t.i);
    break;
  case K_FLOAT:
    printf("(FLOAT %f)", t.f);
    break;
  case K_STR:
    printf("(STR %s)", t.s);
    break;
  case K_ID:
    printf("(ID %s)", t.s);
    break;
  case K_KW:
    printf("(KW %d)", t.kw);
    break;
  case K_OP:
    printf("(OP %d)", t.op);
    break;
  case K_SYM:
    printf("(SYM %d)", t.sym);
    break;
  default:
    printf("(Unknown)");
    break;
  }
}
