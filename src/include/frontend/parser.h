#ifndef _IMPERIVM_FRONTEND_PARSER_H
#define _IMPERIVM_FRONTEND_PARSER_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <templates/vector.h>

// placating the ancient compiler
typedef struct ast_stmt ast_stmt;
typedef struct ast_expr ast_expr;
typedef struct ast_ctxt ast_ctxt;
typedef struct ast_var ast_var;
typedef struct ast_ret ast_ret;
typedef struct ast_fn ast_fn;

ptr_vector(ast_stmt);
ptr_vector(ast_expr);
ptr_vector(ast_ret);
ptr_vector(ast_var);
ptr_vector(ast_fn);


typedef enum {
    VOID_T,
    CHAR_T,
    UCHAR_T,
    INT_T,
    UINT_T,
    LONG_T,
    ULONG_T
} base_type;

typedef struct {
    base_type base;
    uint64_t ptr_layers;
} type_info;

typedef enum {
    O_NOP, O_CAST, O_NOT, O_NEGATE, O_INCREMENT, O_DECREMENT, O_REFERENCE, O_DEREFERENCE, // unary ones
    O_PLUS, O_MINUS, O_TIMES, O_BY, O_GREATER, O_LESSER, O_GREATER_EQUAL, O_LESSER_EQUAL, 
    O_EQUIVALENT, O_NOT_EQUIVALENT, O_ASSIGN, O_ADD_ASSIGN, O_SUBTRACT_ASSIGN, O_MULTIPLY_ASSIGN, 
    O_DIVIDE_ASSIGN, O_BINARY_AND, O_LOGICAL_AND, O_BINARY_OR, O_LOGICAL_OR, O_MOD, O_XOR,
    O_RSHIFT, O_LSHIFT
} ast_op;

typedef struct {
    union {
        struct {
            union {
                char c;
                int d;
                long ld;
            } content;
            enum {
                N_CHAR,
                N_INT,
                N_LONG
            } type;
        } number;
        char* string;
    } content;
    enum {
        LIT_NUMBER,
        LIT_STRING,
        LIT_CHAR
    } type;
} ast_lit; // literals

typedef struct ast_var {
    type_info* type;
    char* name;
    ast_expr* value;
    ast_ctxt* def_ctxt; // the context in which this var was defined
} ast_var; // variables

typedef struct {
    ast_op op;
    ast_expr* e;
    type_info* type;
} ast_un; // unary expressions

typedef struct {
    ast_expr* left;
    ast_op op;
    ast_expr* right;
    type_info* type;
} ast_bin; // binary expressions

typedef struct {
    ast_fn* fn;
    vector_ast_expr* args;
} ast_fn_call;

typedef struct ast_expr {
    union {
        ast_lit lit;
        ast_var var;
        ast_un un;
        ast_bin bin;
        ast_fn_call call;
    } content;
    enum {
        EXPR_NONE,
        EXPR_LITERAL,
        EXPR_VARIABLE,
        EXPR_UNARY,
        EXPR_BINARY,
        EXPR_FN_CALL
    } type; // this tells me which type is each expression instance
} ast_expr;

typedef struct ast_ret {
    ast_expr* var;
    ast_fn* fn;
    int is_last; // is this the last return statement in its function?
} ast_ret;

// context structure, holds variables in the current context
// basically a tree, but parent nodes don't have ptrs to children
// children have a ptr to parent
// the root will be the whole source file, containing global variables

typedef struct ast_ctxt {
    ast_ctxt* parent;
    vector_ast_var* vars;
} ast_ctxt;

typedef struct ast_block {
    vector_ast_stmt* stmts;
    ast_ctxt* ctxt; // the thing heretics don't read
    ast_fn* fn; // which function this block is in
} ast_block;

typedef struct ast_fn {
    type_info* ret_type;
    char* name;
    ast_block* body;
    ast_ctxt* ctxt; // this ptr here points to the same context as the function body context ptr
    vector_ast_var* params; // contains the function parameters
    vector_ast_ret* rets; // contains all the return statements
} ast_fn;

typedef struct {
    ast_expr* cond; // condition
    struct ast_stmt* if_true; // either a single stmt or a block
    struct ast_stmt* if_false; // same but can be null
} ast_if;

typedef struct {
    ast_expr* cond;
    ast_stmt* body;
} ast_while;

typedef struct {
    ast_expr* cond;
    ast_stmt* body;
} ast_until;

typedef struct {
    ast_stmt* start_stmt;
    ast_expr* cond;
    ast_stmt* end_stmt;
    ast_stmt* body;
} ast_for;

typedef struct {
    ast_expr* dst; // this can be any kind of expr, so long as it's an lvalue
    ast_expr* src;
} ast_copy; // standard assignment, dst = src

typedef struct ast_stmt {
    union {
        ast_ret ret;
        ast_copy copy; // this covers assignments
        ast_expr* expr; // an expression statement that exists for its side effects
        ast_block b;
        ast_fn fn;
        ast_if if_stmt;
        ast_while while_stmt;
        ast_until until_stmt;
        ast_for for_stmt;
    } content;
    enum {
        STMT_NONE,
        STMT_RETURN,
        STMT_DECL, // both STMT_DECL and STMT_COPY are covered by ast_copy
        STMT_COPY,
        STMT_EXPR,
        STMT_BLOCK,
        STMT_FUNCTION,
        STMT_IF,
        STMT_WHILE,
        STMT_UNTIL,
        STMT_FOR,
        STMT_DEREF // same union member as copy, disambiguated in the IR, lhs begins with *
    } type;
} ast_stmt;

extern vector_ast_fn* program; // contains all the functions in the parsed program
extern ast_stmt* root; // AST root, a block statement with global scope

void parse(void);
ast_ctxt* var_def_ctxt(char* var_name, ast_ctxt* current_context);

#endif