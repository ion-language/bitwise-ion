// @lang: c99
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

void* xmalloc(size_t new_size)
{
     void* x = malloc(new_size);
     if (!x) {
	  perror("malloc:");
	  exit(1);
     }
     return x;
}

void* xrealloc(void* x, size_t new_size)
{
     x = realloc(x, new_size);
     if (!x) {
	  perror("realloc:");
	  exit(1);
     }
     return x;
}

// (stretchy buffers)
typedef struct BufHdr {
     size_t len;
     size_t cap;
     char buf[];
} BufHdr;

#define buf__hdr(b) ((BufHdr *)((char *)(b) - offsetof(BufHdr, buf)))
#define buf__fits(b, n) (buf_len(b) + (n) <= buf_cap(b))
#define buf__fit(b, n) (buf__fits((b), (n)) ? 0 : ((b) = buf__grow((b), buf_len(b) + (n), sizeof(*(b)))))

#define buf_len(b) ((b) ? buf__hdr(b)->len : 0)
#define buf_cap(b) ((b) ? buf__hdr(b)->cap : 0)
#define buf_push(b, ...) (buf__fit((b), 1), (b)[buf__hdr(b)->len++] = (__VA_ARGS__))
#define buf_free(b) ((b) ? (free(buf__hdr(b)), (b) = NULL) : 0)

void *buf__grow(const void *buf, size_t new_len, size_t elem_size) {
     size_t next_cap = 1 + 2*buf_cap(buf);
     size_t new_cap = next_cap<new_len? new_len:next_cap;
     assert(new_len <= new_cap);
     size_t new_size = offsetof(BufHdr, buf) + new_cap*elem_size;
     BufHdr *new_hdr;
     if (buf) {
	  new_hdr = xrealloc(buf__hdr(buf), new_size);
     } else {
	  new_hdr = xmalloc(new_size);
	  new_hdr->len = 0;
     }
     new_hdr->cap = new_cap;
     return new_hdr->buf;
}
// (stretchy buffers)

void buf_printf(char** d_output_buf, char const* fmt, ...)
{
     char* output_buf = *d_output_buf;
     va_list args;
     va_start(args, fmt);
     int num = 0;
     {
	  va_list a1;
	  va_copy(a1, args);
	  num = vsnprintf(NULL, 0, fmt, a1);
	  num = num<0?0:num;
     }
     buf__fit(output_buf, num+1);
     num = vsnprintf(&output_buf[buf_len(output_buf)], buf_cap(output_buf), fmt, args);
     if (num < 0) {
	  perror("snprintf");
	  exit(1);
     }
     buf__hdr(output_buf)->len += num;
     *d_output_buf = output_buf;
}

enum TokenKind
{
     TokenKind_None,
     // ... ascii range reserved for symbols that stand for themselves ...
     TokenKind_Identifier = 128,
     TokenKind_Integer,
     TokenKind_LeftShift,
     TokenKind_RightShift,
};

struct Token
{
     enum TokenKind kind;
     char* bytes_f;
     char* bytes_l;
     int64_t val;
};

int next_token(struct Token* d_token, char** d_input_z)
{
     char *input = *d_input_z;
     d_token->kind = TokenKind_None;
     d_token->bytes_f = input;

     do {
          switch (*input)
          {
          case '0': case '1': case '2': case '3':
          case '4': case '5': case '6': case '7':
          case '8': case '9': {
               d_token->kind = TokenKind_Integer;
               input++;
               while (*input >= '0' && *input <= '9') {
                    ++input;
               }
          } break;

          case '(': case ')':
          case '-': case '~': case '*': case '/':
          case '%': case '&': case '|': case '^':
          case '+': {
               d_token->kind = *input;
               ++input;
          } break;

          case '<': {
               ++input;
               if (*input == '<') {
                    d_token->kind = TokenKind_LeftShift;
                    ++input;
               }
          } break;

          case '>': {
               ++input;
               if (*input == '>') {
                    d_token->kind = TokenKind_RightShift;
                    ++input;
               }
          } break;

          case ' ': case '\t': {
               d_token->kind = ' ';
               ++input;
               while (*input == ' ' || *input == '\t') {
                    ++input;
               }
               d_token->bytes_f = input;
          } break;
          }
     } while (d_token->kind == ' ');
     d_token->bytes_l = input;
     *d_input_z = input;
     return d_token->kind != TokenKind_None;
}

struct ExpTreeNode
{
     struct Token token;
     size_t lhs_id;
     size_t rhs_id;
};

struct ExpTree
{
     struct ExpTreeNode *nodes_buf;
};

void tree_push(struct ExpTree* tree, struct ExpTreeNode node)
{
     if (!tree) return;
     if (buf_len(tree->nodes_buf) == 0) buf_push(tree->nodes_buf, (struct ExpTreeNode) {0,});
     size_t id = buf_len(tree->nodes_buf);
     buf_push(tree->nodes_buf, node);
     assert(node.lhs_id != id);
     assert(node.rhs_id != id);
}

size_t tree_next_id(struct ExpTree* tree)
{
     if (!tree) return 0;
     int len = buf_len(tree->nodes_buf);
     return len>0?len:1;
}

void tree_forget(struct ExpTree* tree, size_t nodes_l)
{
     if (!tree) return;
     if (tree->nodes_buf) buf__hdr(tree->nodes_buf)->len = nodes_l;
}

void tree_reset(struct ExpTree* tree)
{
     if (!tree) return;
     if (tree->nodes_buf) buf__hdr(tree->nodes_buf)->len = 0;
}

void node_cat_sexp(struct ExpTree* tree, struct ExpTreeNode* node, char** d_temp_buf)
{
     if (node->lhs_id == 0 && node->rhs_id == 0) {
          for (char *lit_i = node->token.bytes_f, *lit_l = node->token.bytes_l; lit_i < lit_l; lit_i++) {
               buf_push(*d_temp_buf, *lit_i);
          }
     }
     if (node->lhs_id != 0) {
          if (node->token.kind == TokenKind_None)
          {
               node_cat_sexp(tree, &tree->nodes_buf[node->lhs_id], d_temp_buf);
          }
          else
          {
               buf_push(*d_temp_buf, '(');
               for (char *lit_i = node->token.bytes_f, *lit_l = node->token.bytes_l; lit_i < lit_l; lit_i++) {
                    buf_push(*d_temp_buf, *lit_i);
               }
               buf_push(*d_temp_buf, ' ');
               node_cat_sexp(tree, &tree->nodes_buf[node->lhs_id], d_temp_buf);
               if (node->rhs_id != 0) {
                    buf_push(*d_temp_buf, ' ');
                    node_cat_sexp(tree, &tree->nodes_buf[node->rhs_id], d_temp_buf);
               }
               buf_push(*d_temp_buf, ')');
          }
     }
}

void tree_sexp(struct ExpTree* tree, char** d_temp_buf)
{
     char* temp_buf = *d_temp_buf;
     if (temp_buf) buf__hdr(temp_buf)->len = 0;

     if (buf_len(tree->nodes_buf) > 0) {
          struct ExpTreeNode* top =
               &tree->nodes_buf[buf_len(tree->nodes_buf) - 1];
          node_cat_sexp(tree, top, &temp_buf);
     }
     buf_push(temp_buf, '\0');
     *d_temp_buf = temp_buf;
}

// Integer
char* exp_parse_terminal(char* input_z, struct ExpTree* tree)
{
     struct Token token = {0,};
     char* t_f = input_z;
     char* t_l = t_f;
     if (next_token(&token, &input_z) && token.kind == TokenKind_Integer) {
          t_l = input_z;
     }

     if (t_f == t_l) return t_f;

     size_t terminal_id = tree_next_id(tree);
     tree_push(tree, (struct ExpTreeNode) { .token = token });
     tree_push(tree, (struct ExpTreeNode) { .lhs_id = terminal_id });

     return input_z;
}

char* exp_parse_a(char* input_z, struct ExpTree* tree);

// terminal|'(' a ')'
char* exp_parse_e(char* input_z, struct ExpTree* tree)
{
  char *e_f = input_z;
  char* e_l = exp_parse_terminal(input_z, tree);
  if (e_l != e_f) {
    return e_l;
  } else {
    char* paren_f = e_f;
    char* paren_l = paren_f;
    struct Token token = {0,};
    if (!(next_token(&token, &input_z) && token.kind == '(')) return e_l;
    e_l = input_z;
    e_l = exp_parse_a(input_z, tree);
    input_z = e_l;
    if (!(next_token(&token, &input_z) && token.kind == ')')) return e_l;
    e_l = input_z;
    return e_l;
  }
}

// {'~'|'-'}terminal
char *exp_parse_u(char* input_z, struct ExpTree* tree)
{
     char* const u_f = input_z;

     struct Token token = {0,};
     struct Token *ops_buf = NULL;

     char* terminal_f = u_f;
     while (next_token(&token, &input_z) &&
            (token.kind == '~' || token.kind == '-')) {
          buf_push(ops_buf, token);
          terminal_f = input_z;
     }

     char *terminal_l = exp_parse_e(terminal_f, tree);
     if (terminal_f == terminal_l)
     {
          buf_free(ops_buf);
          return u_f;
     }

     for (struct Token* ops_i = &ops_buf[buf_len(ops_buf)],
               *ops_l = &ops_buf[0]; ops_i > ops_l; ) {
          --ops_i;
          size_t lhs_id = tree_next_id(tree) - 1;
          tree_push(tree, (struct ExpTreeNode) { .token = *ops_i, .lhs_id = lhs_id });
     }
     buf_free(ops_buf);
     return terminal_l;
}

// u{('*'|'/'|'%'|'<<'|'>>'|'&')u}
char* exp_parse_f(char* input_z, struct ExpTree* tree)
{
     struct Token token = {0,};
     char* f_f = input_z;
     char* f_l = f_f;

     char* u_f = input_z;
     char* u_l = exp_parse_u(u_f, tree);
     if (u_f == u_l) goto done;
     f_l = u_l;

     size_t lhs_id = tree_next_id(tree) - 1;
     input_z = f_l;
     while (next_token(&token, &input_z) &&
            (token.kind == '*' ||
             token.kind == '/' ||
             token.kind == '%' ||
             token.kind == TokenKind_LeftShift ||
             token.kind == TokenKind_RightShift ||
             token.kind == '&'))
     {
          char* u1_f = input_z;
          char* u1_l = exp_parse_u(u1_f, tree);
          if (u1_f == u1_l) goto done;
          size_t rhs_id = tree_next_id(tree) - 1;

          f_l = u1_l;
          input_z = f_l;

          struct ExpTreeNode op = {
               .token = token,
               .lhs_id = lhs_id,
               .rhs_id = rhs_id,
          };
          tree_push(tree, op);
          lhs_id = tree_next_id(tree) - 1;
     }
 done:
     return f_l;
}

// f{(+|-|'|'|^)f}
char* exp_parse_a(char* input_z, struct ExpTree* tree)
{
     struct Token token = {0,};
     char* a_f = input_z;
     char* a_l = a_f;

     char* f_f = input_z;
     char* f_l = exp_parse_f(f_f, tree);
     if (f_f == f_l) goto done;
     a_l = f_l;

     size_t lhs_id = tree_next_id(tree) - 1;
     input_z = a_l;
     while (next_token(&token, &input_z) &&
            (token.kind == '+' ||
             token.kind == '-' ||
             token.kind == '|' ||
             token.kind == '^'))
     {
          char* f1_f = input_z;
          char* f1_l = exp_parse_f(f1_f, tree);
          if (f1_f == f1_l) goto done;
          size_t rhs_id = tree_next_id(tree) - 1;

          a_l = f1_l;
          input_z = a_l;

          struct ExpTreeNode op = {
               .token = token,
               .lhs_id = lhs_id,
               .rhs_id = rhs_id,
          };
          tree_push(tree, op);
          lhs_id = tree_next_id(tree) - 1;
     }
 done:
     return a_l;
}

char* exp_sexp_from(char* input_z, char** d_output_buf)
{
     char* temp_buf = NULL;
     struct ExpTree tree = {0,};

     char* end_l = exp_parse_a(input_z, &tree);
     tree_sexp(&tree, d_output_buf);
     tree_reset(&tree);
     return end_l;
}

void test_exp_parse_terminal()
{
     char* temp_buf = NULL;
     struct ExpTree tree = {0,};
     assert(*exp_parse_terminal("9", &tree) == 0),
          tree_sexp(&tree, &temp_buf),
          assert(strcmp("9", temp_buf) == 0),
          tree_reset(&tree);

     assert(*exp_parse_terminal("12", &tree) == 0),
          tree_sexp(&tree, &temp_buf),
          assert(strcmp("12", temp_buf) == 0),
          tree_reset(&tree);
     assert(*exp_parse_terminal("+", &tree) != 0),
          tree_sexp(&tree, &temp_buf),
          assert(strcmp("", temp_buf) == 0),
          tree_reset(&tree);
     assert(*exp_parse_terminal("VAR", NULL) != 0), tree_reset(&tree);
}

void test_exp_parse_u()
{
     char* temp_buf = NULL;
     struct ExpTree tree = {0,};

     assert(*exp_parse_u("1", &tree) == 0),
          tree_sexp(&tree, &temp_buf), assert(strcmp("1", temp_buf) == 0),
          tree_reset(&tree);
     assert(*exp_parse_u("12", &tree) == 0),
          tree_sexp(&tree, &temp_buf), assert(strcmp("12", temp_buf) == 0),
          tree_reset(&tree);
     assert(*exp_parse_u("-12", &tree) == 0),
          tree_sexp(&tree, &temp_buf), assert(strcmp("(- 12)", temp_buf) == 0),
          tree_reset(&tree);
     assert(*exp_parse_u("~12", &tree) == 0),
          tree_sexp(&tree, &temp_buf),
          assert(strcmp("(~ 12)", temp_buf) == 0),
          tree_reset(&tree);
     assert(*exp_parse_u("-~12", &tree) == 0),
          tree_sexp(&tree, &temp_buf),
          assert(strcmp("(- (~ 12))", temp_buf) == 0),
          tree_reset(&tree);
     assert(*exp_parse_u("~-12", &tree) == 0),
          tree_sexp(&tree, &temp_buf),
          assert(strcmp("(~ (- 12))", temp_buf) == 0),
          tree_reset(&tree);
}

void test_exp_parse_f()
{
     char* temp_buf = NULL;
     struct ExpTree tree = {0,};

     assert(*exp_parse_f("1*2", &tree) == 0),
          tree_sexp(&tree, &temp_buf),
          assert(strcmp("(* 1 2)", temp_buf) == 0),
          tree_reset(&tree);
     assert(*exp_parse_f("1*2/3", &tree) == 0),
          tree_sexp(&tree, &temp_buf),
          assert(strcmp("(/ (* 1 2) 3)", temp_buf) == 0),
          tree_reset(&tree);
}

void test_exp_parse_a()
{
     char* temp_buf = NULL;
     struct ExpTree tree = {0,};

     assert (*exp_parse_a("1+2", &tree) == 0),
          tree_sexp(&tree, &temp_buf),
          assert(strcmp("(+ 1 2)", temp_buf) == 0),
          tree_reset(&tree);
     assert (*exp_parse_a("1-2", &tree) == 0),
          tree_sexp(&tree, &temp_buf),
          assert(strcmp("(- 1 2)", temp_buf) == 0),
          tree_reset(&tree);
     assert (*exp_parse_a("1|2", &tree) == 0),
          tree_sexp(&tree, &temp_buf),
          assert(strcmp("(| 1 2)", temp_buf) == 0),
          tree_reset(&tree);
     assert (*exp_parse_a("1^2", &tree) == 0),
          tree_sexp(&tree, &temp_buf),
          assert(strcmp("(^ 1 2)", temp_buf) == 0),
          tree_reset(&tree);
     assert (*exp_parse_a("1^2+3-9", &tree) == 0),
          tree_sexp(&tree, &temp_buf),
          assert(strcmp("(- (+ (^ 1 2) 3) 9)", temp_buf) == 0),
          tree_reset(&tree);
     assert (*exp_parse_a("1^(2+3)-9", &tree) == 0),
          tree_sexp(&tree, &temp_buf),
          assert(strcmp("(- (^ 1 (+ 2 3)) 9)", temp_buf) == 0),
          tree_reset(&tree);
}

void exp_test()
{
     test_exp_parse_terminal();
     test_exp_parse_u();
     test_exp_parse_f();
     test_exp_parse_a();
     char* test_str = "12*34 + 45/56 + ~25";
     char* o_buf = NULL;
     char* parse_l = exp_sexp_from(test_str, &o_buf);
     if (*parse_l) {
       printf("ERROR: Parsing, stopped at: '%s', got: '%.*s'\n", &parse_l[0], (int)buf_len(o_buf), &o_buf[0]);
	  return;
     }
     printf("'%s' Parsed as '%.*s'\n", test_str, (int)buf_len(o_buf), o_buf);
     assert(strcmp("(+ (+ (* 12 34) (/ 45 56)) (~ 25))", o_buf) == 0);
     printf("DONE\n");
}

int main()
{
     exp_test();
}
