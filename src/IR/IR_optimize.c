#include "frontend/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <IR/IR.h>
#include <IR/IR_print.h>
#include <IR/IR_optimize.h>
#include <templates/vector.h>
#include <templates/graph.h>
#include <templates/set.h>

type_set(ir_insn);
vector_set(ir_var);

#define lifetime_start(value) vector_uint32_t_add(life_start, value)
#define lifetime_end(value) vector_uint32_t_add(life_end, value)
#define ir_add(insn) vector_ir_insn_add(ir, insn)

var_vector* global_vars = 0;

// returns the index at which the actual code starts
// don't want to send global variables into the backend as instructions
int ir_code_start(void)
{
    global_vars = vector_ir_var_new();
    char c[64] = {0};
    int i = 0;
    while(ir->values[i]->type == IR_COPY) {
        if(ir_out) {
            FILE* ir_f = fopen(ir_out, "a");
            ir_print_instr(ir->values[i], c);
            fprintf(ir_f, "%s", c);
            memset(c, 0, 64);
            fclose(ir_f);
        }
        vector_ir_var_add(global_vars, ir->values[i]->content.copy.dst);
        i++;
    }

    return i;
}

// goes through the IR basic block by basic block
// returns 0 only if there is no more code to be processed, and nonzero otherwise
// and sets start and end to the boundaries of the found block
int ir_get_block(int* start, int* end)
{
    static int i = 0;
    if(i == 0) i = ir_code_start();

    if(i >= ir->n_values) return 0;
    *start = i;

    for(ir_insn* insn = ir->values[i]; !ir_insn_is(insn, 5, 
    IR_IF, IR_GOTO, IR_FN_CALL, IR_PROC_CALL, IR_RETURN) && !insn->label; insn = ir->values[++i]);

    *end = ++i; // include the jump instruction and increment the index past it for the next call
    return 1;
}

#define value_add(value) if(value->type == IR_VAR) vector_set_ir_var_add(vars, value->content.var);
#define var_add(var) vector_set_ir_var_add(vars, var);

// this function returns a vector with all variables in the given block
var_vector* ir_get_vars(int start, int end)
{
    var_vector* vars = vector_ir_var_new();

    for(int i = start; i < end; i++) {
        ir_insn* insn = ir->values[i];
        switch(insn->type) {
            case IR_UN:
            value_add(insn->content.un.operand);
            var_add(insn->content.un.result);
            break;

            case IR_BIN:
            value_add(insn->content.bin.left);
            value_add(insn->content.bin.right);
            var_add(insn->content.bin.result);
            break;

            case IR_COPY:
            value_add(insn->content.copy.src); 
            var_add(insn->content.copy.dst);
            break;

            case IR_IF:
            value_add(insn->content.condjmp.cond);
            break;

            case IR_FN_CALL:
            var_add(insn->content.fn_call.result);
            break;

            case IR_RETURN:
            if(insn->content.ret.value)
                value_add(insn->content.ret.value);
            break;

            case IR_ASSIGN_REF:
            value_add(insn->content.assign_ref.src);
            var_add(insn->content.assign_ref.dst);
            break;

            case IR_ASSIGN_DEREF:
            value_add(insn->content.assign_deref.src);
            var_add(insn->content.assign_deref.dst);
            break;

            case IR_DEREF_ASSIGN:
            value_add(insn->content.deref_assign.src);
            var_add(insn->content.deref_assign.dst);
            break;

            default: break;
        }
    }

    // prune the vector now
    // for some reason which I cba to determine,
    // the var vector gets filled with copies and some trash
    // like a var whose ptr is 0x2????
    char* names[vars->n_values];
    int n_values = vars->n_values;
    for(int i = 0; i < vars->n_values; i++) names[i] = calloc(1, 32);
    for(int i = 0; i < vars->n_values; i++) {
        if(vars->values[i] < (ir_var*) 0x100) {
            var_vector_remove(vars, i);
            i--; // so that it doesn't skip over a value
            continue;
        }
        for(int j = 0; j < n_values; j++) 
            if(names[j] && strcmp(vars->values[i]->name, names[j]) == 0) {
                var_vector_remove(vars, i);
                i--;
                goto next; // basically `continue` on the outer loop
            }
        strcpy(names[i], vars->values[i]->name);
        next:;
    }
    for(int i = 0; i < n_values; i++) free(names[i]);

    return vars;
}
#undef value_add
#undef var_add

int ir_find_var(var_vector* v, ir_var* var)
{
    for(int i = 0; i < v->n_values; i++) if(strcmp(var->name, v->values[i]->name) == 0) return i;
    assert(1);
    return -1;
}

int ir_find_maybe_var(var_vector* v, ir_value* value)
{
    if(value->type != IR_VAR) return -1;
    return ir_find_var(v, value->content.var);
}

var_vector* ir_get_local_vars(char* fn)
{
    int fn_start = 0;
    int fn_end = 0;
    char fn_name[64];
    sprintf(fn_name, "fn.%s", fn);

    for(int i = 0; i < ir->n_values; i++) {
        if(ir->values[i]->label && strcmp(ir->values[i]->label, fn_name) == 0) {
            fn_start = i;
            break;
        }
    }
    for(int i = fn_start + 1; i < ir->n_values; i++) {
        if(ir->values[i]->label && strncmp(ir->values[i]->label, "fn.", 3) == 0) {
            fn_end = i-1; // found the start of another function
            break;
        }
    }
    if(!fn_end) fn_end = ir->n_values - 1; // this happens if it's the last function

    var_vector* fn_vars = ir_get_vars(fn_start, fn_end);

    // remove all global variables from the vector
    for(int i = 0; i < fn_vars->n_values; i++) {
        if(fn_vars->values[i]->name[strlen(fn_vars->values[i]->name)-1] == 'g') {
            vector_ir_var_remove(fn_vars, i);
            i--;
        }
    }
    // and all function parameters as well
    for(int i = 0; i < fn_vars->n_values; i++) {
        if(fn_vars->values[i]->name[strlen(fn_vars->values[i]->name)-1] == 'p') {
            vector_ir_var_remove(fn_vars, i);
            i--;
        }
    }

    return fn_vars; // stack size in bytes
}

ast_fn* ir_get_ast_fn(char* fn_name)
{
    for(int i = 0; i < program->n_values; i++) {
        ast_fn* fn = program->values[i];
        if(strcmp(fn->name, fn_name+3) == 0) return fn;
    }
    printf("ir_get_ast_fn: %s not found\n", fn_name);
    return 0;
}

// this function removes unused assignment instrs on used vars
void ir_block_remove_unused_assignments(int start, int end)
{
    // for each var, check if all its value assignments are used and remove ones that aren't
    // except if it's the last, then it may be used outside the block being considered
    // this optimization is not that important rn

    /*for(int i = start; i < end; i++) {
        ir_instr* instr = ir->values[i];
        if(ir_instr_is(instr, 3, IR_UN, IR_BIN, IR_COPY, IR_FN_CALL))
    }*/
}

// this function moves the instruction at ir[src] to right after ir[dst]
void ir_move_instr_after(int src, int dst)
{
    if(src < dst || dst >= ir->n_values || src >= ir->n_values) goto bad_args;
    if(src == dst) return; // same ordering

    ir_insn* insn = ir->values[src];
    for(int i = src; i > dst; i--) {
        ir->values[i] = ir->values[i-1];
    }

    ir->values[dst+1] = insn;
    return;

    bad_args:
    printf("ir_move_instr_after: instructions can only be moved upwards\n");
}

void ir_add_instr_after(ir_insn* instr, int index)
{
    vector_ir_insn_add(ir, instr);
    ir_move_instr_after(ir->n_values - 1, index);
}

// this function deletes an instruction and frees its memory
void ir_remove_instruction(ir_insn* instr)
{
    int i = 0;
    for(; ir->values[i] != instr; i++);
    free(ir->values[i]); // I know there can be memory leaks here but I cba
    for(; i < ir->n_values - 1; i++) ir->values[i] = ir->values[i+1];
    ir->n_values--;
}

// this function reorders instructions to optimize the given block
void ir_block_reorder_instructions(int start, int end)
{
    // each variable can be moved up to the last of its dependencies
    // if it has multiple value assignments, each assignment can be moved up to the last of its dependencies
    // if it has no uses between two assignments, the first assignment can be optimized out
    // unless it's a ptr write, due to mmio and such things
    // also, due to pointer aliasing, two ptr writes have to remain in the same order
    // but I'll worry about the ptr stuff later; let's do basic reordering first

    // what infrastructure do I need for this? ideally, a function that lets me easily reorder instructions
    // one that takes the IR vector, an instr index, and the index after which the instr should be moved

    for(int i = start; i < end; i++) {

    }
}

// result = a + b * c turns into:
// t0 = b * c;
// t1 = a + t0;
// result = t1
// this pass optimizes t1 out and turns it into
// result = a + t0
void ir_remove_redundant_assignments(void)
{
    for(int i = 1; i < ir->n_values; i++) {
        if(ir->values[i]->type == IR_COPY && ir->values[i]->content.copy.src->type == IR_VAR && 
        ir_insn_is(ir->values[i-1], 2, IR_UN, IR_BIN) && 
        strcmp(((ir_un*) ir->values[i-1])->result->name, 
        ir->values[i]->content.copy.src->content.var->name) == 0) {
            ((ir_un*) ir->values[i-1])->result = ir->values[i]->content.copy.dst;
            ir_remove_instruction(ir->values[i]);
            i--;
        }
    }
}

// Emits IR for short circuiting the given logical AND/OR expression.
ir_value* ir_short_circuit(ast_expr* e)
{
    // convert this into a conditional, an IR_IF
    // int res = first && second
    // should become
    // int res = 0;
    // if(first) goto evaluate_second;
    // goto after;
    // evaluate_second:
    // if(second) goto true;
    // goto after;
    // true:
    // res = 1;
    // after: (... other code)

    if(e->content.bin.op == O_LOGICAL_AND) {
        char* l_evaluate_second = ir_autolabel();
        char* l_after = ir_autolabel();
        char* l_true = ir_autolabel();
        ir_var* res = ir_temp(e->content.bin.type);

        ir_insn* res_equals_0 = calloc(1, sizeof(ir_insn));
        res_equals_0->type = IR_COPY;
        res_equals_0->content.copy.dst = res;
        res_equals_0->content.copy.src = ir_value_lit(0);
        ir_add(res_equals_0);

        ir_insn* if_first = calloc(1, sizeof(ir_insn));
        if_first->type = IR_IF;
        if_first->content.condjmp.cond = ir_expr(e->content.bin.left);
        if_first->content.condjmp.if_true = l_evaluate_second;
        ir_add(if_first);

        ir_insn* goto_after = calloc(1, sizeof(ir_insn));
        goto_after->type = IR_GOTO;
        goto_after->content.jmp.dst = l_after;
        ir_add(goto_after);

        ir_insn* evaluate_second = malloc(sizeof(ir_insn));
        evaluate_second->type = IR_NOP;
        evaluate_second->label = l_evaluate_second;
        ir_add(evaluate_second);

        ir_insn* if_second = calloc(1, sizeof(ir_insn));
        if_second->type = IR_IF;
        if_second->content.condjmp.cond = ir_expr(e->content.bin.right);
        if_second->content.condjmp.if_true = l_true;
        ir_add(if_second);

        ir_insn* another_goto_after = calloc(1, sizeof(ir_insn));
        another_goto_after->type = IR_GOTO;
        another_goto_after->content.jmp.dst = l_after;
        ir_add(another_goto_after);

        ir_insn* nop_true = malloc(sizeof(ir_insn));
        nop_true->type = IR_NOP;
        nop_true->label = l_true;
        ir_add(nop_true);

        ir_insn* res_equals_1 = calloc(1, sizeof(ir_insn));
        res_equals_1->type = IR_COPY;
        res_equals_1->content.copy.dst = res;
        res_equals_1->content.copy.src = ir_value_lit(1);
        ir_add(res_equals_1);

        ir_insn* after = malloc(sizeof(ir_insn));
        after->type = IR_NOP;
        after->label = l_after;
        ir_add(after);

        // wrap the ir_var into an ir_value
        ir_value* res_value = malloc(sizeof(ir_value));
        res_value->type = IR_VAR;
        res_value->content.var = res;
        return res_value;
    }

    else {
        // int res = first || second
        // should become
        // int res = 1;
        // if(first) goto after;
        // if(second) goto after;
        // res = 0;
        // after: (... other code)

        char* l_after = ir_autolabel();
        ir_var* res = ir_temp(e->content.bin.type);

        ir_insn* res_equals_1 = calloc(1, sizeof(ir_insn));
        res_equals_1->type = IR_COPY;
        res_equals_1->content.copy.dst = res;
        res_equals_1->content.copy.src = ir_value_lit(1);
        ir_add(res_equals_1);

        ir_insn* if_first = calloc(1, sizeof(ir_insn));
        if_first->type = IR_IF;
        if_first->content.condjmp.cond = ir_expr(e->content.bin.left);
        if_first->content.condjmp.if_true = l_after;
        ir_add(if_first);

        ir_insn* if_second = calloc(1, sizeof(ir_insn));
        if_second->type = IR_IF;
        if_second->content.condjmp.cond = ir_expr(e->content.bin.right);
        if_second->content.condjmp.if_true = l_after;
        ir_add(if_second);

        ir_insn* res_equals_0 = calloc(1, sizeof(ir_insn));
        res_equals_0->type = IR_COPY;
        res_equals_0->content.copy.dst = res;
        res_equals_0->content.copy.src = ir_value_lit(0);
        ir_add(res_equals_0);

        ir_insn* after = malloc(sizeof(ir_insn));
        after->type = IR_NOP;
        after->label = l_after;
        ir_add(after);

        ir_value* res_value = malloc(sizeof(ir_value));
        res_value->type = IR_VAR;
        res_value->content.var = res;
        return res_value;
    }
}

// ==== REGISTER COLORING ===

// I'm doing basic block register coloring
// the idea is, for each basic block (a chunk of code which has only one jump, at the end),
// there will be an interference graph, where each node is a variable,
// and if one variable is live at the same time as another
// (and thus can't be in the same register), their nodes are linked

// ok the basic approach is
// I track where a variable's life begins and where it ends
// and for each variable that has an overlapping life at any point, assign them to different registers
// a variable will get its own register when it becomes live,
// and it gets stored back when it dies AND another variable needs to get its own register

// so I need a function that will go block by block in the IR and create its interference graph

// this function returns the interference graph for the given block
var_graph* ir_get_interference_graph(var_vector* vars, int start, int end)
{
    // go through the IR to find where each var is first and last used
    // from its first use (inclusive) to its last use (exclusive),
    // it should interfere with others that are live at any point during that

    //                              integer promotion is making me do this, can't memset
    int life_start[vars->n_values]; for(int i = 0; i < vars->n_values; i++) life_start[i] = -1;
    int life_end[vars->n_values];   for(int i = 0; i < vars->n_values; i++) life_end[i] = -1;

    for(int ip = start; ip < end; ip++) {
        ir_insn* insn = ir->values[ip];
        
        if(ir_out) {
            char s[128] = {0};
            ir_print_instr(ir->values[ip], s);
            FILE* ir_f = fopen(ir_out, "a");
            fprintf(ir_f, "%s", s);
            fclose(ir_f);
        }

        switch(insn->type) {
            case IR_NOP: break;
            case IR_UN: {
                int result_pos = ir_find_var(vars, insn->content.un.result);
                int operand_pos = ir_find_maybe_var(vars, insn->content.un.operand);
                if(life_start[result_pos] == -1) life_start[result_pos] = ip;
                if(operand_pos != -1 && life_start[operand_pos] == -1) life_start[operand_pos] = ip;
                // this can return negatives, or non-overlapping lifetimes
                // the end lifetime should be taken somewhat loosely,
                // vars that don't have overlapping lifetimes won't be assigned registers // later
                life_end[result_pos] = ip;
                if(operand_pos != -1) life_end[operand_pos] = ip;
            }
            break;

            case IR_BIN: {
                int result_pos = ir_find_var(vars, insn->content.bin.result);
                int left_pos = ir_find_maybe_var(vars, insn->content.bin.left);
                int right_pos = ir_find_maybe_var(vars, insn->content.bin.right);
                if(life_start[result_pos] == -1) life_start[result_pos] = ip;
                if(left_pos != -1 && life_start[left_pos] == -1) life_start[left_pos] = ip;
                if(right_pos != -1 && life_start[right_pos] == -1) life_start[right_pos] = ip;
                life_end[result_pos] = ip;
                if(left_pos != -1) life_end[left_pos] = ip;
                if(right_pos != -1) life_end[right_pos] = ip;
            }
            break;

            case IR_COPY: {
                int dst_pos = ir_find_var(vars, insn->content.copy.dst);
                int src_pos = ir_find_maybe_var(vars, insn->content.copy.src);
                if(life_start[dst_pos] == -1) life_start[dst_pos] = ip;
                if(src_pos != -1 && life_start[src_pos] == -1) life_start[src_pos] = ip;
                life_end[dst_pos] = ip;
                if(src_pos != -1) life_end[src_pos] = ip;
            }
            break;

            // these end basic blocks, I don't think their end lifetimes matter
            // but whatever

            case IR_IF: {
                int cond_pos = ir_find_maybe_var(vars, insn->content.condjmp.cond);
                if(cond_pos != -1 && life_start[cond_pos] == -1) life_start[cond_pos] = ip;
                if(cond_pos != -1) life_end[cond_pos] = ip;
            }
            break;

            case IR_FN_CALL: {
                int result_pos = -1;
                if(insn->content.fn_call.result) result_pos = ir_find_var(vars, insn->content.fn_call.result);
                else continue;
                if(life_start[result_pos] == -1) life_start[result_pos] = ip;
                life_end[result_pos] = ip;
            }
            break;

            case IR_RETURN: {
                if(!insn->content.ret.value || insn->content.ret.value->type != IR_VAR) break;
                int value_pos = ir_find_var(vars, insn->content.ret.value->content.var);
                if(life_start[value_pos] == -1) life_start[value_pos] = ip;
                life_end[value_pos] = ip;
            }
            break;

            // same struct layout for all 3 cases
            case IR_ASSIGN_REF: 
            case IR_ASSIGN_DEREF:
            case IR_DEREF_ASSIGN: {
                int dst_pos = ir_find_var(vars, insn->content.assign_ref.dst);
                int src_pos = ir_find_maybe_var(vars, insn->content.assign_ref.src);
                if(life_start[dst_pos] == -1) life_start[dst_pos] = ip;
                if(life_start[src_pos] == -1) life_start[src_pos] = ip;
                life_end[dst_pos] = ip;
                life_end[src_pos] = ip;
            }
            break;

            default: break;
        }
    }

    // validate the graph to avoid headaches later
    for(int i = 0; i < vars->n_values; i++) if(life_start[i] == -1) life_start[i] = start;//assert(life_start[i] > -1);

    // all lifetimes are gathered, now construct the graph

    var_graph* g = graph_ir_var_new(vars);

    for(int i = 0; i < g->nodes->n_values; i++) {
        int i_start = life_start[i];
        int i_end = life_end[i];

        for(int j = 0; j < g->nodes->n_values; j++) {
            int j_start = life_start[j];
            if(i_end >= j_start && i_start <= j_start) g->matrix[i][j] = 1;
        }
    }

    return g;
}
