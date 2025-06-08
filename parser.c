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

typedef struct Procedure {
    char *name_addr;
    int name_len;
    int proc_start;
} Procedure;

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
DECLARE_ARR(ProcStack, Procedure)

DECLARE_ARR(OpStack, Op)
DECLARE_ARR(NumStack, int)

static void parse_block(bool branched);
static void parse_if(bool branched);
static void parse_while(bool branched);
static void parse_variable(bool branched);
static void parse_print(bool branched);

static void parse_procedure(void);
static void exec_proc(void);

static int parse_expression(bool is_condition);

static void advance(void);
static void consume(TokType type, char *err_msg);

static void jump_back(Token old_token, int old_cursor, int old_scope);
static void jump_forward(Token last_token, int last_cursor, int last_scope);
static void save_state(Token token, int cursor, int scope);

static void report_error(char *err_msg);

int lookup_variable(char *name_addr, int name_len);

static bool reached_eoe(bool is_condition);
static bool reached_eob(void);
static bool reached_eof(void);

static void get_tok_literal(char *tok_literal, Token token);
static Op OpTable_get_op(TokType tok_type);

static void perform_unary_op(NumStack *numbers, TokType tok_type);
static void perform_binary_op(NumStack *numbers, TokType tok_type);
static void perform_comparison_op(NumStack *numbers, TokType tok_type);
static void perform_logical_op(NumStack *numbers, TokType tok_type);
static Op OpStack_top(OpStack operators);

Parser parser;

VarStack variables;
ProcStack procedures;

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

static void jump_back(Token old_token, int old_cursor, int old_scope) 
{
    parser.token = old_token;
    parser.cursor = old_cursor;
    parser.scope = old_scope;
}

static void jump_forward(Token last_token, int last_cursor, int last_scope)
{
    parser.token = last_token;
    parser.cursor = last_cursor;
    parser.scope = last_scope;
}

static void save_state(Token token, int cursor, int scope)
{
    (void) token;
    (void) cursor;
    (void) scope;
    // TODO static fields is not a solution
}

static void report_error(char *err_msg) {
    // I leave these two properties inside parser for now,
    // even though I don't know if they may have some utility.
    parser.error = true;
    strcpy(parser.err_msg, err_msg);
    
    printf("Line %d: %s.\n", parser.token.line, parser.err_msg);
    
    exit(EXIT_FAILURE);
}

// eoe: end of expression
static bool reached_eoe(bool is_condition) 
{
    if (is_condition) {
        if (parser.token.type == TOK_OBRACE || reached_eof()) {
            consume(TOK_OBRACE, "expected '{'");
            return true;
        }
    } else {
        if (parser.token.type == TOK_SEMICOLON || reached_eof()) {
            consume(TOK_SEMICOLON, "expected ';'");
            return true;
        }
    } 
    return false; 
}

// eob: end of block
static bool reached_eob(void)
{
    if (parser.token.type == TOK_CBRACE || reached_eof()) {
        consume(TOK_CBRACE, "expected '}'");
        return true;
    }
    return false;
}

static bool reached_eof(void) {
    return parser.cursor > parser.token_arr.size - 1;
}

int lookup_variable(char *name_addr, int name_len)
{
    for (int i = 0; i < variables.size; i++) {
        if (name_len == variables.data[i].name_len &&
            strncmp(variables.data[i].name_addr, name_addr, name_len) == 0) 
        {
            return variables.data[i].value;
        }
    }

    char err_buffer[ERR_MSG_SIZE];
    snprintf(err_buffer, ERR_MSG_SIZE, "variable '%.*s' not declared", name_len, name_addr);
    report_error(err_buffer);
    return -1;
}

void parse_tokens(void)
{
    ARR_INIT(&variables);

    while (parser.cursor < parser.token_arr.size)
    {        
        parse_block(true);
    }
    printf("Parsing completed successfully!\n");
}

static void parse_block(bool branched)
{
    // Token token = parser.token_arr.data[parser.cursor];
    switch (parser.token.type)
    {
    case TOK_PROC_NAME:
        parse_procedure();
        break;
    case TOK_IF:
        parse_if(branched);
        break;
    case TOK_WHILE:
        parse_while(branched);
        break;
    case TOK_EXEC_PROC:
        exec_proc();
        break;
    case TOK_VAR:
        parse_variable(branched);
        break;
    case TOK_PRINT:
        parse_print(branched);
        break;
    default:
        assert("Unreachable" && false);
        break;
    }
}

static void parse_if(bool branched)
{
    // Header
    parser.scope++;
    advance();

    // Body
    int expr_res = parse_expression(true);
    bool if_branching = expr_res != 0 ? true : false;
    
    while (!reached_eob())
    {
        parse_block(branched && if_branching);
    }

    // Optional else
    if (strlen("else") == parser.token.len && strncmp("else", parser.token.start, parser.token.len) == 0)
    {
        advance();
        consume(TOK_OBRACE, "expected '{'");
        while (!reached_eob())
        {
            parse_block(branched && !if_branching);
        }
    }

    // Footer
    parser.scope--;
}

static void parse_while(bool branched)
{
    // Header
    parser.scope++;
    advance();

    // Body
    Token current_token = parser.token;
    int current_cursor = parser.cursor;
    int current_scope = parser.scope; // Do I need it?

    int expr_res = parse_expression(true);
    bool exec_body = expr_res != 0 ? true : false;

    while (exec_body)
    {
        while (!reached_eob())
        {
            parse_block(branched && exec_body);
        }

        if (!branched) break;

        Token last_token = parser.token;
        int last_cursor = parser.cursor;
        int last_scope = parser.scope; 

        jump_back(current_token, current_cursor, current_scope);

        expr_res = parse_expression(true);
        exec_body = expr_res != 0 ? true : false;

        if (!exec_body) jump_forward(last_token, last_cursor, last_scope);
    }

    // Footer
    parser.scope--;
}

static void exec_proc(void) {
    /*
    for name in proc_arr
        check if the current name is in it
            
    if so, exec it. 
    */

    advance(); // consume 'exec'

    // Get the procedure name
    char *proc_name_addr = parser.token.start;
    int proc_name_len = parser.token.len;

    advance(); // consume proc name
    consume(TOK_SEMICOLON, "expected ';' after procedure name");

    // Save the current state of execution
    int current_cursor = parser.cursor;
    Token current_token = parser.token;
    // TODO scope isn't managed

    // Once it is verified that the request is semantically correct (or, seen from another perspective,
    // once it is verified that what the user meant is to execute a task),
    // search for the requested task.
    bool proc_exists = false;
    int proc_idx = -1;

    for (int i = 0; i < procedures.size; i++) {
        if (proc_name_len == procedures.data[i].name_len 
            && strncmp(procedures.data[i].name_addr, proc_name_addr, proc_name_len) == 0
        ) {
            proc_exists = true;
            proc_idx = i;
        }
    }

    if (!proc_exists) {
        char err_buffer[ERR_MSG_SIZE];
        snprintf(err_buffer, ERR_MSG_SIZE, "procedure '%.*s' doesn't exists", proc_name_len, proc_name_addr);
        report_error(err_buffer);
    }

    parser.cursor = procedures.data[proc_idx].proc_start;
    parser.token = parser.token_arr.data[parser.cursor];
    // TODO scope isn't managed

    while (!reached_eob()) 
    {
        parse_block(true);
    }

    // Once the task procedure is executed, come back to the previous point
    parser.cursor = current_cursor;
    parser.token = current_token;
}

static void parse_variable(bool branched)
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

    advance(); // consume the variable name
    
    if (is_new && parser.scope > GLOBAL_SCOPE) {
        char err_buffer[ERR_MSG_SIZE];
        snprintf(err_buffer, ERR_MSG_SIZE, "variable '%.*s' declared in local scope", var.name_len, var.name_addr);
        report_error(err_buffer);
    }

    consume(TOK_ASSIGN, "expected '=' after variable name");
    int expr_res = parse_expression(false);
    
    // If the branch in which this variable is located, is executed, so assign the value to it.
    if (branched) {
        if (is_new) {
            var.value = expr_res;
            ARR_PUSH(&variables, var, Variable);
        } else {
            variables.data[var_index].value = expr_res;
        }
    }
}

static void parse_print(bool branched)
{
    // Header
    advance();

    // Body
    int expr_res = parse_expression(false);

    if (branched) {
        printf("%d\n", expr_res);
    }
}

static void parse_procedure(void)
{
    Procedure proc;
    proc.name_addr = parser.token.start;
    proc.name_len = parser.token.len;
    advance(); // consume the procedure name

    consume(TOK_OBRACE, "expected '{' after procedure name");
    proc.proc_start = parser.cursor;

    ARR_PUSH(&procedures, proc, Procedure);

    // Parse the body in search of semantic errors.
    while (!reached_eob()) {
        parse_block(false); // parsed, but not executed
    }
}

/*
 *
 *  Parse expression
 */

static int parse_expression(bool is_condition)
{
    OpStack operators;
    ARR_INIT(&operators);
    NumStack numbers;
    ARR_INIT(&numbers);

    int expr_res = 0;
    int prec_lvl = 0;

    while (!reached_eoe(is_condition))
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

        if (token.type == TOK_VAR) {
            int var_value = lookup_variable(token.start, token.len);
            ARR_PUSH(&numbers, var_value, int);
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
