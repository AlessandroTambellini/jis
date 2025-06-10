#include "tokenizer.h"
#include "utils.h"

static void advance(void);
static char look_ahead(void);

static bool is_digit(char c);
static bool is_alpha(char c);
static bool is_upp(char c);

static int get_number_len(bool *error);
static int get_identifier_len(void);

static void create_token(Token *token, TokType type, int len);
static char *tok_type_to_string(TokType tt);

// I have no reason for now to change this variable from being a global one.
Tokenizer tokenizer;

void init_tokenizer(char *source_code)
{
    tokenizer.source_code = source_code;
    tokenizer.cursor = -1;
    tokenizer.line = 1;
    tokenizer.ch = 0;
    advance();
}

void collect_tokens(TokenArr *ta, bool *error)
{
    while (tokenizer.ch != '\0') // eof
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
        case '/': {
            if (look_ahead() == '/') {
                while (look_ahead() != '\n') advance();
            } else {
                create_token(&token, TOK_SLASH, 1);
            }
        } break;

        case '!': {
            if (look_ahead() == '=') {
                advance();
                create_token(&token, TOK_NE, 2);
            } else {
                printf("Line %d: error: unknown token '%c'.\n", tokenizer.line, tokenizer.ch);
                *error = true;
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

        case ';':
            create_token(&token, TOK_SEMICOLON, 1);
            break;
        case '{':
            create_token(&token, TOK_OBRACE, 1);
            break;
        case '}':
            create_token(&token, TOK_CBRACE, 1);
            break;

       case '\n':
            tokenizer.line++;
            break;

        case ' ':
        case '\t':
        case '\r':
        case '\0':
            break;
        
        default: {
            if (is_digit(tokenizer.ch) || tokenizer.ch == '.') {
                int num_len = get_number_len(error);
                create_token(&token, TOK_NUMBER, num_len);
            } 
            else if (is_alpha(tokenizer.ch) || tokenizer.ch == '_') 
            {
                char *start = &tokenizer.source_code[tokenizer.cursor];
                int len = get_identifier_len();

                TokType type = TOK_VAR;

                if (match("if", start, len)) type = TOK_IF;
                else if (match("else", start, len)) type = TOK_ELSE;
                else if (match("while", start, len)) type = TOK_WHILE;
                else if (match("print", start, len)) type = TOK_PRINT;
                else if (match("exec", start, len)) type = TOK_EXEC_TASK;
                else if (is_upp(start[0])) type = TOK_TASK;

                create_token(&token, type, len);

            } else {
                printf("Line %d: error: unknown token starting with '%c'.\n", tokenizer.line, tokenizer.ch);
                *error = true;
            }
        } break;
        }

        advance();
        
        // Skip ' ', '\t', '\n', '\0'
        if (token.start != NULL) {
            ARR_PUSH(ta, token, Token);
        }
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

static bool is_alpha(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

static bool is_upp(char c) { return 'A' <= c && c <= 'Z'; }

static int get_number_len(bool *error) 
{
    /* there can be a digit or a dot. if a dot is found, it cannot be repeated anymore
    and it cannot be at the end of the number. */

    int len = 1;
    int dots = tokenizer.ch == '.' ? 1 : 0;
    while (is_digit(look_ahead()) || look_ahead() == '.') {
        if (look_ahead() == '.') dots++;
        len++;
        advance();
    }

    if (tokenizer.ch == '.') {
        printf("Line %d: '.' at the end of number.\n", tokenizer.line);
        *error = true;
    }
    if (dots > 1) {
        printf("Line %d: more than a single '.' in number.\n", tokenizer.line);
        *error = true;
    }

    /* NOTE: With this approach, if a string like '123abc' is encountered, 
    '123' is taken and 'abc' left for the next tokenization cycle. */

    return len;
}

static int get_identifier_len(void) 
{
    int len = 1;
    while (is_alpha(look_ahead()) || look_ahead() == '_' || is_digit(look_ahead())) {
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
    case   TOK_OPAREN: return "OPEN_PAREN";
    case  TOK_CPAREN: return "CLOSE_PAREN";
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

    case          TOK_AND: return "AND";
    case           TOK_OR: return "OR";
    
    case       TOK_NUMBER: return "NUMBER";
    case     TOK_VAR: return "VARIABLE";
    case      TOK_IF: return "IF";
    case      TOK_ELSE: return "ELSE";
    case      TOK_WHILE: return "WHILE";
    case      TOK_PRINT: return "PRINT";
    case TOK_TASK: return "PROC_NAME";
    case TOK_EXEC_TASK: return "EXEC_PROC";
    
    case      TOK_SEMICOLON: return "SEMICOLON";
        
    default:
        assert("Unreachable" && false);
        break;
    }

    return NULL;
}
