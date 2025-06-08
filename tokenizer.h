#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "array.h"

typedef enum TokType 
{
    TOK_ERROR,

    // (), {}
    TOK_OPAREN, TOK_CPAREN,
    TOK_OBRACE, TOK_CBRACE,
    
    // +, -, *, /
    TOK_PLUS, TOK_MINUS, 
    TOK_STAR, TOK_SLASH,

    // <, >, <=, >=
    TOK_LT, TOK_GT,
    TOK_LE, TOK_GE,

    // =, ==, !=
    TOK_ASSIGN,
    TOK_EQ, TOK_NE,

    TOK_NOT, TOK_AND, TOK_OR,

    TOK_NUMBER,

    TOK_SEMICOLON, 

    // if, else, while, print
    TOK_IF, TOK_ELSE, TOK_WHILE, TOK_PRINT, 
    
    // my_var, MyProc
    TOK_VAR, TOK_PROC,
} TokType;

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
