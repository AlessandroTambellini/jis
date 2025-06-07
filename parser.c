#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "parser.h"
#include "array.h"
#include "tokenizer.h"

#define GLOBAL_SCOPE 0

#define ERR_MSG_SIZE 256

typedef struct Parser {
    bool error;
    char err_msg[256];
    // int line; // the line is given by each individual token
    int cursor;
    int scope;
    Token token;
    TokenArr token_arr;
} Parser;

typedef struct Variable {
    char *name_addr;
    int name_len;
    int value;
} Variable;

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

DECLARE_ARR(VarStack, Variable)
DECLARE_ARR(OpStack, Op)
DECLARE_ARR(NumStack, int)

static void parse_block(void);
static void parse_if(void);
static void parse_while(void);
static void parse_variable(void);
static void parse_print(void);
static int parse_expression(void);

static void advance(void);
static void consume(TokType type, char *err_msg);
static void report_error(char *err_msg);

static bool at_end_symbol(TokType type);

static void get_tok_literal(char *tok_literal, Token token);
static Op OpTable_get_op(TokType tok_type);

static void perform_unary_op(NumStack *numbers, TokType tok_type);
static void perform_binary_op(NumStack *numbers, TokType tok_type);
static void perform_comparison_op(NumStack *numbers, TokType tok_type);
static void perform_logical_op(NumStack *numbers, TokType tok_type);
static Op OpStack_top(OpStack operators);

Parser parser;

VarStack variables;

void init_parser(TokenArr token_arr)
{
    parser.error = false;
    parser.cursor = -1;
    parser.scope = 0;
    parser.token = (Token){0};
    parser.token_arr = token_arr;
    advance();
}

static void advance(void) 
{
    parser.cursor++;
    if (parser.cursor < parser.token_arr.size) {
        parser.token = parser.token_arr.data[parser.cursor];
    } else {
        parser.token = (Token){0};
    }
}

static void consume(TokType type, char *err_msg) {
    if (parser.token.type == type) {
        advance();
    } else {
        report_error(err_msg);
    }
}

static void report_error(char *err_msg) {
    // I leave these two properties inside parser for now,
    // even though I don't know if they may have some utility.
    parser.error = true;
    strcpy(parser.err_msg, err_msg);
    
    printf("Line %d: %s.\n", parser.token.line, parser.err_msg);
    
    exit(EXIT_FAILURE);
}

static bool at_end_symbol(TokType type)
{
    // Not taking TOK_EOF for granted, caused me problems in the past.
    if (parser.token.type == type || parser.token.type == TOK_EOF) return true;
    return false;
}

void parse_tokens(void)
{
    ARR_INIT(&variables);

    while (parser.cursor < parser.token_arr.size)
    {
        if (parser.token.type == TOK_EOF) {
            advance();
            printf("EOF reached. Parsing completed successfully!\n");
            break;
        }

        parse_block();
    }
}

static void parse_block(void)
{
    switch (parser.token.type)
    {
    case TOK_IF:
        parse_if();
        break;
    case TOK_WHILE:
        parse_while();
        break;
    case TOK_VARIABLE:
        parse_variable();
        break;
    case TOK_PRINT:
        parse_print();
        break;
    default:
        assert("Unreachable" && false);
        break;
    }
}

static void parse_if(void)
{
    // Header
    parser.scope++;
    advance();

    // Body
    int expr_res = parse_expression();
    (void) expr_res;
    
    consume(TOK_OBRACE, "expected '{'");
    while (!at_end_symbol(TOK_CBRACE))
    {
        parse_block();
    }
    consume(TOK_CBRACE, "expected '}'");

    // Optional else
    // TODO implement match() function?
    if (strncmp("else", parser.token.start, parser.token.len) == 0)
    {
        advance();
        consume(TOK_OBRACE, "expected '{'");
        while (!at_end_symbol(TOK_CBRACE))
        {
            parse_block();
        }
        consume(TOK_CBRACE, "expected '}'");
    }

    // Footer
    parser.scope--;
}

static void parse_while(void)
{
    // Header
    parser.scope++;
    advance();

    // Body
    int expr_res = parse_expression();
    (void) expr_res;

    consume(TOK_OBRACE, "expected '{'");
    while (!at_end_symbol(TOK_CBRACE))
    {
        parse_block();
    }
    consume(TOK_CBRACE, "expected '}'");

    // Footer
    parser.scope--;
}

static void parse_variable(void)
{
    // Create the var struct in any case
    Variable var;
    var.name_addr = parser.token.start;
    var.name_len = parser.token.len;

    bool is_new = true;
    int var_index = -1;
    for (int i = 0; i < variables.size; i++) {
        if (var.name_len == variables.data[i].name_len &&
            strncmp(variables.data[i].name_addr, var.name_addr, var.name_len) == 0) 
        {
            is_new = false;
            var_index = i;   
        }
    }

    advance(); // consume variable name
    
    if (is_new && parser.scope > GLOBAL_SCOPE) {
        char err_buffer[ERR_MSG_SIZE];
        snprintf(err_buffer, ERR_MSG_SIZE, "variable '%.*s' declared in local scope", var.name_len, var.name_addr);
        report_error(err_buffer);
        // return;
    }

    consume(TOK_ASSIGN, "expected '=' after variable name");
    int expr_res = parse_expression();
    consume(TOK_SEMICOLON, "expected ';' at the end of expression");
    
    if (is_new) {
        var.value = expr_res;
        ARR_PUSH(&variables, var, Variable);
    } else {
        variables.data[var_index].value = expr_res;
    }
}

static void parse_print(void)
{
    // Header
    advance();

    // Body
    int expr_res = parse_expression();
    consume(TOK_SEMICOLON, "expected ';' at the end of the print expression");

    // Meh
    printf("%d\n", expr_res);
}

/*
 *
 *  Parse expression
 */

static int parse_expression(void)
{
    OpStack operators;
    ARR_INIT(&operators);
    NumStack numbers;
    ARR_INIT(&numbers);

    int expr_res = 0;
    int prec_lvl = 0;

    while (parser.token.type != TOK_SEMICOLON && parser.token.type != TOK_OBRACE && parser.token.type != TOK_EOF)
    {        
        // Current token, syntactic sugar
        Token token = parser.token;

        char tok_literal[token.len + 1];
        get_tok_literal(tok_literal, token);

        if (token.type == TOK_NUMBER) {
            ARR_PUSH(&numbers, atoi(tok_literal), int);
            advance(); // TODO find solution to remove advance() from here
            continue;
        }

        if (token.type == TOK_VARIABLE) {
            // TODO lookup variable
            advance(); // TODO find solution to remove advance() from here
            continue;
        }

        Op new_op = OpTable_get_op(token.type);
        if (new_op.prec == 0) { // The NULL Op
            char err_buffer[ERR_MSG_SIZE];
            snprintf(err_buffer, ERR_MSG_SIZE, "expected an operator, but got '%.*s' instead", token.len, token.start);
            report_error(err_buffer);
        }

        // TODO this if check and the next one seem hard coded to me. I'll go ahead in the development
        // of the parser and see if a good solution is going to take shape on its own.
        if (new_op.family == UNARY && parser.cursor < parser.token_arr.size-1 && parser.token_arr.data[parser.cursor+1].type != TOK_NUMBER) {
            get_tok_literal(tok_literal, parser.token_arr.data[parser.cursor+1]);
            char err_buffer[ERR_MSG_SIZE];
            snprintf(err_buffer, ERR_MSG_SIZE, "expected a number after unary operator, but got '%s' instead", tok_literal);
            report_error(err_buffer);
        }
        
        if (new_op.family == UNARY && parser.cursor == parser.token_arr.size-1) {
            report_error("expected a number after unary operator, but reached EOF instead");
        }

        if (new_op.tok_type == TOK_OPAREN) {
            prec_lvl++;
            advance(); // TODO find solution to remove advance() from here
            continue;
        }

        if (new_op.tok_type == TOK_CPAREN) {
            prec_lvl--;
            advance(); // TODO find solution to remove advance() from here
            continue;
        }
        
        new_op.prec += MAX_PREC * prec_lvl;

        Op top_op = OpStack_top(operators);

        // Explanation of the Stack-based precedence parsing [From 7:40]: 
        // https://youtu.be/c2nR3Ua4CFI?si=G-YMsmXbP65WApAh&t=460
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
            top_op = OpStack_top(operators);
        }

        ARR_PUSH(&operators, new_op, Op);

        advance();
    } // while()

    // advance(); // Consume ';' or EOF

    /* if prec_lvl != 0, therefore some open paren doesn't have the corresponding closing paren
    [e.g. 3 * (4 + 5) + (6 + 7], no error is reported, because for how the expression parsing works,
    they are not needed. */

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

        ARR_POP(&operators);
    }

    expr_res = ARR_TOP(&numbers);
    ARR_POP(&numbers);

    // Assert that everything was consumed
    assert(operators.size == 0);
    assert(numbers.size == 0);
    
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
        report_error("while parsing expression: expected number to perform unary operation");
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
        report_error("while parsing expression: expected right-hand side number to perform binary operation");
    }

    int r_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    if (ARR_IS_EMPTY(numbers)) {
        report_error("while parsing expression: expected left-hand side number to perform binary operation");
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
        report_error("while parsing expression: expected right-hand side number to perform comparison operation");
    }

    int r_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    if (ARR_IS_EMPTY(numbers)) {
        report_error("while parsing expression: expected left-hand side number to perform comparison operation");
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
        report_error("while parsing expression: expected right-hand side number to perform logical operation");
    }

    int r_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    if (ARR_IS_EMPTY(numbers)) {
        report_error("while parsing expression: expected left-hand side number to perform logical operation");
    }

    int l_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    int res = 0;
    if (tok_type == TOK_AND) res = l_num && r_num;
    else if (tok_type == TOK_OR) res = l_num || r_num;
    else assert("Unreachable" && false);

    ARR_PUSH(numbers, res, int);
}

// ARR_TOP is not usable because data of OpStack is a struct
static Op OpStack_top(OpStack operators) {
    return operators.size > 0 ? operators.data[operators.size - 1] : (Op){0};
}
