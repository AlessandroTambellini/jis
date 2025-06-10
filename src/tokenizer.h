#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "utils.h"

typedef enum TokType 
{
    // (), {}
    TOK_OPAREN, TOK_CPAREN,
    TOK_CBRACE,
    
    // +, -, *, /
    TOK_PLUS, TOK_MINUS, 
    TOK_STAR, TOK_SLASH,

    // <, >, <=, >=
    TOK_LT, TOK_GT,
    TOK_LE, TOK_GE,

    // =, ==, !=
    TOK_ASSIGN,
    TOK_EQ, TOK_NE,

    // &&, ||
    TOK_AND, TOK_OR,

    TOK_NUMBER,

    // Terminating chars: ';', '{'
    TOK_SEMICOLON, TOK_OBRACE,

    // if, else, while, print
    TOK_IF, TOK_ELSE, TOK_WHILE, TOK_EXEC_TASK, TOK_PRINT, 
    
    // my_var, MyProc
    TOK_VAR, TOK_TASK,
} TokType;

/* A task is a specific piece of work to be done,
a procedure is the set of steps to be performed to accomplish the task.
So, the body of the task is called procedure. */

typedef struct Token {
    TokType type;
    char *start;
    int len;
    int line;
} Token;

DECLARE_ARR(TokenArr, Token)

typedef struct Tokenizer {
    char *source_code;
    int cursor;
    char ch; // Syntatic sugar for src[cursor] 
    int line; // Should line be inside or outside of the tokenizer?
} Tokenizer;

void init_tokenizer(char *source_code);
void collect_tokens(TokenArr *ta, bool *error);
void print_token(Token token);

#endif // TOKENIZER_H
