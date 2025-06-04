#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "parser.h"
#include "array.h"
#include "tokenizer.h"

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
    int name_len;
    char *p_name;
} Op;

#define MAX_PREC 7

Op OpTable[] = 
{
    {GROUPING,   MAX_PREC,     TOK_OPAREN, 1, "("},
    {GROUPING,   MAX_PREC,     TOK_CPAREN, 1, ")"},
    {UNARY,      MAX_PREC - 1, TOK_NOT,    1, "!"},
    {BINARY,     MAX_PREC - 2, TOK_STAR,   1, "*"},
    {BINARY,     MAX_PREC - 2, TOK_SLASH,  1, "/"},
    {BINARY,     MAX_PREC - 3, TOK_PLUS,   1, "+"},
    {BINARY,     MAX_PREC - 3, TOK_MINUS,  1, "-"},
    {COMPARISON, MAX_PREC - 4, TOK_LT,     1, "<"},
    {COMPARISON, MAX_PREC - 4, TOK_GT,     1, ">"},
    {COMPARISON, MAX_PREC - 4, TOK_LE,     2, "<="},
    {COMPARISON, MAX_PREC - 4, TOK_GE,     2, ">="},
    {COMPARISON, MAX_PREC - 4, TOK_EQ,     2, "=="},
    {COMPARISON, MAX_PREC - 4, TOK_NE,     2, "!="},
    {LOGICAL,    MAX_PREC - 5, TOK_AND,    2, "&&"},
    {LOGICAL,    MAX_PREC - 5, TOK_OR,     2, "||"},
    {0,          0,            TOK_NULL,   0, NULL} // Terminator
};

DECLARE_ARR(OpStack, Op) // TokType
DECLARE_ARR(NumStack, int)

Op OpTable_get_op(TokType tok_type);
void get_tok_literal(char *tok_literal, Token token);

void perform_unary_op(NumStack *numbers, TokType tok_type);
void perform_binary_op(NumStack *numbers, TokType tok_type);
void perform_comparison_op(NumStack *numbers, TokType tok_type);
void perform_logical_op(NumStack *numbers, TokType tok_type);

static char *op_to_literal(TokType tt);

// ARR_TOP is not usable because data of OpStack is a struct
Op OpStack_top(OpStack operators) {
    return operators.size > 0 ? operators.data[operators.size - 1] : (Op){0};
}

// TODO(04/06) see where to move this piece
/* The consequence of this approach is that 
the expression is true if the result is different than 0, otherwise is false. Therefore, 
it isn't a logical true/false. */

void parse_tokens(TokenArr ta)
{
    OpStack operators;
    ARR_INIT(&operators);
    NumStack numbers;
    ARR_INIT(&numbers);

    int prec_lvl = 0;

    for (int i = 0; i < ta.size; i++)
    {
        Token token = ta.data[i];
        
        char tok_literal[token.len + 1];
        get_tok_literal(tok_literal, token);

        if (token.type == TOK_NUMBER) {
            ARR_PUSH(&numbers, atoi(tok_literal), int);
            continue;
        }

        Op new_op = OpTable_get_op(token.type);
        if (new_op.p_name == NULL) {
            printf("Line %d: expected an operator; got '%s' instead.\n", token.line, tok_literal);
            continue;
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

            ARR_POP(&operators);
            top_op = OpStack_top(operators); // OpTable_get_op(ARR_TOP(&operators))
        }

        ARR_PUSH(&operators, new_op, Op);
    } // for()

    // Missing trailing ')' are not a problem for the parsing. So, I just warn the user.
    if (prec_lvl != 0) {
        printf("Line %d: warning: missing %d closing parenthesis.\n", ta.data[ta.size-1].line, prec_lvl);
    }

    // Perform remaining operations now ordered
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

        ARR_POP(&operators);
    }

    int res = ARR_TOP(&numbers);

    ARR_FREE(&operators);
    ARR_FREE(&numbers);

    printf("res: %d\n", res);

    // return res;
}

void get_tok_literal(char *tok_literal, Token token)
{
    memcpy(tok_literal, token.start, token.len + 1);
    tok_literal[token.len] = '\0';
}

Op OpTable_get_op(TokType tok_type)
{
    for (size_t i = 0; OpTable[i].p_name != NULL; i++) {
        if (OpTable[i].tok_type == tok_type) {
            return OpTable[i];
        }
    }

    return (Op){0, 0, 0, 0, NULL};
}

void perform_unary_op(NumStack *numbers, TokType tok_type)
{
    if (ARR_IS_EMPTY(numbers)) printf("ERROR: numbers stack is empty.\n");
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

void perform_binary_op(NumStack *numbers, TokType tok_type)
{
    // TODO errors are not properly managed
    if (ARR_IS_EMPTY(numbers)) printf("ERROR: numbers stack is empty.\n");
    int r_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    if (ARR_IS_EMPTY(numbers)) printf("ERROR: numbers stack is empty.\n");
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
        // TODO manage floating point numbers
        res = l_num / r_num; 
        break; 
    default:
        assert("Unreachable" && false);
        break;
    }   

    ARR_PUSH(numbers, res, int);
}

void perform_comparison_op(NumStack *numbers, TokType tok_type)
{
    if (ARR_IS_EMPTY(numbers)) printf("ERROR: numbers stack is empty.\n");
    int r_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    if (ARR_IS_EMPTY(numbers)) printf("ERROR: numbers stack is empty.\n");
    int l_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    // l_num and r_num stand for number on the left and on the right of the comparison operator.

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

void perform_logical_op(NumStack *numbers, TokType tok_type)
{
    if (ARR_IS_EMPTY(numbers)) printf("ERROR: numbers stack is empty.\n");
    int r_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    if (ARR_IS_EMPTY(numbers)) printf("ERROR: numbers stack is empty.\n");
    int l_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    int res = 0;
    if (tok_type == TOK_AND) res = l_num && r_num;
    else if (tok_type == TOK_OR) res = l_num || r_num;
    else assert("Unreachable" && false);

    ARR_PUSH(numbers, res, int);
}

static char *op_to_literal(TokType tt)
{
    switch (tt)
    {
    case TOK_OPAREN: return "(";
    case TOK_CPAREN: return ")";

    case   TOK_PLUS: return "+";
    case  TOK_MINUS: return "-";
    case   TOK_STAR: return "*";
    case  TOK_SLASH: return "/";

    case     TOK_LT: return "<";
    case     TOK_GT: return ">";
    case     TOK_LE: return "<=";
    case     TOK_GE: return ">=";

    case TOK_ASSIGN: return "=";
    case     TOK_EQ: return "==";
    case     TOK_NE: return "!=";

    case    TOK_NOT: return "!";
    case    TOK_AND: return "&&";
    case     TOK_OR: return "||";
    
    default:
        assert("Unreachable" && false);
        break;
    }

    return NULL;
}
