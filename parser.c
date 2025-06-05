#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "parser.h"
#include "array.h"
#include "tokenizer.h"

typedef struct Parser {
    bool error;
    char *err_msg;
    int line;
} Parser;

typedef enum OpFamily {
    GROUPING,
    UNARY,
    BINARY,
    COMPARISON,
    LOGICAL,
} OpFamily;

typedef struct Op {
    OpFamily family;
    int prec;
    TokType tok_type;
} Op;

/* There are 6 different precedence levels. 
Don't confuse them with OpFamily! 
Operators of the same family, might have a different precedence; e.g. '+' and '*'. */
#define MAX_PREC 6 

Op OpTable[] = 
{
    {GROUPING,   MAX_PREC,     TOK_OPAREN}, // (
    {GROUPING,   MAX_PREC,     TOK_CPAREN}, // )
    {UNARY,      MAX_PREC - 1, TOK_NOT   }, // !
    {BINARY,     MAX_PREC - 2, TOK_STAR  }, // *
    {BINARY,     MAX_PREC - 2, TOK_SLASH }, // /
    {BINARY,     MAX_PREC - 3, TOK_PLUS  }, // +
    {BINARY,     MAX_PREC - 3, TOK_MINUS }, // -
    {COMPARISON, MAX_PREC - 4, TOK_LT    }, // <
    {COMPARISON, MAX_PREC - 4, TOK_GT    }, // >
    {COMPARISON, MAX_PREC - 4, TOK_LE    }, // <=
    {COMPARISON, MAX_PREC - 4, TOK_GE    }, // >=
    {COMPARISON, MAX_PREC - 4, TOK_EQ    }, // ==
    {COMPARISON, MAX_PREC - 4, TOK_NE    }, // !=
    {LOGICAL,    MAX_PREC - 5, TOK_AND   }, // &&
    {LOGICAL,    MAX_PREC - 5, TOK_OR    }, // ||
    {0,          0,            0         }  //  NULL (Terminator)
};

DECLARE_ARR(OpStack, Op)
DECLARE_ARR(NumStack, int)

static int parse_expression(TokenArr token_arr);
// get_tok_literal() might be part of tokenizer.h
static void get_tok_literal(char *tok_literal, Token token);
static Op OpTable_get_op(TokType tok_type);

static void perform_unary_op(NumStack *numbers, TokType tok_type);
static void perform_binary_op(NumStack *numbers, TokType tok_type);
static void perform_comparison_op(NumStack *numbers, TokType tok_type);
static void perform_logical_op(NumStack *numbers, TokType tok_type);

// ARR_TOP is not usable because data of OpStack is a struct
static Op OpStack_top(OpStack operators) {
    return operators.size > 0 ? operators.data[operators.size - 1] : (Op){0};
}

Parser parser = {
    .error = false,
    .err_msg = NULL,
    .line = 0,
};

void parse_tokens(TokenArr token_arr)
{
    int expr_res = parse_expression(token_arr);
    printf("expr_res: %d\n", expr_res);
}

int parse_expression(TokenArr token_arr)
{
    OpStack operators;
    ARR_INIT(&operators);
    NumStack numbers;
    ARR_INIT(&numbers);

    int expr_res = 0;
    int prec_lvl = 0;

    for (int i = 0; i < token_arr.size; i++)
    {
        Token token = token_arr.data[i];

        // PARSER
        parser.line = token.line;
        
        char tok_literal[token.len + 1];
        get_tok_literal(tok_literal, token);

        if (token.type == TOK_NUMBER) {
            ARR_PUSH(&numbers, atoi(tok_literal), int);
            continue;
        }

        Op new_op = OpTable_get_op(token.type);
        if (new_op.prec == 0) { // The NULL Op
            printf("Line %d: expected an operator, but got '%s' instead.\n", parser.line, tok_literal);
            parser.error = true;
            goto error;
        }

        // TODO this if check and the next one seem hard coded to me. I'll go ahead in the development
        // of the parser and see if a good solution is going to take shape on its own.
        if (new_op.family == UNARY && i < token_arr.size-1 && token_arr.data[i+1].type != TOK_NUMBER) {
            get_tok_literal(tok_literal, token_arr.data[i+1]);
            printf("Line %d: expected a number after unary operator, but got '%s' instead.\n", parser.line, tok_literal);
            parser.error = true;
            goto error;
        }
        
        if (new_op.family == UNARY && i == token_arr.size-1) {
            printf("Line %d: expected a number after unary operator, but reached EOF instead.\n", parser.line);
            parser.error = true;
            goto error;
        }

        if (new_op.tok_type == TOK_OPAREN) {
            prec_lvl++;
            continue;
        }

        if (new_op.tok_type == TOK_CPAREN) {
            prec_lvl--;
            continue;
        }
        
        new_op.prec += MAX_PREC * prec_lvl;

        Op top_op = OpStack_top(operators);

        // Explanation of the Stack-based precedence parsing [From 7:40]: https://youtu.be/c2nR3Ua4CFI?si=G-YMsmXbP65WApAh&t=460
        while (!ARR_IS_EMPTY(&operators) && top_op.prec >= new_op.prec)
        {
            switch (top_op.family)
            {
            case UNARY:
                perform_unary_op(&numbers, top_op.tok_type);
                break;
            case BINARY:
                perform_binary_op(&numbers, top_op.tok_type);
                break;
            case COMPARISON:
                perform_comparison_op(&numbers, top_op.tok_type);
                break;
            case LOGICAL:
                perform_logical_op(&numbers, top_op.tok_type);
                break;
            default:
                assert("Unreachable" && false);
                break;
            }

            if (parser.error) goto error;

            ARR_POP(&operators);
            top_op = OpStack_top(operators);
        }

        ARR_PUSH(&operators, new_op, Op);
    } // for()

    // Missing trailing ')' are not a problem for the parsing. So, I just warn the user.
    if (prec_lvl != 0) {
        printf("Line %d: warning: missing %d closing parenthesis.\n", token_arr.data[token_arr.size-1].line, prec_lvl);
    }

    // Perform remaining operations in order of apparence
    while (!ARR_IS_EMPTY(&operators))
    {
        Op op = OpStack_top(operators);
        switch (op.family)
        {
        case UNARY:
            perform_unary_op(&numbers, op.tok_type);
            break;
        case BINARY:
            perform_binary_op(&numbers, op.tok_type);
            break;
        case COMPARISON:
            perform_comparison_op(&numbers, op.tok_type);
            break;
        case LOGICAL:
            perform_logical_op(&numbers, op.tok_type);
            break;
        default:
            assert("Unreachable" && false);
            break;
        }

        if (parser.error) goto error;

        ARR_POP(&operators);
    }

    expr_res = ARR_TOP(&numbers);
    ARR_POP(&numbers);

    // Assert that everything was consumed
    assert(operators.size == 0);
    assert(numbers.size == 0);
    
error:
    ARR_FREE(&operators);
    ARR_FREE(&numbers);

    return expr_res;
}

static void get_tok_literal(char *tok_literal, Token token)
{
    memcpy(tok_literal, token.start, token.len + 1);
    tok_literal[token.len] = '\0';
}

static Op OpTable_get_op(TokType tok_type)
{
    for (size_t i = 0; OpTable[i].prec != 0; i++) {
        if (OpTable[i].tok_type == tok_type) {
            return OpTable[i];
        }
    }

    return (Op){0, 0, 0};
}

static void perform_unary_op(NumStack *numbers, TokType tok_type)
{
    if (ARR_IS_EMPTY(numbers)) {
        printf("Line %d: while parsing expression: expected number to perform unary operation.\n", parser.line);
        parser.error = true;
        return;
    }

    int num = ARR_TOP(numbers);
    ARR_POP(numbers);

    int res = 0;
    if (tok_type == TOK_NOT) {
        if (num == 0) res = 1;
    } else {
        assert("Unreachable" && false);
    }

    ARR_PUSH(numbers, res, int);
}

static void perform_binary_op(NumStack *numbers, TokType tok_type)
{
    if (ARR_IS_EMPTY(numbers)) {
        printf("Line %d: while parsing expression: expected right-hand side number to perform binary operation.\n", parser.line);
        parser.error = true;
        return;
    }
    int r_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    if (ARR_IS_EMPTY(numbers)) {
        printf("Line %d: while parsing expression: expected left-hand side number to perform binary operation.\n", parser.line);
        parser.error = true;
        return;
    }
    int l_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    int res;
    switch (tok_type)
    {
    case TOK_PLUS: 
        res = l_num + r_num; 
        break;
    case TOK_MINUS: 
        res = l_num - r_num; 
        break;
    case TOK_STAR: 
        res = l_num * r_num; 
        break;
    case TOK_SLASH: 
        res = l_num / r_num; 
        break; 
    default:
        assert("Unreachable" && false);
        break;
    }   

    ARR_PUSH(numbers, res, int);
}

static void perform_comparison_op(NumStack *numbers, TokType tok_type)
{
    if (ARR_IS_EMPTY(numbers)) {
        printf("Line %d: while parsing expression: expected right-hand side number to perform comparison operation.\n", parser.line);
        parser.error = true;
        return;
    }
    int r_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    if (ARR_IS_EMPTY(numbers)) {
        printf("Line %d: while parsing expression: expected left-hand side number to perform comparison operation.\n", parser.line);
        parser.error = true;
        return;
    }
    int l_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    int res = 0;
    switch (tok_type)
    {
    case TOK_LT:
        res = l_num < r_num;
        break;
    case TOK_GT: 
        res = l_num > r_num;
        break;
    case TOK_LE:
        res = l_num <= r_num;
        break;
    case TOK_GE:
        res = l_num >= r_num;
        break;
    case TOK_EQ:
        res = l_num == r_num;
        break;
    case TOK_NE:
        res = l_num != r_num;
        break;
    default:
        assert("Unreachable" && false);
        break;
    }

    ARR_PUSH(numbers, res, int);
}

static void perform_logical_op(NumStack *numbers, TokType tok_type)
{
    if (ARR_IS_EMPTY(numbers)) {
        printf("Line %d: while parsing expression: expected right-hand side number to perform logical operation.\n", parser.line);
        parser.error = true;
        return;
    }
    int r_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    if (ARR_IS_EMPTY(numbers)) {
        printf("Line %d: while parsing expression: expected left-hand side number to perform logical operation.\n", parser.line);
        parser.error = true;
        return;
    }
    int l_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    int res = 0;
    if (tok_type == TOK_AND) res = l_num && r_num;
    else if (tok_type == TOK_OR) res = l_num || r_num;
    else assert("Unreachable" && false);

    ARR_PUSH(numbers, res, int);
}
