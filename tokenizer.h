#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "array.h"

typedef enum TokType 
{
    TOK_ERROR,

    // (), [], {}
    TOK_OPEN_PAREN, TOK_CLOSE_PAREN,
    TOK_OPEN_SQUARE, TOK_CLOSE_SQUARE,
    TOK_OPEN_BRACE, TOK_CLOSE_BRACE,
    
    // +, -, *, /
    TOK_PLUS, TOK_MINUS, 
    TOK_STAR, TOK_SLASH,

    // <, >, <=, >=
    TOK_LESS, TOK_GREATER,
    TOK_LT, TOK_GT,

    // =, ==, ~=
    TOK_ASSIGN,
    TOK_EQUALITY, TOK_INEQUALITY,

    TOK_NUMBER,

    TOK_NULL, TOK_EOF
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
void collect_tokens(TokenArr *ta);
Token get_next_token(void);
void print_token(Token token);

#endif // TOKENIZER_H
