#ifndef _IMPERIVM_BACKEND_AMD64_AMD64_H
#define _IMPERIVM_BACKEND_AMD64_AMD64_H

#include "IR/IR_optimize.h"
#include <stdio.h>
#include <assert.h>

#include <IR/IR.h>
#include <templates/vector.h>
#include <templates/graph.h>

#define N_REGS 16

#define RAX 0
#define RBX 1
#define RCX 2
#define RDX 3
#define RSI 4
#define RDI 5
#define R8  6
#define R9  7
#define R10 8
#define R11 9
#define R12 10
#define R13 11
#define R14 12
#define R15 13
#define RSP 14
#define RBP 15

// when calling a function, its local variables will be stored in vars,
// the address of each local variable relative to RSP will be in offsets,
// and the total size of the stack frame will be in size
// this vector will function like a stack structure

// in a practical example, suppose you're calling a function that takes 9 arguments
// int fn(int a, int b, int c, int d, int e, int f, int g, int h, int i);
// the first 6 will be passed in registers,
// and the other three will be put on the stack before the function is called
// the stack will thus be, horizontally from left to right, as follows:
// i | h | g (each | is an 8 byte boundary)

// the function is then called with the CALL instruction,
// which pushes the address of the next instruction on the stack and jumps to fn
// i | h | g | ret_addr

// now we're in the called function
// according to the SysV calling convention, the function pushes RBP and moves RSP into it
// i | h | g | ret_addr | RBP

// the function then has to save the non-scratch registers it modifies
// i | h | g | ret_addr | RBP | RBX | R12 | R13 | R14 | R15

// it then also needs to allocate stack space for its local variables
// so, supposing it has three 64-bit integers, the stack will be:
// i | h | g | ret_addr | RBP | RBX | R12 | R13 | R14 | R15 | local1 | local2 | local3

// so in the function epilogue, what I need to do is:
// - subtract (n=3) * 8 bytes from RSP to deallocate local variables
// i | h | g | ret_addr | RBP | RBX | R12 | R13 | R14 | R15
// - pop back the saved registers in reverse order
// i | h | g | ret_addr | RBP
// - move RBP into RSP and pop RBP
// i | h | g | ret_addr
// - then finally, execute RET, which pops the return address from the stack into RIP
// i | h | g
// and we're back in the calling function
// now we need to just subtract (n=3) * 8 bytes from RSP to deallocate the function arguments
// and the stack is clean again
// btw the return value of fn() is stored in RAX

// the SysV ABI on AMD64 requires that the stack is 16-byte aligned on function calls
// was that the case in the previous example? let's check
// the stack was i | h | g | ret_addr when the function was called
// that's 8*4 bytes, which respects the alignment requirement
// but suppose you have a function like this:
// int fn(int a, int b);
// it takes only two arguments, which will be passed in registers
// so when calling this function, the stack will be just
// ret_addr
// which is 8 bytes - the stack is not 16-byte aligned
// so if the function has an even number of arguments, we must pad the stack before calling
// as follows:
// padding | ret_addr
// after the function returns, we'll decrement RSP by 8 to restore the stack

ptr_vector(char);
ptr_vector(vector_ir_var);
#define asm_vector vector_char
#define _asm_add(value) vector_char_add(amd64_asm, value)
#define _is_global(var) (var->name[strlen(var->name)-1] == 'g')
#define _is_arg(var) (var->name[strlen(var->name)-1] == 'p')
#define _is_local(var) (var->name[strlen(var->name)-1] == 'l')
#define _stackframe_build() vector_vector_ir_var_add(stack_status, vector_ir_var_new())
#define _stackframe_add(var) vector_ir_var_add(stack_status->values[stack_status->n_values-1], var)
#define _stackframe_clean() vector_ir_var_free(stack_status->values[stack_status->n_values-1]); vector_vector_ir_var_remove(stack_status, stack_status->n_values-1);
#define stack_vector vector_vector_ir_var

static inline int has_reg(ir_var*);
static inline int get_reg(ir_var*);
static inline int check_reg(ir_var*);

#define symtable_get() hashmap_ast_fn_vector_ir_var_get(fn_symtable, amd64_current_fn);

// returns the position on the stack frame of the passed variable 
#define _get_local_pos(var) vector_ir_var_find(stack_status->values[stack_status->n_values-1], var)
// returns the position in the param vector of the current function
#define _get_arg_pos(var) vector_find(amd64_current_fn->param_vector, var)
// first six arguments are passed in registers so
#define _is_reg_arg(var) (get_arg_pos(var) < 6)
// this returns the actual offset on the stack for function call arguments
// potentially incorrect, should I use stack_status or param_vector here?
#define _arg_get_offset(var) (stack_status->values[stack_status->n_values-1]->n_values + 1 + (get_arg_pos(var) - 6)) // the +1 is for the return address
// and this returns the actual offset on the stack for local variables
#define _local_var_get_offset(var) (stack_status->values[stack_status->n_values-1]->n_values - get_local_pos(var))
// and then one macro to rule them all
//#define get_offset(var) (((is_arg(var) ? arg_get_offset(var) : local_var_get_offset(var)) * 8) == -8 ? getchar() : ((is_arg(var) ? arg_get_offset(var) : local_var_get_offset(var)) * 8))
#define _get_offset(var) ((is_arg(var) ? arg_get_offset(var) : local_var_get_offset(var)) * 8)

extern asm_vector* amd64_asm;
extern ir_var** reg_status;
extern var_graph* g;
extern stack_vector* stack_status;
extern ast_fn* amd64_current_fn;

static inline int has_reg(ir_var* var) { for(int i = 0; i < g->nodes->n_values; i++) if(strcmp(g->nodes->values[i]->name, var->name) == 0) return 1; return 0; }
static inline int get_reg(ir_var* var) { for(int i = 0; i < g->nodes->n_values; i++) if(strcmp(g->nodes->values[i]->name, var->name) == 0) return g->colors[i]; assert(1); return 0; } // return i;
static inline int check_reg(ir_var* var) { if(get_reg(var) == -1) return 1; if(reg_status[get_reg(var)] && strcmp(reg_status[get_reg(var)]->name, var->name) == 0) return 1; return 0; }

#define stackframe_top stack_status->values[stack_status->n_values-1]
int stackframe_find(ir_var* var);

static inline void stackframe_build(void) { vector_vector_ir_var_add(stack_status, vector_ir_var_new()); }
static inline void stackframe_clean(void) { vector_ir_var_free(stack_status->values[stack_status->n_values-1]); vector_vector_ir_var_remove(stack_status, stack_status->n_values-1); }
static inline void stackframe_add(ir_var* var) { vector_ir_var_add(stack_status->values[stack_status->n_values-1], var); }

static inline long is_global(ir_var* var) { return var->name[strlen(var->name)-1] == 'g'; }
static inline long is_arg(ir_var* var)    { return var->name[strlen(var->name)-1] == 'p'; }
static inline long is_local(ir_var* var)  { return var->name[strlen(var->name)-1] == 'l'; }
static inline long get_local_pos(ir_var* var) { return stackframe_find(var); }
static inline long local_var_get_offset(ir_var* var) { return (stack_status->values[stack_status->n_values-1]->n_values - (get_local_pos(var) + 1)); }
static inline long get_offset(ir_var* var) {
    //if(is_arg(var)) return ((arg_get_offset(var)) * 8);
    return ((local_var_get_offset(var)) * 8);
}

static inline int get_arg_reg(ir_var* var)
{
    var_vector* params = symtable_get();
    for(int i = 0; i < 6; i++) {
        if(strcmp(var->name, params->values[i]->name) == 0) {
            switch(i) {
                case 0: return RDI;
                case 1: return RSI;
                case 2: return RDX;
                case 3: return RCX;
                case 4: return R8;
                case 5: return R9;
            }
        }
    }

    assert(1);
    return -1;
}

// amd64.c
void amd64_init(void);
void amd64_color_registers(var_graph* g, int start, int end);
void amd64_global_vars(void);
void amd64_translate(var_graph* graph, int start, int end);

// amd64_translate.c
void ensure_reg(ir_var* var);
void amd64_prologue(void);
void amd64_epilogue(void);
void amd64_store(ir_var* var, int reg);
void amd64_fn_call(ir_fn_call* call);
void amd64_proc_call(ir_proc_call* call);
void amd64_un_rr(ir_un* un);
void amd64_un_mr(ir_un* un);
void amd64_un_rm(ir_un* un);
void amd64_un_mm(ir_un* un);
void amd64_bin_rrr(ir_bin* bin);
void amd64_bin_mrr(ir_bin* bin);
void amd64_bin_rrm(ir_bin* bin);
void amd64_bin_mrm(ir_bin* bin);
void amd64_bin_rmr(ir_bin* bin);
void amd64_bin_mmr(ir_bin* bin);
void amd64_bin_mmm(ir_bin* bin);
void amd64_bin_rmm(ir_bin* bin);
void amd64_copy_xr(ir_copy* copy);
void amd64_copy_rv(ir_copy* copy);
void amd64_copy_mm(ir_copy* copy);
void amd64_deref_assign(ir_deref_assign* insn);
void amd64_condjmp(ir_if* condjmp);
void amd64_exit(ir_return* ret);

static inline void asm_add(char* value) { vector_char_add(amd64_asm, value); if(verbose_asm) printf("%s", value); }

#endif
