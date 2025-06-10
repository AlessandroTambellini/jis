#include "parser.h"
#include "utils.h"
#include "tokenizer.h"

#define GLOBAL_SCOPE 0

#define ERR_MSG_SIZE 256

typedef struct Parser {
    int cursor;
    int scope;
    Token token;
    TokenArr token_arr;
} Parser;

typedef struct Variable {
    char *name_addr;
    int name_len;
    float value;
} Variable;

typedef struct Task {
    char *name_addr;
    int name_len;
    int proc_start;
} Task;

typedef enum OpFamily {
    GROUPING,
    ARITHMETIC,
    COMPARISON,
    LOGICAL,
} OpFamily;

typedef struct Op {
    OpFamily family;
    int prec;
    TokType tok_type;
} Op;

/* There are 5 different precedence levels. 
Don't confuse them with OpFamily! 
Operators of the same family, might have a different precedence; e.g. '+' and '*'. */
#define MAX_PREC 6

Op OpTable[] = 
{
    {GROUPING,   MAX_PREC,     TOK_OPAREN}, // (
    {GROUPING,   MAX_PREC,     TOK_CPAREN}, // )
    {ARITHMETIC, MAX_PREC - 1, TOK_STAR  }, // *
    {ARITHMETIC, MAX_PREC - 1, TOK_SLASH }, // /
    {ARITHMETIC, MAX_PREC - 2, TOK_PLUS  }, // +
    {ARITHMETIC, MAX_PREC - 2, TOK_MINUS }, // -
    {COMPARISON, MAX_PREC - 3, TOK_LT    }, // <
    {COMPARISON, MAX_PREC - 3, TOK_GT    }, // >
    {COMPARISON, MAX_PREC - 3, TOK_LE    }, // <=
    {COMPARISON, MAX_PREC - 3, TOK_GE    }, // >=
    {COMPARISON, MAX_PREC - 3, TOK_EQ    }, // ==
    {COMPARISON, MAX_PREC - 3, TOK_NE    }, // !=
    {LOGICAL,    MAX_PREC - 4, TOK_AND   }, // &&
    {LOGICAL,    MAX_PREC - 5, TOK_OR    }, // ||
    {0,          0,            0         }  // NULL (terminator)
};

DECLARE_ARR(VarArr, Variable)
DECLARE_ARR(TaskArr, Task)

DECLARE_ARR(OpStack, Op)
DECLARE_ARR(NumStack, float)

static void advance(void);
static void consume(TokType type, char *err_msg);
static void jump(Token token, int cursor, int scope);
static bool reached_eoe(bool is_condition);
static bool reached_eob(void);
static bool reached_eof(void);
static void report_error(char *err_msg);

static void parse_block(bool branched);
static void parse_task(void);
static void parse_if(bool branched);
static void parse_while(bool branched);
static void exec_task(bool branched);
static void parse_variable(bool branched);
static void parse_print(bool branched);

static int parse_expression(bool branched, bool is_condition);
static float lookup_variable(char *name_addr, int name_len);
static Op get_op_from_OpTable(TokType tok_type);
static void perform_arithmetic_op(NumStack *numbers, TokType tok_type);
static void perform_comparison_op(NumStack *numbers, TokType tok_type);
static void perform_logical_op(NumStack *numbers, TokType tok_type);
static Op OpStack_top(OpStack operators);

Parser parser;

VarArr variables;
TaskArr tasks;

void init_parser(TokenArr token_arr)
{
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

static void jump(Token token, int cursor, int scope) 
{
    parser.token = token;
    parser.cursor = cursor;
    parser.scope = scope;
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
        parser.scope--;
        consume(TOK_CBRACE, "expected '}'");
        return true;
    }
    return false;
}

// eof: end of file
static bool reached_eof(void) {
    return parser.cursor > parser.token_arr.size - 1;
}

static void report_error(char *err_msg) 
{    
    printf("Line %d: %s.\n", parser.token.line, err_msg);
    exit(EXIT_FAILURE);
}

void parse_tokens(void)
{
    ARR_INIT(&variables);
    while (!reached_eof())
    {        
        parse_block(true);
    }
}

static void parse_block(bool branched)
{
    switch (parser.token.type)
    {
    case TOK_TASK:
        parse_task();
        break;
    case TOK_IF:
        parse_if(branched);
        break;
    case TOK_WHILE:
        parse_while(branched);
        break;
    case TOK_EXEC_TASK:
        exec_task(branched);
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

// This function is executed just at the first parsing of a task
static void parse_task(void)
{
    Task task;
    task.name_addr = parser.token.start;
    task.name_len = parser.token.len;
    advance(); // consume the task name

    consume(TOK_OBRACE, "expected '{' after task name");

    if (parser.scope > GLOBAL_SCOPE) {
        char err_buffer[ERR_MSG_SIZE];
        snprintf(err_buffer, ERR_MSG_SIZE, "task '%.*s' declared in local scope", task.name_len, task.name_addr);
        report_error(err_buffer);
    }

    task.proc_start = parser.cursor;

    ARR_PUSH(&tasks, task, Task);

    // Parse the body in search of semantic errors.
    parser.scope++;
    while (!reached_eob()) {
        parse_block(false); // parsed, but not executed
    }
}

static void parse_if(bool branched)
{
    parser.scope++;
    advance();

    float expr_res = parse_expression(branched, true);
    
    while (!reached_eob())
    {
        parse_block(branched && expr_res);
    }

    // Optional else
    if (match("else", parser.token.start, parser.token.len))
    {
        parser.scope++;
        advance();
        
        consume(TOK_OBRACE, "expected '{' after 'else'");
        while (!reached_eob())
        {
            parse_block(branched && !expr_res);
        }
    }
}

static void parse_while(bool branched)
{
    parser.scope++;
    advance();

    bool parsed_once = false;

    while (1)
    {
        Token s_token = parser.token;
        int s_cursor = parser.cursor;
        int s_scope = parser.scope;

        float expr_res = parse_expression(branched, true);

        while (!reached_eob())
        {
            parse_block(branched && expr_res);
        }

        parsed_once = true;

        if (!branched) break;
        
        if (!expr_res && parsed_once) break;
        else jump(s_token, s_cursor, s_scope);
    }
}

static void exec_task(bool branched) 
{
    advance(); // consume 'exec'

    // Get the task name
    char *task_name_addr = parser.token.start;
    int task_name_len = parser.token.len;

    // Once it is verified that the request is semantically correct (or, seen from another perspective,
    // once it is verified that what the user meant is to execute a task),
    // search for the requested task.
    bool task_exists = false;
    int task_idx = -1;

    for (int i = 0; i < tasks.size; i++) {
        if (task_name_len == tasks.data[i].name_len && 
            strncmp(tasks.data[i].name_addr, task_name_addr, task_name_len) == 0
        ) {
            task_exists = true;
            task_idx = i;
        }
    }

    if (branched && !task_exists) {
        char err_buffer[ERR_MSG_SIZE];
        snprintf(err_buffer, ERR_MSG_SIZE, "task '%.*s' doesn't exists", task_name_len, task_name_addr);
        report_error(err_buffer);
    }

    advance(); // consume proc name
    consume(TOK_SEMICOLON, "expected ';' after procedure name");

    // Save the current state of execution
    Token s_token = parser.token;
    int s_cursor = parser.cursor;
    int s_scope = parser.scope;

    if (branched) 
    {
        parser.cursor = tasks.data[task_idx].proc_start;
        parser.token = parser.token_arr.data[parser.cursor];
        parser.scope = 0; // Because a task can be only at the global scope

        while (!reached_eob()) 
        {
            parse_block(branched);
        }

        jump(s_token, s_cursor, s_scope);
    }
}

static void parse_variable(bool branched)
{
    // Create the var struct case
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
    
    if (branched && is_new && parser.scope > GLOBAL_SCOPE) {
        char err_buffer[ERR_MSG_SIZE];
        snprintf(err_buffer, ERR_MSG_SIZE, "variable '%.*s' declared in local scope", var.name_len, var.name_addr);
        report_error(err_buffer);
    }

    consume(TOK_ASSIGN, "expected '=' after variable name");
    float expr_res = parse_expression(branched, false);
    
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
    advance();

    float expr_res = parse_expression(branched, false);

    if (branched) {
        printf("%f\n", expr_res);
    }
}

/*
 *
 *  Parse expression
 */

static int parse_expression(bool branched, bool is_condition)
{
    OpStack operators;
    ARR_INIT(&operators);
    NumStack numbers;
    ARR_INIT(&numbers);

    float expr_res = 0;
    int prec_lvl = 0;

    while (!reached_eoe(is_condition))
    {        
        // Current token, syntactic sugar
        Token token = parser.token;

        if (token.type == TOK_NUMBER) 
        {
            char tok_literal[token.len+1];
            snprintf(tok_literal, token.len+1, "%.*s", token.len, token.start);
            ARR_PUSH(&numbers, atoi(tok_literal), float);
            advance(); // TODO find solution to remove advance() from here
            continue;
        }

        if (token.type == TOK_VAR) 
        {
            float var_value = branched ? lookup_variable(token.start, token.len) : 0.0f;
            ARR_PUSH(&numbers, var_value, float);
            advance(); // TODO find solution to remove advance() from here
            continue;
        }

        Op new_op = get_op_from_OpTable(token.type);
        if (new_op.prec == 0) { // The NULL Op
            char err_buffer[ERR_MSG_SIZE];
            snprintf(err_buffer, ERR_MSG_SIZE, "expected an operator or terminating symbol '%c', but got '%.*s' instead", (is_condition ? '{' : ';'), token.len, token.start);
            report_error(err_buffer);
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
            case ARITHMETIC:
                perform_arithmetic_op(&numbers, top_op.tok_type);
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
    e.g. '3 * (4 + 5) + (6 + 7', no error is reported, because for how the expression parsing works,
    they are not needed. */

    // Perform remaining operations in order of apparence
    while (!ARR_IS_EMPTY(&operators))
    {
        Op op = OpStack_top(operators);
        switch (op.family)
        {
        case ARITHMETIC:
            perform_arithmetic_op(&numbers, op.tok_type);
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

    // Assert that the stacks are empty
    assert(operators.size == 0);
    assert(numbers.size == 0);
    
    ARR_FREE(&operators);
    ARR_FREE(&numbers);

    return expr_res;
}

static Op get_op_from_OpTable(TokType tok_type)
{
    for (size_t i = 0; OpTable[i].prec != 0; i++) {
        if (OpTable[i].tok_type == tok_type) {
            return OpTable[i];
        }
    }

    return (Op){0, 0, 0};
}

static void perform_arithmetic_op(NumStack *numbers, TokType tok_type)
{
    if (ARR_IS_EMPTY(numbers)) {
        report_error("expected right-hand side number to perform arithmetic operation");
    }

    float r_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    if (ARR_IS_EMPTY(numbers)) {
        report_error("expected left-hand side number to perform arithmetic operation");
    }

    float l_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    float res;
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

    ARR_PUSH(numbers, res, float);
}

static void perform_comparison_op(NumStack *numbers, TokType tok_type)
{
    if (ARR_IS_EMPTY(numbers)) {
        report_error("expected right-hand side number to perform comparison operation");
    }

    float r_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    if (ARR_IS_EMPTY(numbers)) {
        report_error("expected left-hand side number to perform comparison operation");
    }

    float l_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    float res = 0;
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

    ARR_PUSH(numbers, res, float);
}

static void perform_logical_op(NumStack *numbers, TokType tok_type)
{
    if (ARR_IS_EMPTY(numbers)) {
        report_error("expected right-hand side number to perform logical operation");
    }

    float r_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    if (ARR_IS_EMPTY(numbers)) {
        report_error("expected left-hand side number to perform logical operation");
    }

    float l_num = ARR_TOP(numbers);
    ARR_POP(numbers);

    float res = 0;
    if (tok_type == TOK_AND) res = l_num && r_num;
    else if (tok_type == TOK_OR) res = l_num || r_num;
    else assert("Unreachable" && false);

    ARR_PUSH(numbers, res, float);
}

// ARR_TOP is not usable because data of OpStack is a struct
static Op OpStack_top(OpStack operators) {
    return operators.size > 0 ? operators.data[operators.size - 1] : (Op){0};
}

static float lookup_variable(char *name_addr, int name_len)
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
