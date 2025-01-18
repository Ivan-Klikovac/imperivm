#ifndef _IMPERIVM_IR_H
#define _IMPERIVM_IR_H

#include <stdlib.h>
#include <string.h>
#include <imperivm.h>
#include <frontend/parser.h>
#include <templates/vector.h>
#include <templates/graph.h>
#include <templates/hashmap.h>
#include <templates/named_vector.h>

// done vaguely according to chapter 6 of dragon book

// a stmt like
// result = -a + b * c;
// would be translated into an IR like
// t1 = b * c;
// t2 = -a;
// t3 = t1 + t2;
// result = t3;

typedef struct ir_value ir_value;
typedef struct ir_insn ir_insn;
typedef struct ir_var ir_var;

ptr_vector(ir_value);
ptr_vector(ir_insn);
ptr_vector(ir_var);

typedef enum {
    // reserved
    IR_NO_OP,
    // unary
    IR_MINUS,
    IR_LOGICAL_NOT,
    IR_BINARY_NOT,
    IR_LOGICAL_OR,
    IR_BINARY_OR,
    IR_LOGICAL_AND,
    IR_BINARY_AND,
    IR_CAST,
    IR_DEREFERENCE,
    IR_REFERENCE,
    // binary
    IR_ADD,
    IR_SUBTRACT,
    IR_MULTIPLY,
    IR_DIVIDE,
    IR_LESSER,
    IR_GREATER,
    IR_LESSER_EQUAL,
    IR_GREATER_EQUAL,
    IR_EQUAL,
    IR_NOT_EQUAL
} ir_op;

typedef union {
    // for now
    int64_t i;
} ir_lit;

typedef struct ir_var {
    char* name;
    type_info* type;
} ir_var;

typedef struct ir_value {
    union {
        ir_lit lit;
        ir_var* var;
    } content;
    enum {
        IR_NONE, // for easier debugging
        IR_LIT,
        IR_VAR
    } type;
} ir_value;

typedef struct {
    ir_var* result; // don't allow literals on lhs
    ir_op op;
    ir_value* operand;
    type_info* type;
} ir_un;

typedef struct {
    ir_var* result;
    ir_value* left;
    ir_op op;
    ir_value* right;
    type_info* type;
} ir_bin;

typedef struct {
    ir_var* dst;
    ir_value* src;
} ir_copy;

typedef struct {
    char* dst;
} ir_goto;

typedef struct {
    ir_value* cond;
    char* if_true;
    char* if_false; // basically `else`, can be null
} ir_if;

typedef struct {
    char* fn_label;
    vector_ir_value* args;
    ir_var* result;
    ast_fn* ast_fn;
} ir_fn_call;

typedef struct {
    char* fn_label;
    vector_ir_value* args;
} ir_proc_call;

typedef struct {
    char* fn; // the function/procedure this return call belongs to
    ir_value* value; // null in case of a procedure
    int is_last; // set to 1 if this is the last return statement in its function
} ir_return;

struct ir_ptr_stuff {
    ir_var* dst;
    ir_value* src;
};

typedef struct ir_ptr_stuff ir_assign_ref; // dst = &src
typedef struct ir_ptr_stuff ir_assign_deref; // dst = *src
typedef struct ir_ptr_stuff ir_deref_assign; // *dst = src

typedef struct ir_insn {
    union {
        ir_un un;
        ir_bin bin;
        ir_copy copy;
        ir_goto jmp;
        ir_if condjmp;
        ir_fn_call fn_call;
        ir_proc_call proc_call;
        ir_return ret;
        ir_assign_ref assign_ref;
        ir_assign_deref assign_deref;
        ir_deref_assign deref_assign;
    } content;
    enum {
        IR_NOP,
        IR_UN, // x = op y
        IR_BIN, // x = y op z
        IR_COPY, // x = y
        IR_GOTO, // goto label
        IR_IF, // if x goto label (optionally else)
        IR_FN_CALL, // can return a value
        IR_PROC_CALL, // does not return anything
        IR_RETURN, // can be with or without a return value
        IR_ASSIGN_REF, // x = &y
        IR_ASSIGN_DEREF, // x = *y
        IR_DEREF_ASSIGN, // *x = y
    } type;
    char* label; // usually null, only set in case there's a label on that line
} ir_insn;

// just don't think about this too hard
#define ir_vector vector_ir_insn
#define var_vector vector_ir_var
#define var_vector_remove vector_ir_var_remove
#define var_vector_contains vector_ir_var_contains
#define var_graph graph_ir_var
value_vector(int);
value_vector(uint32_t);
named_vector(ast_var);
graph(ir_var);
hashmap(ast_fn, vector_ir_var);

extern vector_ir_insn* ir;
extern char* ir_output;
extern int verbose_asm;
extern int print_blocks;
extern hashmap_ast_fn_vector_ir_var* fn_symtable; // holds all parameters for each function

ir_value* ir_expr(ast_expr* e);
ir_var* ir_dummy_var(void);
ir_var* ir_temp(type_info*);
ir_value* ir_value_lit(long);
char* ir_autolabel(void);
var_vector* ir_get_vars(int start, int end);
var_graph* ir_get_interference_graph(var_vector* vars, int start, int end);

#endif