#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include "tokenizer.h"
#include "array.h"

static void advance(void);
static char look_ahead(void);
static bool is_digit(char c);
static int scan_number(void);
static void create_token(Token *token, TokType type, int len);
static char *tok_type_to_string(TokType tt);

// I have no reason for now to change this variable from being a global one.
Tokenizer tokenizer;

void init_tokenizer(char *source_code)
{
    tokenizer.source_code = source_code;
    tokenizer.cursor = -1;
    tokenizer.line = 1;
    advance();
}

void collect_tokens(TokenArr *ta)
{
    for (;;)
    {
        Token token = {0};

        switch (tokenizer.ch)
        {
        case '(':
            create_token(&token, TOK_OPAREN, 1);
            break;
        case ')':
            create_token(&token, TOK_CPAREN, 1);
            break;

        case '+':
            create_token(&token, TOK_PLUS, 1);
            break;
        case '-':
            create_token(&token, TOK_MINUS, 1);
            break;
        case '*':
            create_token(&token, TOK_STAR, 1);
            break;
        case '/':
            create_token(&token, TOK_SLASH, 1);
            break;

        case '!': {
            if (look_ahead() == '=') {
                advance();
                create_token(&token, TOK_NE, 2);
            } else {
                create_token(&token, TOK_NOT, 1);
            }
        } break;
        
        case '&': {
            if (look_ahead() == '&') {
                advance();
                create_token(&token, TOK_AND, 2);
            }
        } break;

        case '|': {
            if (look_ahead() == '|') {
                advance();
                create_token(&token, TOK_OR, 2);
            }
        } break;

        case '<': {
            if (look_ahead() == '=') {
                advance();
                create_token(&token, TOK_LE, 2);
            } else {
                create_token(&token, TOK_LT, 1);
            }
        } break;

        case '>': {
            if (look_ahead() == '=') {
                advance();
                create_token(&token, TOK_GE, 2);
            } else {
                create_token(&token, TOK_GT, 1);
            }
        } break;

        case '=': {
            if (look_ahead() == '=') {
                advance();
                create_token(&token, TOK_EQ, 2);
            } else {
                create_token(&token, TOK_ASSIGN, 1);
            }
        } break;

        case ' ':
        case '\t':
        case '\r':
            break;
            
        case '\n':
            tokenizer.line++;
            break;

        case '\0':
            create_token(&token, TOK_EOF, 1);
            break;
        
        default:
            if (is_digit(tokenizer.ch)) {
                int num_len = scan_number();
                create_token(&token, TOK_NUMBER, num_len);
            } else {
                printf("Line %d: error: unknown token starting with '%c'.\n", tokenizer.line, tokenizer.ch);
            }
        }

        advance();
        
        if (token.start != NULL) ARR_PUSH(ta, token, Token);
        if (token.type == TOK_EOF) break;
    }
}

static void create_token(Token *token, TokType type, int len)
{
    token->start = &tokenizer.source_code[tokenizer.cursor - (len-1)];
    token->len = len;
    token->type = type;
    token->line = tokenizer.line;
}

static void advance(void)
{
    if ((size_t)tokenizer.cursor + 1 < strlen(tokenizer.source_code)) {
        tokenizer.cursor++;
        tokenizer.ch = tokenizer.source_code[tokenizer.cursor];
    } else {
        tokenizer.ch = '\0';
    }
}

static char look_ahead(void)
{
    int i;
    if ((size_t)tokenizer.cursor + 1 < strlen(tokenizer.source_code)) {
        i = tokenizer.cursor + 1;
    } else {
        i = tokenizer.cursor; 
    }
    return tokenizer.source_code[i];
}

static bool is_digit(char c) {
    return '0' <= c && c <= '9';
}

// TODO scan floating point numbers
static int scan_number(void) 
{
    int len = 1;
    while (is_digit(look_ahead()))
    {
        len++;
        advance();
    }

    return len;
}

void print_token(Token token)
{
    for (int i = 0; i < token.len; i++) {
        printf("%c", token.start[i]);
    }
    printf(": %s", tok_type_to_string(token.type));
}

static char *tok_type_to_string(TokType tt)
{
    switch (tt)
    {
    case        TOK_ERROR: return "ERROR";

    case   TOK_OPAREN: return "OPEN_PAREN";
    case  TOK_CPAREN: return "CLOSE_PAREN";
    case  TOK_OSQUARE: return "OPEN_SQUARE";
    case TOK_CSQUARE: return "CLOSE_SQUARE";
    case   TOK_OBRACE: return "OPEN_BRACE";
    case  TOK_CBRACE: return "CLOSE_BRACE";

    case         TOK_PLUS: return "PLUS";
    case        TOK_MINUS: return "MINUS";
    case         TOK_STAR: return "STAR";
    case        TOK_SLASH: return "SLASH";

    case           TOK_LT: return "LT";
    case           TOK_GT: return "GT";
    case           TOK_LE: return "LE";
    case           TOK_GE: return "GE";

    case       TOK_ASSIGN: return "ASSIGN";
    case           TOK_EQ: return "EQ";
    case           TOK_NE: return "NE";

    case          TOK_NOT: return "NOT";
    case          TOK_AND: return "AND";
    case           TOK_OR: return "OR";
    
    case       TOK_NUMBER: return "NUMBER";
    
    case         TOK_NULL: return "NULL";
    case          TOK_EOF: return "EOF";
    
    default:
        assert("Unreachable" && false);
    }

    return NULL;
}
