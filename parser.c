#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "parser.h"
#include "array.h"

typedef enum OPCODE {
    OPOPAREN, OPCPAREN,
    OPNOT, OPNEG,
    OPPOW,
    OPMUL, OPDIV, OPMOD,
    OPSUM, OPDIFF, 
    OPLT, OPGT
} OPCODE;

typedef enum OPTYPE {
    GROUPING,
    UNARY,
    BINARY,
    COMPARISON,
} OPTYPE;

typedef struct Op {
    int type;
    int opcode;
    int prec;
    int oplen;
    char *opname;
} Op;

Op OpTable[] = {
    {GROUPING,   OPOPAREN, 7, 1, "("},
    {GROUPING,   OPCPAREN, 7, 1, ")"},
    {UNARY,      OPNOT,    6, 1, "!"},
    {UNARY,      OPNEG,    6, 1, "-"},
    {BINARY,     OPMUL,    4, 1, "*"},
    {BINARY,     OPDIV,    4, 1, "/"},
    {BINARY,     OPSUM,    3, 1, "+"},
    {BINARY,     OPDIFF,   3, 1, "-"},
    {COMPARISON, OPLT,     2, 1, "<"},
    {COMPARISON, OPGT,     2, 1, ">"},
    {0,          0,        0, 0, NULL} // Terminator
};

DECLARE_ARR(OpStack, char *)
DECLARE_ARR(NumStack, int)

bool is_operator(char c);
Op OpTable_get_op(char *op);
void perform_binary_op(NumStack *numbers, char op);

void parse_tokens(TokenArr ta)
{
    // Explanation of the Stack-based expression parsing [From 7:40]: https://youtu.be/c2nR3Ua4CFI?si=G-YMsmXbP65WApAh&t=460
    OpStack operators;
    ARR_INIT(&operators);
    NumStack numbers;
    ARR_INIT(&numbers);

    for (int i = 0; i < ta.size; i++)
    {
        Token token = ta.data[i];
        
        // Get token literal
        char tok_literal[token.len];
        memcpy(tok_literal, &token.start, token.len);
        tok_literal[token.len] = '\0';

        if (token.type == TOK_NUMBER) {
            ARR_PUSH(&numbers, atoi(token.start), int);
        }
        else if (is_operator(token.start[0])) 
        {
            Op new_op = OpTable_get_op(tok_literal);
            Op top_op = OpTable_get_op(ARR_TOP(&operators));

            while (!ARR_IS_EMPTY(&operators) && top_op.prec >= new_op.prec)
            {
                switch (top_op.type)
                {
                case GROUPING:
                    break;
                case UNARY:
                    break;
                case BINARY:
                    perform_binary_op(&numbers, top_op.opname[0]);
                    break;
                case COMPARISON:
                    break;
                default:
                    assert("Unreachable" && false);
                    break;
                }

                ARR_POP(&operators);
                top_op = OpTable_get_op(ARR_TOP(&operators));
            }

            ARR_PUSH(&operators, new_op.opname, char *);
            
        } else {
            printf("undefined\n");
        }
    }

#ifdef PDEBUG
    printf("nums: ");
    for (int i = 0; i < numbers.size; i++) {
        printf("%d ", numbers.data[i]);
    }
    printf("\n");

    printf("ops: ");
    for (int i = 0; i < operators.size; i++) {
        printf("%c ", operators.data[i]);
    }
    printf("\n");
#endif

    ARR_FREE(&operators);
    ARR_FREE(&numbers);
}

bool is_operator(char c)
{
    char operators[] = {'(', ')', '!', '*', '/', '+', '-', '<', '>', '\0'};
    for (size_t i = 0; operators[i] != '\0'; i++) {
        if (c == operators[i]) return true;
    }
    return false;
}

Op OpTable_get_op(char *op)
{
    for (size_t i = 0; OpTable[i].opname != NULL; i++) {
        if (strcmp(OpTable[i].opname, op) == 0) {
            return OpTable[i];
        }
    }

    return (Op){0, 0, 0, 0, NULL};
}

void perform_binary_op(NumStack *numbers, char op)
{
    // TODO errors are not properly managed
    if (ARR_IS_EMPTY(numbers)) printf("ERROR: numbers stack is empty.\n");
    int num1 = ARR_TOP(numbers);
    ARR_POP(numbers);

    if (ARR_IS_EMPTY(numbers)) printf("ERROR: numbers stack is empty.\n");
    int num2 = ARR_TOP(numbers);
    ARR_POP(numbers);

    int res;
    switch (op)
    {
    case '+': 
        res = num2 + num1; 
        break;
    case '-': 
        res = num2 - num1; 
        break;
    case '*': 
        res = num2 * num1; 
        break;
    case '/': 
        // TODO manage floating point numbers
        res = num2 / num1; 
        break; 
    default:
        assert("Unreachable" && false);
    }   

    ARR_PUSH(numbers, res, int);
}
