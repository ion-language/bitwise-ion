// @title: Day2 homework, expression calculator
#include "buf.h"

#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

int test_main(int argc, char* argv[]);

int64_t expr_calc_from_str(char const* str);

int main(int argc, char* argv[])
{
    test_main(argc, argv);
    char* expr_str = NULL;
    for(char** parg = &argv[1]; parg < &argv[argc]; ++parg) {
        buf_printf(expr_str, "%s", *parg);
    }
    printf("eval(%s) = %lld\n", expr_str, expr_calc_from_str(expr_str));
}


#include "buf.c"

typedef enum TokenKind
{
    TokenKind_None,
    TokenKind_AsciiMax = 127,
    TokenKind_Number = TokenKind_AsciiMax+1,
    TokenKind_Exponent,
    TokenKind_Mul,
    TokenKind_Div,
    TokenKind_Add,
    TokenKind_Sub,
} TokenKind;

typedef struct Token
{
    TokenKind kind;
    union {
        int64_t int_val;
    };
} Token;

Token scan_token(char const* zstr, char const** zstr_end)
{
    Token result = {0,};
    while (*zstr)
    {
        switch (*zstr) {
            case ' ': case '\t': case '\n': {
                zstr++;
                while (isspace(*zstr)) zstr++;
            } break;

            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7':
            case '8': case '9': {
                result.kind = TokenKind_Number;
                char* int_end;
                result.int_val = strtol(zstr, &int_end, 10);
                assert(int_end > zstr);
                zstr = int_end;
                goto got_token;
            } break;

            case '(': case ')': {
                result.kind = (TokenKind) *zstr;
                zstr++;
                goto got_token;
            } break;

            case '+': {
                result.kind = TokenKind_Add;
                zstr++;
                goto got_token;
            } break;

            case '-': {
                result.kind = TokenKind_Sub;
                zstr++;
                goto got_token;
            } break;

            case '*': {
                zstr++;
                size_t n = 0;
                while (*zstr == '*') {
                    zstr++; n++;
                }
                if (n == 0)
                {
                    result.kind = TokenKind_Mul;
                }
                else if (n == 1)
                {
                    result.kind = TokenKind_Exponent;
                }
                else
                {
                    goto got_unknown;
                }
                goto got_token;
            }

            case '/': {
                result.kind = TokenKind_Div;
                zstr++;
                goto got_token;
            }

            default: goto got_unknown;
        }
    }
    got_token:
    *zstr_end = zstr;
    return result;

    got_unknown:
    fprintf(stderr, "scan_token: got unknown token at: %s\n", zstr);
    *zstr_end = zstr;
    return result;
}

// Expr language

typedef struct ExprOperator
{
    char const* name;
    TokenKind token;
    int arity; // how many arguments, 1: unary, 2: binary
} ExprOperator;

typedef struct ExprOperatorPrecedence
{
    int arity;
    int associativity; // 0: left-associative, 1: right-associative
    int position; // -1: before, 0: infix, +1: after
    size_t operators_n; // >0
    ExprOperator const *operators;
} ExprOperatorPrecedence;

static const ExprOperator g_expr_operators_in_precedence_order[] = {
    { .name = "exp", .token = TokenKind_Exponent, .arity = 2 },
    { .name = "add", .token = TokenKind_Add, .arity = 2 },
    { .name = "sub", .token = TokenKind_Sub, .arity = 2 },
    { .name = "mul", .token = TokenKind_Mul, .arity = 2 },
    { .name = "div", .token = TokenKind_Div, .arity = 2 },
    { .name = "neg", .token = TokenKind_Sub, .arity = 1 },
    { .name = "id+", .token = TokenKind_Add, .arity = 1 },
};
static const size_t g_expr_operators_in_precedence_order_n = sizeof g_expr_operators_in_precedence_order / sizeof g_expr_operators_in_precedence_order[0];

static const ExprOperatorPrecedence g_expr_operator_precedences[] = {
    { .arity = 2, .position = 0, .associativity = 1, .operators_n = 1, .operators = &g_expr_operators_in_precedence_order[0] },
    { .arity = 2, .operators_n = 2, .operators = &g_expr_operators_in_precedence_order[1] },
    { .arity = 2, .operators_n = 2, .operators = &g_expr_operators_in_precedence_order[3] },
    { .arity = 1, .position = -1, .associativity = 0, .operators_n = 2, .operators = &g_expr_operators_in_precedence_order[5] },
};
static const size_t g_expr_operator_precedences_n = sizeof g_expr_operator_precedences / sizeof g_expr_operator_precedences[0];

typedef struct ExprStream
{
    char const* bytes;
    Token token;
} ExprStream;

void token_match(ExprStream *stream, TokenKind kind)
{
    TokenKind got_kind = stream->token.kind;
    if (got_kind != kind) fprintf(stderr, "Error: Expected Token %d got %d\n", kind, got_kind), exit(1);
}

void token_next(ExprStream *stream)
{
    stream->token = scan_token(stream->bytes, &stream->bytes);
}

bool token_expect(ExprStream *stream, TokenKind kind)
{
    TokenKind got_kind = stream->token.kind;
    if (got_kind != kind)
    {
        fprintf(stderr, "Error: Expected Token %d got %d\n", kind, got_kind);
        return false;
    }
    token_next(stream);
    return true;
}

char *calc_trace = NULL;

int64_t expr_int(ExprStream *stream)
{
    token_match(stream, TokenKind_Number);
    int64_t y = stream->token.int_val;
    token_next(stream);
    buf_printf(calc_trace, " %ld", y);
    return y;
}

int64_t expr_gen_op(ExprStream *stream, int precedence);

int64_t expr_e(ExprStream *stream)
{
    if (stream->token.kind == '(')
    {
        buf_printf(calc_trace, "(");
        char const* paren_start = stream->bytes;
        token_next(stream);
        int64_t y = expr_gen_op(stream, 0);
        if (!token_expect(stream, ')'))
        {
            fprintf(stderr, "Start: %s\n", paren_start);
        }
        buf_printf(calc_trace, ")");
        return y;
    }
    return expr_int(stream);
}

ExprOperator const* expr_op_find_operator_for_precedence(TokenKind op_kind, int precedence)
{
    ExprOperatorPrecedence level = g_expr_operator_precedences[precedence];
    for (ExprOperator const *op = level.operators, *op_l = &op[level.operators_n];
         op < op_l; ++op) {
        if (op->token == op_kind) return op;
    }
    return NULL;
}

int64_t expr_op_unary_prefix(ExprStream *stream, int precedence)
{
    int64_t multiplier = 1;
    ExprOperator const *uop;
    do {
        TokenKind op_kind = stream->token.kind;
        uop = expr_op_find_operator_for_precedence(op_kind, precedence);
        if (uop) {
            token_next(stream);
            switch(uop->token)
            {
                case TokenKind_Sub: multiplier *= -1; break;
                case TokenKind_Add: break;
                default: assert(0); break;
            }
        }
    } while (uop);

    int64_t y = expr_gen_op(stream, precedence + 1);

    if (multiplier == -1) {
        y *= multiplier;
        buf_printf(calc_trace, " -{1}");
    } else {
        assert(multiplier == 1);
    }
    return y;
}

int64_t expr_op_binary_infix_left_associative(ExprStream *stream, int precedence)
{
    int64_t y = expr_gen_op(stream, precedence + 1);
    ExprOperator const *op;
    do {
        TokenKind op_kind = stream->token.kind;
        op = expr_op_find_operator_for_precedence(op_kind, precedence);
        if (op) {
            token_next(stream);
            int64_t yr = expr_gen_op(stream, precedence+1);
            buf_printf(calc_trace, " %s{%d}", op->name, op->arity);
            switch (op->token) {
                case TokenKind_Mul: {
                    y *= yr;
                } break;
                case TokenKind_Div: {
                    y /= yr;
                } break;
                case TokenKind_Add: {
                    y += yr;
                } break;
                case TokenKind_Sub: {
                    y -= yr;
                } break;
                default: {
                    assert(0);
                } break;
            }
        }
    } while (op);
    return y;
}

int64_t expr_op_binary_infix_right_associative(ExprStream *stream, int precedence)
{
    int64_t y = expr_gen_op(stream, precedence + 1);

    TokenKind op_kind = stream->token.kind;
    ExprOperator const *op = expr_op_find_operator_for_precedence(op_kind, precedence);
    if (op) {
        token_next(stream);
        int64_t yr = expr_gen_op(stream, precedence); // right-fold recursion
        buf_printf(calc_trace, " %s{%d}", op->name, op->arity);
        switch (op->token) {
            case TokenKind_Exponent: {
                y = (int64_t)pow(y, yr);
            } break;
            default: {
                assert(0); // don't know any such
            }
        }
    }
    return y;
}

int64_t expr_gen_op(ExprStream *stream, int precedence)
{
    if (precedence == g_expr_operator_precedences_n) {
        return expr_e(stream);
    }

    ExprOperatorPrecedence level = g_expr_operator_precedences[precedence];
    if (level.position == -1) {
        assert(level.arity == 1);
        return expr_op_unary_prefix(stream, precedence);
    } else if (level.position == 0) {
        assert(level.arity == 2);
        if (level.associativity == 0) {
            return expr_op_binary_infix_left_associative(stream, precedence);
        } else {
            assert(level.associativity == 1);
            return expr_op_binary_infix_right_associative(stream, precedence);
        }
    } else {
        assert(0);
        return 0;
    }
}

int64_t expr_calc(ExprStream *stream)
{
    int64_t y = expr_gen_op(stream, 0);
    printf("Trace: %s\n", calc_trace), buf_free(calc_trace);
    return y;
}

int64_t expr_calc_from_str(char const* str)
{
    ExprStream stream = { .bytes = str };
    token_next(&stream);
    return expr_calc(&stream);
}

void test_expr_calc()
{
    typedef struct ExprCalcTest
    {
        char const* str;
        int64_t result;
    } ExprCalcTest;

    ExprCalcTest tests[] = {
        { "2**3", (int64_t)pow(2.0,3.0) },
        { "2**1+2", (int64_t)pow(2.0,3.0) },
        { "(2**1)+5", 2+5 },
        { "1+3+5**3", (int64_t)pow(1+3+5, 3) },
        { "5**3**2", (int64_t)pow(5, pow(3, 2)) }, // right associativity of exponentiation
        { "(5**3)**2", (int64_t)pow(5, 3*2) },
        { "5**3*2", (int64_t)pow(5, 3*2) },
        { "5**3+2", (int64_t)pow(5, 3+2) },
        { "5**6/2", (int64_t)pow(5, 6/2) },
        { "3", 3 },
        { "-6", -6 },
        { "+7", 7 },
        { "2*3", 6 },
        { "2*3 + 11/5", 2*3+11/5 },
        { "2*(3 + 11)/5", 2*(3+11)/5 },
        { "2-3", 2-3, },
        { "2-3+5", 2-3+5 },
        { "2-3*4", 2-3*4 },
        { "2-3*4-1*5", 2-3*4-1*5 },
        { "2*3-5*2", 2*3-5*2 },
        { "12*34 + 45/56 + -25", 12*34+45/56+-25 },
    };
    size_t tests_n = sizeof tests/sizeof tests[0];
    for (ExprCalcTest *test_i = &tests[0], *test_l = &tests[tests_n]; test_i < test_l; test_i++)
    {
        ExprStream stream = { .bytes = test_i->str };
        token_next(&stream);
        buf_printf(calc_trace, "Expression: '%s' = ", test_i->str);
        int64_t result = expr_calc(&stream);
        assert(*stream.bytes == 0);
        assert(result == test_i->result);
    }
}

void assert_expected_tokens(char* zstr, size_t tokens_n, Token tokens[])
{
    Token *ptoken = &tokens[0];
    while (tokens_n--)
    {
        Token token = scan_token(zstr, &zstr);
        assert(token.kind == ptoken->kind);
        if (ptoken->kind == TokenKind_Number)
        {
            assert(ptoken->int_val == token.int_val);
        }
        ptoken++;
    }
}

void test_tokens()
{
    char* expr_str = "1 23 456 789 +-** */()";
    Token expected_tokens[] = {
        { .kind = TokenKind_Number, .int_val = 1 },
        { .kind = TokenKind_Number, .int_val = 23 },
        { .kind = TokenKind_Number, .int_val = 456 },
        { .kind = TokenKind_Number, .int_val = 789 },
        { .kind = TokenKind_Add },
        { .kind = TokenKind_Sub },
        { .kind = TokenKind_Exponent },
        { .kind = TokenKind_Mul },
        { .kind = TokenKind_Div },
        { .kind = '(' },
        { .kind = ')' },
        { .kind = TokenKind_None },

    };
    size_t expected_tokens_n = sizeof expected_tokens/sizeof expected_tokens[0];
    assert_expected_tokens(expr_str, expected_tokens_n, expected_tokens);
}


// left-fold <=> f(f(x_0, x_1), x_2)
// right-fold <=> f(x_0, f(x_1, x_2))

int left_fold_sub_recursive(int array[], int array_n)
{
    assert(array_n > 0);
    if (array_n == 1) return array[0];
    return left_fold_sub_recursive(&array[0], array_n-1) - array[array_n-1];
}

int left_fold_sub_tail_recursive(int array[], int array_n, int accumulator)
{
    if (array_n == 0) return accumulator;
    accumulator = accumulator - array[0];
    return left_fold_sub_tail_recursive(&array[1], array_n-1, accumulator);
}

int left_fold_sub_iterative(int array[], int array_n)
{
    assert(array_n > 0);
    int y = array[0];
    array++;
    array_n--;
    while (array_n > 0)
    {
        y = y - array[0];
        array++;
        array_n--;
    }
    return y;
}

int right_fold_sub_recursive(int array[], int array_n)
{
    assert(array_n > 0);
    if (array_n == 1) return array[0];
    return array[0] - right_fold_sub_recursive(&array[1], array_n - 1);
}

int right_fold_sub_tail_recursive(int array[], int array_n, int accumulator)
{
    if (array_n == 0) return accumulator;
    accumulator = array[array_n - 1] - accumulator;
    return right_fold_sub_tail_recursive(&array[0], array_n - 1, accumulator);
}

int right_fold_sub_iterative(int array[], int array_n)
{
    // right-to-left iteration
    assert(array_n > 0);
    int y = array[array_n - 1];
    array_n--;
    while (array_n > 0)
    {
        y = array[array_n - 1] - y;
        array_n--;
    }
    return y;
}

int test_main(int argc, char* argv[])
{
    // Parsing left-associative operators means performing a left-fold:
    {
        int abc[] = { 3, 5, 7 };
        int result = left_fold_sub_recursive(&abc[0], 3);
        assert(result == ((3-5)-7));
        int lr = left_fold_sub_iterative(&abc[0], 3);
        assert(result == lr);
        int lrtc = left_fold_sub_tail_recursive(&abc[1], 2, abc[0]);
        assert(lrtc == result);

        // Compare to a right-fold which gives an "unnatural" result for '-'
        int result_r = right_fold_sub_recursive(&abc[0], 3);
        assert(result_r == 3-(5-7));
        // iterative and tail recursive versions of a right fold break causality for a stream, because
        // they expect to work right-to-left. Plain recursive versions consume stack space.
        int rftc = right_fold_sub_tail_recursive(&abc[0], 2, abc[2]);
        assert(rftc == result_r);
        int rf = right_fold_sub_iterative(&abc[0], 3);
        assert(result_r == rf);
    }

    test_buf(argc, argv);
    test_tokens();
    test_expr_calc();
    return 1;
}
