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

Op OpTable[] = 
{
    {GROUPING,   7, TOK_OPAREN, 1, ")"},
    {GROUPING,   7, TOK_CPAREN, 1, "("},
    {UNARY,      6, TOK_NOT,    1, "!"},
    {BINARY,     4, TOK_STAR,   1, "*"},
    {BINARY,     4, TOK_SLASH,  1, "/"},
    {BINARY,     3, TOK_PLUS,   1, "+"},
    {BINARY,     3, TOK_MINUS,  1, "-"},
    {COMPARISON, 2, TOK_LT,     1, "<"},
    {COMPARISON, 2, TOK_GT,     1, ">"},
    {COMPARISON, 2, TOK_LE,     2, "<="},
    {COMPARISON, 2, TOK_GE,     2, ">="},
    {COMPARISON, 2, TOK_EQ,     2, "=="},
    {COMPARISON, 2, TOK_NE,     2, "!="},
    {LOGICAL,    1, TOK_AND,    2, "&&"},
    {LOGICAL,    1, TOK_OR,     2, "||"},
    {0,          0, TOK_NULL,   0, NULL} // Terminator
};

DECLARE_ARR(OpStack, TokType)
DECLARE_ARR(NumStack, int)

Op OpTable_get_op(TokType tok_type);
void get_tok_literal(char *tok_literal, Token token);

void perform_unary_op(NumStack *numbers, TokType tok_type);
void perform_binary_op(NumStack *numbers, TokType tok_type);
void perform_comparison_op(NumStack *numbers, TokType tok_type);
void perform_logical_op(NumStack *numbers, TokType tok_type);

static char *op_to_literal(TokType tt);

void parse_tokens(TokenArr ta)
{
    // Explanation of the Stack-based expression parsing [From 7:40]: https://youtu.be/c2nR3Ua4CFI?si=G-YMsmXbP65WApAh&t=460
    /* The consequence of this approach is that 
    the expression is true if the result is different than 0, otherwise is false. Therefore, 
    it isn't a logical true/false. */

    OpStack operators;
    ARR_INIT(&operators);
    NumStack numbers;
    ARR_INIT(&numbers);

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
            // printf("the new_op of type %d isn't a valid operator.\n", token.type);
            continue;
        }

        Op top_op = OpTable_get_op(ARR_TOP(&operators));

        while (!ARR_IS_EMPTY(&operators) && top_op.prec >= new_op.prec && top_op.tok_type != TOK_OPAREN)
        {
            switch (top_op.family)
            {
            case GROUPING:
                break;
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
            top_op = OpTable_get_op(ARR_TOP(&operators));
        }

        ARR_PUSH(&operators, new_op.tok_type, TokType);
    } // for()

#ifdef PDEBUG
    printf("nums: ");
    for (int i = 0; i < numbers.size; i++) {
        printf("%d ", numbers.data[i]);
    }
    printf("\n");

    printf("ops: ");
    for (int i = 0; i < operators.size; i++) {
        printf("%s ", op_to_literal(operators.data[i]));
    }
    printf("\n");
#endif

    ARR_FREE(&operators);
    ARR_FREE(&numbers);
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
