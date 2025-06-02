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

Tokenizer t;

void init_tokenizer(char *source_code)
{
    t.source_code = source_code;
    t.cursor = -1;
    advance();
}

void collect_tokens(TokenArr *ta)
{
    for (;;)
    {
        Token token = get_next_token();

        if (token.type == TOK_EOF) break;

        if (token.type == TOK_ERROR) {
            printf("Unrecognized token: "); print_token(token); printf("\n");
        }
    
        if (token.type != TOK_ERROR && token.type != TOK_NULL) {
            ARR_PUSH(ta, token, Token);
        }
    }
}

Token get_next_token(void)
{
    Token token;

    switch (t.ch)
    {
    case '(':
        create_token(&token, TOK_OPEN_PAREN, 1);
        break;
    case ')':
        create_token(&token, TOK_CLOSE_PAREN, 1);
        break;
    case '[':
        create_token(&token, TOK_OPEN_SQUARE, 1);
        break;
    case ']':
        create_token(&token, TOK_CLOSE_SQUARE, 1);
        break;
    case '{':
        create_token(&token, TOK_OPEN_BRACE, 1);
        break;
    case '}': 
        create_token(&token, TOK_CLOSE_BRACE, 1);
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

    case '<': {
        if (look_ahead() == '=') {
            advance();
            create_token(&token, TOK_LT, 2);
        } else {
            create_token(&token, TOK_LESS, 1);
        }
    } break;

    case '>': {
        if (look_ahead() == '=') {
            advance();
            create_token(&token, TOK_GT, 2);
        } else {
            create_token(&token, TOK_GREATER, 1);
        }
    } break;

    case '~': {
        if (look_ahead() == '=') {
            advance();
            create_token(&token, TOK_INEQUALITY, 2);
        } else {
            // TODO manage the error
            printf("ERROR: Invalid token\n");
        }
    } break;

    case '=': {
        if (look_ahead() == '=') {
            advance();
            create_token(&token, TOK_EQUALITY, 2);
        } else {
            create_token(&token, TOK_ASSIGN, 1);
        }
    } break;

    case ' ':
    case '\t':
    case '\r':
        create_token(&token, TOK_NULL, 1);
        break;
        
    case '\n':
        create_token(&token, TOK_NULL, 1);
        t.line++;
        break;

    case '\0':
        create_token(&token, TOK_EOF, 1);
        break;
    
    default:
        if (is_digit(t.ch)) {
            int num_len = scan_number();
            create_token(&token, TOK_NUMBER, num_len);
        } else {
            create_token(&token, TOK_ERROR, 1);
        }
    }

    advance();

    return token;
}

static void create_token(Token *token, TokType type, int len)
{
    token->start = &t.source_code[t.cursor - (len-1)];
    token->len = len;
    token->type = type;
    token->line = t.line;
}

static void advance(void)
{
    if ((size_t)t.cursor + 1 < strlen(t.source_code)) {
        t.cursor++;
        t.ch = t.source_code[t.cursor];
    } else {
        t.ch = '\0';
    }
}

static char look_ahead(void)
{
    int i = (size_t)t.cursor + 1 < strlen(t.source_code) ? t.cursor + 1 : t.cursor; 
    return t.source_code[i];
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
    case   TOK_OPEN_PAREN: return "OPEN_PAREN";
    case  TOK_CLOSE_PAREN: return "CLOSE_PAREN";
    case  TOK_OPEN_SQUARE: return "OPEN_SQUARE";
    case TOK_CLOSE_SQUARE: return "CLOSE_SQUARE";
    case   TOK_OPEN_BRACE: return "OPEN_BRACE";
    case  TOK_CLOSE_BRACE: return "CLOSE_BRACE";
    case         TOK_PLUS: return "PLUS";
    case        TOK_MINUS: return "MINUS";
    case         TOK_STAR: return "STAR";
    case        TOK_SLASH: return "SLASH";
    case         TOK_LESS: return "LESS";
    case      TOK_GREATER: return "GREATER";
    case           TOK_LT: return "LT";
    case           TOK_GT: return "GT";
    case       TOK_ASSIGN: return "ASSIGN";
    case     TOK_EQUALITY: return "EQUALITY";
    case   TOK_INEQUALITY: return "INEQUALITY";
    case       TOK_NUMBER: return "NUMBER";
    case         TOK_NULL: return "NULL";
    case          TOK_EOF: return "EOF";
    
    default:
        assert("Unreachable" && false);
    }

    return NULL;
}

