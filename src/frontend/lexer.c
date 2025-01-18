#include <imperivm.h>
#include <frontend/lexer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

token* tokens;

void token_new(token* new, int line, token_type type, char* lexeme, int length)
{
    new->line = line;
    new->type = type;
    strncpy(new->lexeme, lexeme, length);
}

void print_tokens(void)
{
    for(int i = 0; !(tokens[i].type == END && tokens[i].lexeme[0] == ' '); i++) {
        printf("%s ", tokens[i].lexeme);
    }
    printf("\n");
}

void run(char* src)
{
    tokens = malloc(strlen(src) * sizeof(token)); // guaranteed to be enough
    memset(tokens, 0, strlen(src) * sizeof(token)); // placating valgrind
    char* start = src; // start of a given lexeme (not constantly updated)
    char* current = src; // the current character being considered
    int line = 1; // basically counts '\n's lol
    int n = 0; // total tokens so far

    while(*current) {
        switch(*current) {
            case '\n': line++; current++; break;
            case '\t': current++; break;
            case ' ': current++; break;

            // single character tokens
            case '(': token_new(&tokens[n++], line, LPAREN, current++, 1); break;
            case ')': token_new(&tokens[n++], line, RPAREN, current++, 1); break;
            case '[': token_new(&tokens[n++], line, LANGLED, current++, 1); break;
            case ']': token_new(&tokens[n++], line, RANGLED, current++, 1); break;
            case '{': token_new(&tokens[n++], line, LBRACE, current++, 1); break;
            case '}': token_new(&tokens[n++], line, RBRACE, current++, 1); break;
            case '.': token_new(&tokens[n++], line, DOT, current++, 1); break;
            case ',': token_new(&tokens[n++], line, COMMA, current++, 1); break;
            case '?': token_new(&tokens[n++], line, QMARK, current++, 1); break;
            case ';': token_new(&tokens[n++], line, SEMICOLON, current++, 1); break;
            case ':': token_new(&tokens[n++], line, COLON, current++, 1); break;

            // one or two character tokens
            case '=': 
            if(current[1] == '=') { 
                token_new(&tokens[n++], line, EQUAL_EQUAL, current, 2);
                current += 2;
            }
            else token_new(&tokens[n++], line, EQUAL, current++, 1);
            break;

            case '>': 
            if(current[1] == '=') {
                token_new(&tokens[n++], line, GREATER_EQUAL, current, 2);
                current += 2;
            }
            else if(current[1] == '>') {
                token_new(&tokens[n++], line, GREATER_GREATER, current, 2);
                current += 2;
            }
            else token_new(&tokens[n++], line, GREATER, current++, 1);
            break;

            case '<':
            if(current[1] == '=') {
                token_new(&tokens[n++], line, LESSER_EQUAL, current, 2);
                current += 2;
            }
            else if(current[1] == '<') {
                token_new(&tokens[n++], line, LESSER_LESSER, current, 2);
                current += 2;
            }
            else token_new(&tokens[n++], line, LESSER, current++, 1);
            break;

            case '&':
            if(current[1] == '&') {
                token_new(&tokens[n++], line, AND_AND, current, 2);
                current += 2;
            }
            else token_new(&tokens[n++], line, AND, current++, 1);
            break;

            case '|':
            if(current[1] == '|') {
                token_new(&tokens[n++], line, OR_OR, current, 2);
                current += 2;
            }
            else token_new(&tokens[n++], line, OR, current++, 1);
            break;

            case '!':
            if(current[1] == '=') {
                token_new(&tokens[n++], line, NOT_EQUAL, current, 2);
                current += 2;
            }
            else token_new(&tokens[n++], line, NOT, current++, 1);
            break;

            case '-':
            if(current[1] == '-') {
                token_new(&tokens[n++], line, MINUS_MINUS, current, 2);
                current += 2;
            }
            else if(current[1] == '=') {
                token_new(&tokens[n++], line, MINUS_EQUAL, current, 2);
                current += 2;
            }
            else token_new(&tokens[n++], line, MINUS, current++, 1);
            break;

            case '+':
            if(current[1] == '+') {
                token_new(&tokens[n++], line, PLUS_PLUS, current, 2);
                current += 2;
            }
            else if(current[1] == '=') {
                token_new(&tokens[n++], line, PLUS_EQUAL, current, 2);
                current += 2;
            }
            else token_new(&tokens[n++], line, PLUS, current++, 1);
            break;

            case '/':
            if(current[1] == '=') {
                token_new(&tokens[n++], line, SLASH_EQUAL, current, 2);
                current += 2;
            }
            else if(current[1] == '/') {
                // comment, ignore until newline
                while(*current != '\n') current++;
            }
            else token_new(&tokens[n++], line, SLASH, current++, 1);
            break;

            case '*': // handle pointers to pointers here at some point (pun not intended)
            if(current[1] == '=') {
                token_new(&tokens[n++], line, STAR_EQUAL, current, 2);
                current += 2;
            }
            else token_new(&tokens[n++], line, STAR, current++, 1);
            break;

            case '%':
            if(current[1] == '=') {
                token_new(&tokens[n++], line, PERCENT_EQUAL, current, 2);
                current += 2;
            }
            else token_new(&tokens[n++], line, PERCENT, current++, 1);
            break;

            case '^':
            if(current[1] == '=') {
                token_new(&tokens[n++], line, XOR_EQUAL, current, 2);
                current += 2;
            }
            else token_new(&tokens[n++], line, XOR, current++, 1);
            break;

            case '\'': // character literal
            // find the other apostrophe, it won't necessarily be right after the character
            start = current++; // save current and increment it for the loop condition
            while(*current && *current != '\'') current++;
            if(!(*current)) { report(line, start, "Incomplete character literal"); continue; }
            // start[1] != \ allows for things like \n, \t etc
            if(current - start != 2 && start[1] != '\\') report(line, start, "Invalid character literal");
            else token_new(&tokens[n++], line, CHAR_LIT, start, (current - start) + 1); // valid, save it into a token
            current++; // either way, go on
            break;

            case '"': // string literal
            // find the end of the string
            start = current++;
            while(*current && *current != '"') current++;
            if(!(*current)) { report(line, start, "Incomplete string literal"); continue; }
            token_new(&tokens[n++], line, STRING, start, (current - start) + 1);
            current++;
            break;

            default:
            // number literals, identifiers and keywords here
            if(isdigit(*current)) {
                // check if it's a number literal
                // so just digits (and up to one dot) until a ), ;, *, /, +, - or whitespace
                // anything else throws an error
                start = current++;
                int dot = 0;
                while(*current && (isdigit(*current) || ((*current == '.') && !dot))) {
                    if(*current == '.') dot = 1;
                    current++;
                }
                if(!(*current)) { report(line, start, "Missing code after number literal"); continue; }
                if(*current == '.') report(line, start, "Extra decimal point in number literal");
                else if(*current != ')' && *current != ';' && *current != '*' && *current != '/' 
                     && *current != '+' && *current != '-' && *current != ']' && *current != ' ')
                    report(line, start, "Only number literals can start with a digit");
                // valid number literal, save it in a token
                else token_new(&tokens[n++], line, NUMBER, start, current - start);
            }

            else if(strncmp(current, "if", 2) == 0) {
                // this is not necessarily a keyword
                // it could be perhaps an identifier, something like
                // if_passed or some other such thing
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                if(current - start == 2) token_new(&tokens[n++], line, IF, start, 2); // it's just an if
                else token_new(&tokens[n++], line, IDENTIFIER, start, (current - start) + 1);
            }

            else if(strncmp(current, "else", 4) == 0) {
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                if(current - start == 4) token_new(&tokens[n++], line, ELSE, start, 4);
                else token_new(&tokens[n++], line, IDENTIFIER, start, (current - start) + 1);
            }

            else if(strncmp(current, "do", 2) == 0) {
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                if(current - start == 2) token_new(&tokens[n++], line, DO, start, 2);
                else token_new(&tokens[n++], line, IDENTIFIER, start, (current - start) + 1);
            }

            else if(strncmp(current, "while", 5) == 0) {
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                if(current - start == 5) token_new(&tokens[n++], line, WHILE, start, 5);
                else token_new(&tokens[n++], line, IDENTIFIER, start, (current - start) + 1);
            }
            
            else if(strncmp(current, "goto", 4) == 0) {
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                if(current - start == 4) token_new(&tokens[n++], line, GOTO, start, 4);
                else token_new(&tokens[n++], line, IDENTIFIER, start, (current - start) + 1);
            }

            else if(strncmp(current, "return", 6) == 0) {
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                if(current - start == 6) token_new(&tokens[n++], line, RETURN, start, 6);
                else token_new(&tokens[n++], line, IDENTIFIER, start, (current - start) + 1);
            }

            // I CBA TO SUPPORT ANYTHING BUT 64-BIT INTEGERS and maybe floats
            // maybe later

            /*else if(strncmp(current, "i8", 2) == 0) {
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                if(current - start == 2) token_new(&tokens[n++], line, I8, start, 2);
                else token_new(&tokens[n++], line, IDENTIFIER, start, (current - start) + 1);
            }

            else if(strncmp(current, "i16", 3) == 0) {
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                if(current - start == 3) token_new(&tokens[n++], line, I16, start, 3);
                else token_new(&tokens[n++], line, IDENTIFIER, start, (current - start) + 1);     
            }

            else if(strncmp(current, "i32", 3) == 0) {
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                if(current - start == 3) token_new(&tokens[n++], line, I32, start, 3);
                else token_new(&tokens[n++], line, IDENTIFIER, start, (current - start) + 1);         
            }*/

            else if(strncmp(current, "int", 3) == 0) {
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                if(current - start == 3) token_new(&tokens[n++], line, INT, start, 3);
                else token_new(&tokens[n++], line, IDENTIFIER, start, (current - start) + 1);
            }

            else if(strncmp(current, "long", 4) == 0) {
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                if(current - start == 4) token_new(&tokens[n++], line, LONG, start, 4);
                else token_new(&tokens[n++], line, IDENTIFIER, start, (current - start) + 1);
            }

            else if(strncmp(current, "signed", 6) == 0) {
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                if(current - start == 6) token_new(&tokens[n++], line, SIGNED, start, 6);
                else token_new(&tokens[n++], line, IDENTIFIER, start, (current - start) + 1);
            }


            else if(strncmp(current, "unsigned", 8) == 0) {
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                if(current - start == 8) token_new(&tokens[n++], line, UNSIGNED, start, 8);
                else token_new(&tokens[n++], line, IDENTIFIER, start, (current - start) + 1);
            }

            else if(strncmp(current, "char", 4) == 0) {
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                if(current - start == 4) token_new(&tokens[n++], line, CHAR, start, 4);
                else token_new(&tokens[n++], line, IDENTIFIER, start, (current - start) + 1);            
            }

            else if(strncmp(current, "void", 4) == 0) {
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                if(current - start == 4) token_new(&tokens[n++], line, VOID, start, 4);
                else token_new(&tokens[n++], line, IDENTIFIER, start, (current - start) + 1);            
            }

            /*else if(strncmp(current, "EOF", 3) == 0) {
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                if(current - start == 3) token_new(&tokens[n++], line, T_EOF, start, 3);
                else token_new(&tokens[n++], line, IDENTIFIER, start, (current - start) + 1);            
            }*/

            else {
                // it's an identifier
                start = current;
                while(*current && (isalnum(*current) || *current == '-' || *current == '_')) current++;
                if(!(*current)) { report(line, start, "Missing code"); continue; }
                token_new(&tokens[n++], line, IDENTIFIER, start, current - start);
            }
        }
    }

    token_new(&tokens[n], line, END, " ", 1);
    //print_tokens();
}