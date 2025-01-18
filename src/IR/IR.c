#include "frontend/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <IR/IR.h>
#include <IR/IR_optimize.h>
#include <backend/amd64/amd64.h>
#include <templates/vector.h>
#include <templates/graph.h>

#define ir_add(value) vector_ir_insn_add(ir, value)
#define symtable_add_fn() hashmap_ast_fn_vector_ir_var_add(fn_symtable, ir_current_fn, vector_ir_var_new())
#define symtable_add_var(var) vector_ir_var_add(hashmap_ast_fn_vector_ir_var_get(fn_symtable, ir_current_fn), var)

ir_vector* ir = 0;
ast_ctxt* ir_current_context = 0;
ast_fn* ir_current_fn = 0;
int fn_label = 0; // has a label been placed on the first line of the current function?
hashmap_ast_fn_vector_ir_var* fn_symtable = 0;

char* ir_output = 0;

void ir_stmt(ast_stmt* s);

// Create an ir_var to hold the result of a composite expression.
ir_var* ir_temp(type_info* type)
{
    static int n_temps = 0;
    ir_var* new = calloc(1, sizeof(ir_var));
    new->type = malloc(sizeof(type_info));
    if(type) memcpy(new->type, type, sizeof(type_info));
    else {
        new->type->base = LONG_T;
        new->type->ptr_layers = 0;
    }
    char buffer[1024] = {0};
    sprintf(buffer, ".t%d.l", n_temps++);
    new->name = strdup(buffer);

    return new;
}

// returns an ir_value of type literal with the given value
ir_value* ir_value_lit(long value)
{
    ir_value* v = malloc(sizeof(ir_value));
    v->type = IR_LIT;
    v->content.lit.i = value;

    return v;
}

ir_var* ir_dummy_var(void)
{
    ir_var* new = calloc(1, sizeof(ir_var));
    new->name = strdup("dummy");
    new->type = malloc(sizeof(type_info));
    new->type->base = INT_T;
    new->type->ptr_layers = 1; // have it be the same width as a pointer on any arch
    return new;
}

char* ir_autolabel(void)
{
    static int n_labels = 0;
    char buffer[1024] = {0};
    sprintf(buffer, "L.%d", n_labels++);
    
    return strdup(buffer);
}

ir_op convert_op(ast_op op)
{
    switch(op) {
        case O_PLUS: return IR_ADD;
        case O_MINUS: return IR_SUBTRACT;
        case O_TIMES: return IR_MULTIPLY;
        case O_BY: return IR_DIVIDE;
        case O_GREATER: return IR_GREATER;
        case O_LESSER: return IR_LESSER;
        case O_GREATER_EQUAL: return IR_GREATER_EQUAL;
        case O_LESSER_EQUAL: return IR_LESSER_EQUAL;
        case O_EQUIVALENT: return IR_EQUAL;
        case O_NOT_EQUIVALENT: return IR_NOT_EQUAL;
        case O_NEGATE: return IR_MINUS;
        case O_LOGICAL_AND: return IR_LOGICAL_AND;
        case O_LOGICAL_OR: return IR_LOGICAL_OR;
        case O_DEREFERENCE: return IR_DEREFERENCE;
        case O_REFERENCE: return IR_REFERENCE;

        default: return IR_NO_OP;
    }
}

// creates a variable to be used locally in the function to access its parameters
ir_var* ir_create_param(char* param_name)
{
    ir_var* param = malloc(sizeof(ir_var));
    char name[64];
    sprintf(name, "%s_%s.p", ir_current_fn->name, param_name);
    param->name = strdup(name);
    param->type = 0; // lol
    symtable_add_var(param);
    return param;
}

// Create an ir_value based on the passed AST expression.
ir_value* ir_create_value(ast_expr* e)
{
    ir_value* value = malloc(sizeof(ir_value));

    if(e->type == EXPR_LITERAL) {
        value->type = IR_LIT;
        value->content.lit.i = e->content.lit.content.number.content.ld;
    }

    else if(e->type == EXPR_VARIABLE) {
        if(e->content.var.value) {
            free(value);
            return ir_expr(e->content.var.value);
        }

        value->type = IR_VAR;
        value->content.var = malloc(sizeof(ir_var));
        char name[64];

        if(ir_current_fn->params && named_vector_ast_var_contains(ir_current_fn->params, &e->content.var))
            sprintf(name, "%s_%s.p", ir_current_fn->name, e->content.var.name);
        else if(e->content.var.def_ctxt == root->content.b.ctxt)
            sprintf(name, "%s.g", e->content.var.name);
        else sprintf(name, "%s.l", e->content.var.name);
        
        value->content.var->name = strdup(name);
        value->content.var->type = calloc(1, sizeof(type_info));
        if(e->content.var.type)
            memcpy(value->content.var->type, e->content.var.type, sizeof(type_info));
        else value->content.var->type->base = LONG_T;
    }

    else value->type = IR_NONE;
    return value;
}

ir_value* ir_unary(ast_expr* e)
{
    ir_insn* insn = calloc(1, sizeof(ir_insn));
    insn->type = IR_UN;

    ir_var* temp = ir_temp(e->content.un.type);
    ir_value* operand = ir_expr(e->content.un.e);
    insn->content.un.result = temp;
    insn->content.un.op = convert_op(e->content.un.op);
    insn->content.un.operand = operand;

    ir_add(insn);

    ir_value* result = calloc(1, sizeof(ir_value));
    result->type = IR_VAR;
    result->content.var = temp;
    return result;
}

ir_value* ir_binary(ast_expr* e)
{
    // special case: logical AND and OR operators - they short circuit
    if(e->content.bin.op == O_LOGICAL_AND || e->content.bin.op == O_LOGICAL_OR)
        return ir_short_circuit(e);

    ir_value* left = ir_expr(e->content.bin.left);
    ir_value* right = ir_expr(e->content.bin.right);
    ir_insn* insn = calloc(1, sizeof(ir_insn));
    insn->type = IR_BIN;

    ir_var* temp = ir_temp(e->content.bin.type);
    insn->content.bin.result = temp;
    insn->content.bin.left = left;
    insn->content.bin.op = convert_op(e->content.bin.op);
    insn->content.bin.right = right;
    ir_add(insn);

    ir_value* result = malloc(sizeof(ir_value));
    result->type = IR_VAR;
    result->content.var = temp;
    return result;
}

ir_value* ir_function_call(ast_expr* e)
{
    ir_insn* insn = calloc(1, sizeof(ir_insn));
    insn->type = IR_FN_CALL;

    ir_var* temp = ir_temp(e->content.call.fn->ret_type);

    char fn_name[1024] = {0};
    snprintf(fn_name, 1024, "fn.%s", e->content.call.fn->name);
    insn->content.fn_call.fn_label = strdup(fn_name);
    insn->content.fn_call.result = temp;
    insn->content.fn_call.args = vector_ir_value_new();
    insn->content.fn_call.ast_fn = e->content.call.fn;

    for(int i = 0; i < e->content.call.args->n_values; i++) {
        ast_expr* arg = e->content.call.args->values[i];
        ir_value* arg_value = ir_expr(arg);
        vector_ir_value_add(insn->content.fn_call.args, arg_value);
    }
    ir_add(insn);

    ir_value* result = malloc(sizeof(ir_value));
    result->type = IR_VAR;
    result->content.var = temp;
    return result;
}

void ir_procedure_call(ast_expr* e)
{
    ir_insn* insn = calloc(1, sizeof(ir_insn));
    insn->type = IR_PROC_CALL;

    char proc_name[1024] = {0};
    snprintf(proc_name, 1024, "fn.%s", e->content.call.fn->name);
    insn->content.proc_call.fn_label = strdup(proc_name);
    insn->content.proc_call.args = vector_ir_value_new();

    for(int i = 0; i < e->content.call.args->n_values; i++) {
        ast_expr* arg = e->content.call.args->values[i];
        ir_value* arg_value = ir_expr(arg);
        vector_ir_value_add(insn->content.proc_call.args, arg_value);
    }
    ir_add(insn);
}

void ir_deref_expr(ast_copy* copy)
{
    if(!copy->src) return;

    ir_insn* insn = calloc(1, sizeof(ir_insn));
    insn->type = IR_DEREF_ASSIGN;
    insn->content.deref_assign.dst = ir_expr(copy->dst->content.un.e)->content.var;
    insn->content.deref_assign.src = ir_expr(copy->src);
    
    ir_add(insn);
}

ir_value* ir_expr(ast_expr* e)
{
    if(!e) goto bad_args;

    switch(e->type) {
        case EXPR_LITERAL:
        case EXPR_VARIABLE:
        return ir_create_value(e);
        
        case EXPR_UNARY: return ir_unary(e);
        case EXPR_BINARY: return ir_binary(e);
        
        case EXPR_FN_CALL:
        if(e->content.call.fn->ret_type->base != VOID_T || e->content.call.fn->ret_type->ptr_layers)
            return ir_function_call(e);
        else { ir_procedure_call(e); return 0; }

        default: 
        printf("ir_expr: unknown type %d\n", e->type);
        ir_exit();
    }

    bad_args:
    printf("ir_expr: e is NULL\n");
    ir_exit();
}

void ir_block_stmt(ast_block* b)
{
    ir_current_context = b->ctxt;

    for(int i = 0; i < b->stmts->n_values; i++)
        ir_stmt(b->stmts->values[i]);

    ir_current_context = ir_current_context->parent;
}

void ir_stmt(ast_stmt* s)
{
    if(!s) return;

    switch(s->type) {
        case STMT_BLOCK:
        ir_block_stmt(&s->content.b);
        break;

        case STMT_COPY: {
            ast_copy* copy = &s->content.copy;
            ir_insn* insn = calloc(1, sizeof(ir_insn));
            insn->type = IR_COPY;

            ir_value* dst_value = ir_expr(copy->dst);
            ir_var* dst = malloc(sizeof(ir_var));
            memcpy(dst, dst_value->content.var, sizeof(ir_var));
            free(dst_value);
            insn->content.copy.dst = dst;
            insn->content.copy.src = ir_expr(copy->src);
            
            ir_add(insn);
        }
        break;
        
        case STMT_DECL: {
            // dst should contain an empty IR variable
            // and src should contain the actual value
            ir_insn* insn = calloc(1, sizeof(ir_insn));
            insn->type = IR_COPY;
            insn->content.copy.src = ir_expr(s->content.expr);
            insn->content.copy.dst = malloc(sizeof(ir_var));

            char name[64] = {0};
            strcpy(name, s->content.copy.dst->content.var.name);
            if(ir_current_context && ir_current_context->parent) strcat(name, ".l");
            else strcat(name, ".g");

            insn->content.copy.dst->name = strdup(name);
            insn->content.copy.dst->type = malloc(sizeof(type_info));
            memcpy(insn->content.copy.dst->type, s->content.copy.dst->content.var.type, sizeof(type_info));
            ir_add(insn);
        }
        break;

        case STMT_FUNCTION:
        if(!s->content.fn.body) break; // for forward declarations

        ir_insn* label_op = malloc(sizeof(ir_insn));
        label_op->type = IR_NOP;
        char fn_name[1024] = {0};
        snprintf(fn_name, 1024, "fn.%s", s->content.fn.name);
        label_op->label = strdup(fn_name);
        ir_add(label_op);
        ir_block_stmt(s->content.fn.body);
        break;

        case STMT_EXPR:
        ir_expr(s->content.expr);
        break;

        case STMT_DEREF:
        ir_deref_expr(&s->content.copy);
        break;

        case STMT_IF: {

            /*
            if(expr) if_true
            else if_false

            should translate into

            condjmp(expr, label_true)
            if_false stmts
            ...
            goto label_after
            label_true:
            if_true stmts
            ...
            label_after:
            ... other code
            */


            ast_if* if_stmt = &s->content.if_stmt;
            ir_insn* insn = calloc(1, sizeof(ir_insn));
            insn->type = IR_IF;
            insn->content.condjmp.cond = ir_expr(if_stmt->cond);
            insn->content.condjmp.if_true = ir_autolabel();
            ir_add(insn); // condjmp(expr, label_true)

            ir_stmt(if_stmt->if_false); // works even if it's null
            ir_insn* goto_after = calloc(1, sizeof(ir_insn));
            goto_after->type = IR_GOTO;
            goto_after->content.jmp.dst = ir_autolabel();
            ir_add(goto_after);

            // if_true handling
            // first make a label, then generate the IR code
            ir_insn* if_true_op = malloc(sizeof(ir_insn));
            if_true_op->type = IR_NOP;
            if_true_op->label = insn->content.condjmp.if_true;
            ir_add(if_true_op);
            ir_stmt(if_stmt->if_true);

            // make the nop for goto_after
            ir_insn* goto_after_op = malloc(sizeof(ir_insn));
            goto_after_op->type = IR_NOP;
            goto_after_op->label = goto_after->content.jmp.dst;
            ir_add(goto_after_op);
        }
        break;

        case STMT_WHILE: {
            // make a label for the loop
            // parse condition
            // condjmp to after the loop if the condition doesn't hold
            // loop code
            // goto loop label
            // label for the condjmp

            // make a nop to hold the loop label
            ir_insn* loop_label = malloc(sizeof(ir_insn));
            loop_label->type = IR_NOP;
            loop_label->label = ir_autolabel();
            ir_add(loop_label);

            // make an instruction to make a negation of the while condition
            ast_while* while_stmt = &s->content.while_stmt;
            ir_insn* cond_negate = calloc(1, sizeof(ir_insn));
            cond_negate->type = IR_UN;
            cond_negate->content.un.type = 0;
            cond_negate->content.un.operand = ir_expr(while_stmt->cond);
            cond_negate->content.un.result = ir_temp(0);
            cond_negate->content.un.op = IR_LOGICAL_NOT;
            ir_add(cond_negate);

            // make an instruction to evaluate the condition
            ir_insn* cond_check = calloc(1, sizeof(ir_insn));
            cond_check->type = IR_IF;
            // have to pack it like this because condjmp.cond expects an ir_value*, nor ir_var*
            ir_value* result_value = malloc(sizeof(ir_value));
            result_value->type = IR_VAR;
            result_value->content.var = cond_negate->content.un.result;
            cond_check->content.condjmp.cond = result_value;
            cond_check->content.condjmp.if_true = ir_autolabel(); // loop exit
            ir_add(cond_check);

            // now parse the loop body
            ir_stmt(while_stmt->body);

            // then an unconditional jump back to the loop
            ir_insn* jmp = calloc(1, sizeof(ir_insn));
            jmp->type = IR_GOTO;
            jmp->content.jmp.dst = loop_label->label;
            ir_add(jmp);

            // and finally the loop end label
            ir_insn* loop_end_label = malloc(sizeof(ir_insn));
            loop_end_label->type = IR_NOP;
            loop_end_label->label = cond_check->content.condjmp.if_true;
            ir_add(loop_end_label);
        }
        break;

        case STMT_UNTIL:
        case STMT_FOR:
        break; // cba;

        case STMT_RETURN: {
            ir_insn* insn = calloc(1, sizeof(ir_insn));
            insn->type = IR_RETURN;
            insn->content.ret.fn = strdup(ir_current_fn->name);
            if(s->content.ret.var)
                insn->content.ret.value = ir_expr(s->content.ret.var);
            insn->content.ret.is_last = s->content.ret.is_last;
            ir_add(insn);
        }
        break;

        case STMT_NONE: return;

        default:
        printf("ir_stmt: unknown stmt\n");
        ir_exit();
    }
}

void ir_init(void)
{
    ir = vector_ir_insn_new();

    // first cover global variables
    for(int i = 0; i < root->content.b.stmts->n_values; i++) {
        ast_stmt* s = root->content.b.stmts->values[i];
        if(s->type == STMT_DECL) ir_stmt(s); // only cover declarations
    }

    // and then each function
    fn_symtable = hashmap_ast_fn_vector_ir_var_new();

    for(int i = 0; i < program->n_values; i++) {
        ast_fn* fn = program->values[i];
        ir_current_fn = fn;
        // save this function's parameters in the symtable
        if(fn->params) {
            symtable_add_fn();
            for(int param_i = 0; param_i < fn->params->n_values; param_i++) {
                ir_create_param(fn->params->values[param_i]->name);
            }
        }
        
        // a bit of glue to abstract the fn into a stmt
        ast_stmt* s = malloc(sizeof(ast_stmt));
        s->type = STMT_FUNCTION;
        memcpy(&s->content.fn, fn, sizeof(ast_fn));
        ir_stmt(s);
        free(s);
    }

    // optimization passes go here
    ir_remove_redundant_assignments();
}