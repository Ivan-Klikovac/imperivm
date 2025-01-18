#ifndef _IMPERIVM_FRONTEND_LEXER_H
#define _IMPERIVM_FRONTEND_LEXER_H

typedef enum {
    // single character
    LPAREN, RPAREN, LANGLED, RANGLED, LBRACE, RBRACE, DOT, COMMA, SEMICOLON, QMARK, COLON,

    // one or two characters
    EQUAL, EQUAL_EQUAL, GREATER, GREATER_GREATER, GREATER_EQUAL, LESSER, LESSER_LESSER, LESSER_EQUAL,
    AND, AND_AND, OR, OR_OR, NOT, NOT_EQUAL, MINUS, MINUS_MINUS, MINUS_EQUAL, PERCENT, PERCENT_EQUAL,
    PLUS, PLUS_PLUS, PLUS_EQUAL, SLASH, SLASH_EQUAL, STAR, STAR_EQUAL, XOR, XOR_EQUAL,

    // literals
    IDENTIFIER, NUMBER, STRING, CHAR_LIT,

    // keywords
    IF, ELSE, DO, WHILE, GOTO, RETURN, CHAR, INT, SIGNED, UNSIGNED, LONG, VOID, END
} token_type;

typedef struct {
    int line;
    token_type type;
    char lexeme[32];
} token;

extern token* tokens;

void run(char*);

#endif