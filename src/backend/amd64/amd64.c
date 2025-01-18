#include <stdlib.h>
#include <string.h>
#include <IR/IR.h>
#include <IR/IR_print.h>
#include <IR/IR_optimize.h>
#include <backend/amd64/amd64.h>
#include <backend/amd64/amd64_asm.h>
#include <templates/vector.h>
#include <templates/set.h>

#define spill_all() for(int i = 0; i < g->nodes->n_values; i++) /*if(g->nodes->values[i]->name[0] != '.')*/ amd64_spill(i, reg_status[i])
#define spill_globals() for(int i = 0; i < g->nodes->n_values; i++) if(reg_status[i] && is_global(reg_status[i])) amd64_spill(i, reg_status[i]);

type_set(ir_insn);

var_graph* g = 0;
asm_vector* amd64_asm = 0;
ir_var** reg_status = 0;
stack_vector* stack_status = 0;
ast_fn* amd64_current_fn = 0;

void reset_graph(var_graph* g)
{
    for(int i = 0; i < g->nodes->n_values; i++) g->colors[i] = -1;
}

int validate_graph(var_graph* g)
{
    for(int i = 0; i < g->nodes->n_values; i++)
        for(int j = 0; j < g->nodes->n_values; j++)
            if(g->matrix[i][j] && (g->colors[i] != -1) && (g->colors[i] == g->colors[j])) 
                return 0;
    
    for(int i = 0; i < g->nodes->n_values; i++)
        if(g->colors[i] == -1)
            return 0;
    
    return 1;
}

int safe_coloring(var_graph* g, int node, int color)
{
    for(int i = 0; i < g->nodes->n_values; i++)
        if(g->matrix[i][node] && g->colors[i] == color)
            return 0;
        
    return 1;
}

int color_node(var_graph* g, int node, int n_colors)
{
    if(node == g->nodes->n_values) return 1;

    for(int color = 0; color < n_colors; color++) {
        if(safe_coloring(g, node, color)) {
            g->colors[node] = color;
            if(color_node(g, node + 1, n_colors)) return 1;
            g->colors[node] = -1;
        }
    }

    return 0;
}

int color_graph(var_graph* g, int n_colors)
{
    return color_node(g, 0, n_colors);
}

// ABI NOTES ===
// using sysv abi
// function return values are stored in RAX
// RSP is never allocated for general use
// the caller is responsible for saving RAX, RCX, RDX, RSI, RDI, R8, R9, R10 and R11
// the callee saves RBX, RBP, R12, R13, R14 and R15
// function/procedure calls pass the first six integer/ptr args through RDI, RSI, RDX, RCX, R8 and R9

// if the graph is not N_REGS-colorable, this will mark vars for spilling so that it will be N_REGS-colorable
// this function returns the final graph, where only the vars and colors are important
void amd64_color_registers(var_graph* g, int start, int end)
{
    int i = 1;
    g->colors = malloc(g->nodes->n_values * sizeof(int));
    for(reset_graph(g); !color_graph(g, i); i++, reset_graph(g));
    if(i <= N_REGS - 3) return; 

    // I need to aim for N_REGS-3 to save RSP, RBP and another one (arbitrarily R15)
    // remove variables from consideration for coloring by LFU

    ir_output = malloc(65536);
    for(int ip = start; ip < end; ip++) 
        ir_print_instr(ir->values[ip], ir_output[strlen(ir_output)]);

    var_graph* graph = g;

    remove_one: {
        int uses[g->nodes->n_values];
        memset(uses, 0, graph->nodes->n_values * sizeof(int));
        for(int i = 0; i < graph->nodes->n_values; i++) {    
            char* current;
            while((current = strstr(ir_output, graph->nodes->values[i]->name))) {
                current++;
                uses[i]++;
            }
        }

        // now find the one with the least uses
        uint32_t min = (uint32_t) -1;
        int min_index = 0;
        for(int i = 0; i < graph->nodes->n_values; i++) if(uses[i] < min) {
            min = uses[i];
            min_index = i;
        }
        var_vector_remove(graph->nodes, min_index);
        var_graph* new_graph = graph_ir_var_new(graph->nodes);
        for(int i = 0; i < graph->nodes->n_values; i++) free(graph->matrix[i]);
        free(graph->matrix);
        free(graph->colors);
        free(graph);
        graph = new_graph;
    }

    if(!color_graph(graph, N_REGS - 3)) goto remove_one;
}

void amd64_translate(var_graph* graph, int start, int end)
{
    // this will contain the variables currently stored in each register
    // reg_status[0] will be set to 'a' if 'a' is currently in RAX
    // for each instruction I will check the register that is used to store each variable
    // if a new var put in a reg is not the var in reg_status, then first store the old reg value in memory
    ir_var* array[graph->nodes->n_values];
    memset(array, 0, graph->nodes->n_values * sizeof(ir_var*));
    reg_status = array;
    g = graph;

    for(int ip = start; ip < end; ip++) {
        ir_insn* insn = ir->values[ip];

        if(verbose_asm) {
            char* s = calloc(1, 64);
            ir_print_instr(insn, s);
            printf("--- IR:\n%s\n", s);
            free(s);
        }

        if(insn->label) {
            spill_all();
            amd64_label(insn->label);
            if(strncmp(insn->label, "fn.", 3) == 0) {
                amd64_current_fn = ir_get_ast_fn(insn->label);
                amd64_prologue();
            }
        }

        switch(insn->type) {
            case IR_NOP:
            asm_add("nop\n");
            break;

            case IR_UN:;
            ir_un* un = &insn->content.un;
            if(has_reg(un->result) && un->operand->type == IR_VAR && has_reg(un->operand->content.var))
                amd64_un_rr(un); 
            else if(has_reg(un->result) && un->operand->type == IR_VAR)
                amd64_un_rm(un);
            else if(un->operand->type == IR_VAR && has_reg(un->operand->content.var))
                amd64_un_mr(un);
            else amd64_un_mm(un);
            break;

            case IR_BIN:;
            ir_bin* bin = &insn->content.bin;

            int _1 = has_reg(bin->result);
            int _2 = bin->left->type == IR_VAR;
            int _3 = bin->right->type == IR_VAR;
            int _4 = _2 && has_reg(bin->left->content.var);
            int _5 = _3 && has_reg(bin->right->content.var);

            if(_1 && _4 && _5)       amd64_bin_rrr(bin);
            else if(_1 && _4 && !_5) amd64_bin_rrm(bin);
            else if(_1 && !_4 && _5) amd64_bin_rmr(bin);
            else if(_1)              amd64_bin_rmm(bin);
            else if(_4 && _5)        amd64_bin_mrr(bin);
            else if(_4 && !_5)       amd64_bin_mrm(bin);
            else if(!_4 && _5)       amd64_bin_mmr(bin);
            else                     amd64_bin_mmm(bin);
            break;

            case IR_COPY:;
            ir_copy* copy = &insn->content.copy;

            if(copy->src->type == IR_VAR && has_reg(copy->src->content.var)) 
                amd64_copy_xr(copy);
            else if(has_reg(copy->dst))
                amd64_copy_rv(copy);
            else amd64_copy_mm(copy);
            break;

            case IR_DEREF_ASSIGN:
            amd64_deref_assign(&insn->content.deref_assign);
            break;

            case IR_GOTO:
            spill_all();
            amd64_jmp(insn->content.jmp.dst);
            break;

            case IR_IF:
            spill_all();
            amd64_condjmp(&insn->content.condjmp);
            break;

            case IR_RETURN:;
            spill_globals();

            // move the return value, if any, into RAX
            ir_value* v = insn->content.ret.value;
            if(v && v->type == IR_VAR && has_reg(v->content.var)) {
                ensure_reg(v->content.var);
                amd64_mov_rr(RAX, get_reg(v->content.var));
            }
            else if(v)
                amd64_mov_rv(RAX, insn->content.ret.value);
            
            // emit code for clearing the stack frame
            amd64_epilogue();

            // and internally clear the stack frame if this is the last return in the function
            if(insn->content.ret.is_last) stackframe_clean();
            break;

            case IR_FN_CALL:
            spill_all();
            amd64_fn_call(&insn->content.fn_call);
            break;

            case IR_PROC_CALL:
            spill_all();
            amd64_proc_call(&insn->content.proc_call);
            break;

            default: asm_add("not implemented yet\n"); break;
        }
    }
}

void amd64_global_vars(void)
{
    ir_insn* insn = ir->values[0];
    int i = 0;
    while(insn->type == IR_COPY) {
        amd64_global_var(insn->content.copy.dst, insn->content.copy.src->content.lit.i);
        insn = ir->values[++i];
    }
    asm_add("\n.text\n.globl main\n");
}

void amd64_init(void)
{
    amd64_asm = vector_char_new();
    stack_status = vector_vector_ir_var_new();
    asm_add(".data\n");
    amd64_global_vars();
}