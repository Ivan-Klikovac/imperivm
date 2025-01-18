#ifndef _IMPERIVM_BACKEND_AMD64_ASM_H
#define _IMPERIVM_BACKEND_AMD64_ASM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector.h> // I've yet to move the whole compiler to templated vectors
#include <IR/IR.h>
#include <backend/amd64/amd64.h>
#include <templates/vector.h>

// function overloading

#define amd64_neg(arg) _Generic((arg), \
    ir_var*: amd64_neg_m, \
    int: amd64_neg_r)(arg)

#define amd64_not(arg) _Generic((arg), \
    ir_var*: amd64_not_m, \
    int: amd64_not_r)(arg)

#define amd64_push(arg) _Generic((arg), \
    ir_var*: amd64_push_m, \
    ir_value*: amd64_push_v, \
    int64_t: amd64_push_i, \
    int: amd64_push_r)(arg)

#define amd64_pop(arg) _Generic((arg), \
    ir_var*: amd64_pop_m, \
    int: amd64_pop_r)(arg)

#define amd64_load(reg, operand) _Generic((operand), \
    ir_var*: amd64_load_var, \
    int64_t: amd64_load_lit, \
    ir_value*: amd64_load_val)(reg, operand)

#define amd64_mov(reg_dst, src) _Generic((src), \
    ir_var*: amd64_mov_rm, \
    int64_t: amd64_mov_ri, \
    ir_value*: amd64_mov_rv, \
    int: amd64_mov_rr)(reg_dst, src)

// assembly generation

// translates a register number (color) into its name
// RSP and RBP are never used for register coloring
static inline char* amd64_reg_name(int reg)
{
    switch(reg) {
        case 0: return "rax";
        case 1: return "rbx";
        case 2: return "rcx";
        case 3: return "rdx";
        case 4: return "rsi";
        case 5: return "rdi";
        case 6: return "r8";
        case 7: return "r9";
        case 8: return "r10";
        case 9: return "r11";
        case 10: return "r12";
        case 11: return "r13";
        case 12: return "r14";
        case 13: return "r15";
        case 14: return "rsp";
        case 15: return "rbp";
        default: return 0;
    }
}

static inline char* amd64_reg8_name(int reg)
{
    switch(reg) {
        case 0: return "al";
        case 1: return "bl";
        case 2: return "cl";
        case 3: return "dl";
        case 4: return "sil";
        case 5: return "dil";
        case 6: return "r8b";
        case 7: return "r9b";
        case 8: return "r10b";
        case 9: return "r11b";
        case 10: return "r12b";
        case 11: return "r13b";
        case 12: return "r14b";
        case 13: return "r15b";
        default: return NULL;
    }
}

static inline char* amd64_type_to_str(type_info* t)
{
    if(!t) return "quad";
    if(t->ptr_layers) return "quad";

    switch(t->base) {
        case CHAR_T:
        case UCHAR_T:
        case INT_T:
        case UINT_T:
        return "long"; // ?
        
        case LONG_T: 
        case ULONG_T:
        default:
        return "quad";
    }
}

static inline void amd64_label(char* label)
{
    char buffer[64];
    sprintf(buffer, "%s:\n", label);
    if(strncmp(label, "fn.", 3)) asm_add(strdup(buffer));
    else if(strcmp(label, "fn.main") == 0) asm_add("main:\n");
    else asm_add(strdup(buffer+3));
}

static inline void amd64_load_var(int reg, ir_var* var)
{
    char buffer[64];
    if(is_global(var)) sprintf(buffer, "movq %s(%%rip), %%%s\n", var->name, amd64_reg_name(reg));
    else sprintf(buffer, "movq %ld(%%rsp), %%%s\n", get_offset(var), amd64_reg_name(reg));
    asm_add(strdup(buffer));
}

static inline void amd64_load_lit(int reg, uint64_t lit)
{
    char buffer[64];
    sprintf(buffer, "movq $%lu, %%%s\n", lit, amd64_reg_name(reg));
    asm_add(strdup(buffer));
}

static inline void amd64_load_val(int reg, ir_value* value)
{
    if(value->type == IR_VAR) amd64_load_var(reg, value->content.var);
    else amd64_load_lit(reg, value->content.lit.i);
}

static inline void amd64_spill(int reg, ir_var* var)
{
    if(!var) return;

    char buffer[64];
    if(is_global(var)) sprintf(buffer, "movq %%%s, %s(%%rip)\n", amd64_reg_name(reg), var->name);
    else sprintf(buffer, "movq %%%s, %ld(%%rsp)\n", amd64_reg_name(reg), get_offset(var));
    asm_add(strdup(buffer));
}

static inline void amd64_neg_r(int reg)
{
    char buffer[64];
    sprintf(buffer, "negq %%%s\n", amd64_reg_name(reg));
    asm_add(strdup(buffer));
}

static inline void amd64_neg_m(ir_var* var)
{
    char buffer[64];
    if(is_global(var)) sprintf(buffer, "negq %s(%%rip)\n", var->name);
    else sprintf(buffer, "negq %ld(%%rsp)\n", get_offset(var));
    asm_add(strdup(buffer));
}

static inline void amd64_not_r(int reg)
{
    char buffer[64];
    sprintf(buffer, "notq %%%s\n", amd64_reg_name(reg));
    asm_add(strdup(buffer));
}

// binary NOT
static inline void amd64_not_m(ir_var* var)
{
    char buffer[64];
    if(is_global(var)) sprintf(buffer, "notq %s(%%rip)\n", var->name);
    else sprintf(buffer, "notq %ld(%%rsp)\n", get_offset(var));
    asm_add(strdup(buffer));
}

static inline void amd64_cmp_rr(int op1, int op2)
{
    char buffer[64];
    sprintf(buffer, "cmp %%%s, %%%s\n", amd64_reg_name(op2), amd64_reg_name(op1));
    asm_add(strdup(buffer));
}

static inline void amd64_cmp_rm(int reg_op, ir_var* mem_op)
{
    char buffer[64];
    if(is_global(mem_op)) sprintf(buffer, "cmp %s(%%rip), %%%s", mem_op->name, amd64_reg_name(reg_op));
    else sprintf(buffer, "cmp %ld(%%rsp), %%%s\n", get_offset(mem_op), amd64_reg_name(reg_op));
    asm_add(strdup(buffer));
}

static inline void amd64_cmp_ri(int reg_op, int64_t imm_op)
{
    char buffer[64];
    sprintf(buffer, "cmp $%ld, %%%s\n", imm_op, amd64_reg_name(reg_op));
    asm_add(strdup(buffer));
}

static inline void amd64_cmp_rv(int reg_op, ir_value* v)
{
    if(v->type == IR_VAR) amd64_cmp_rm(reg_op, v->content.var);
    else amd64_cmp_ri(reg_op, v->content.lit.i);
}

// sets the low byte of reg to 0 or 1 if the last cmp evaluated to less
static inline void amd64_setl_r(int reg)
{
    char buffer[64];
    sprintf(buffer, "setl %%%s\n", amd64_reg8_name(reg));
    asm_add(strdup(buffer));
}

// lesser or equal
static inline void amd64_setle_r(int reg)
{
    char buffer[64];
    sprintf(buffer, "setle %%%s\n", amd64_reg8_name(reg));
    asm_add(strdup(buffer));
}

// greater
static inline void amd64_setg_r(int reg)
{
    char buffer[64];
    sprintf(buffer, "setg %%%s\n", amd64_reg8_name(reg));
    asm_add(strdup(buffer));
}

// greater or equal
static inline void amd64_setge_r(int reg)
{
    char buffer[64];
    sprintf(buffer, "setge %%%s\n", amd64_reg8_name(reg));
    asm_add(strdup(buffer));
}

// equal
static inline void amd64_sete_r(int reg)
{
    char buffer[64];
    sprintf(buffer, "sete %%%s\n", amd64_reg8_name(reg));
    asm_add(strdup(buffer));
}

// not equal
static inline void amd64_setne_r(int reg)
{
    char buffer[64];
    sprintf(buffer, "setne %%%s\n", amd64_reg_name(reg));
    asm_add(strdup(buffer));
}

// movzbq from the low byte in reg to the whole 64-bit reg
static inline void amd64_movzbq_r(int reg)
{
    char buffer[64];
    sprintf(buffer, "movzbq %%%s, %%%s\n", amd64_reg8_name(reg), amd64_reg_name(reg));
    asm_add(strdup(buffer));
}

static inline void amd64_logical_not_r(int reg)
{
    // test the register (bitwise AND, set flags, discard result)
    // then setz (set the register to 1 if ZF, otherwise 0)
    // and movzbq (zero-extend move from byte to quad, clears the upper bits of the register)

    char buffer[64];
    sprintf(buffer, "test %%%s, %%%s\n", amd64_reg_name(reg), amd64_reg_name(reg));
    asm_add(strdup(buffer));
    sprintf(buffer, "setz %%%s\n", amd64_reg8_name(reg));
    asm_add(strdup(buffer));
    amd64_movzbq_r(reg);
}

static inline void amd64_mov_rr(int dst, int src)
{
    if(dst == src) return;

    char buffer[64];
    sprintf(buffer, "movq %%%s, %%%s\n", amd64_reg_name(src), amd64_reg_name(dst));
    asm_add(strdup(buffer));
}

static inline void amd64_mov_ri(int dst, int64_t imm)
{
    char buffer[64];
    sprintf(buffer, "movq $%ld, %%%s\n", imm, amd64_reg_name(dst));
    asm_add(strdup(buffer));
}

static inline void amd64_mov_rm(int dst, ir_var* src)
{
    char buffer[64];
    if(is_global(src)) sprintf(buffer, "movq %s(%%rip), %%%s\n", src->name, amd64_reg_name(dst));
    else sprintf(buffer, "movq %ld(%%rsp), %%%s\n", get_offset(src), amd64_reg_name(dst));
    asm_add(strdup(buffer));
}

static inline void amd64_mov_rv(int dst, ir_value* value)
{
    if(value->type == IR_VAR) amd64_mov_rm(dst, value->content.var);
    else amd64_mov_ri(dst, value->content.lit.i);
}

// Dereferences ptr_reg and places the value in dst_reg.
static inline void amd64_deref_mov_rr(int dst_reg, int ptr_reg)
{
    char buffer[64];
    sprintf(buffer, "movq (%%%s), %%%s\n", amd64_reg_name(ptr_reg), amd64_reg_name(dst_reg));
    asm_add(strdup(buffer));
}

static inline void amd64_deref_mov_mr(ir_var* dst, int ptr_reg)
{
    char buffer[64];
    if(is_global(dst)) sprintf(buffer, "movq (%%%s), %s(%%rip)\n", amd64_reg_name(ptr_reg), dst->name);
    else sprintf(buffer, "movq (%%%s), %ld(%%rsp)\n", amd64_reg_name(ptr_reg), get_offset(dst));
    asm_add(strdup(buffer));
}

// Puts the value of src into the memory pointed to by dst_ptr_reg.
static inline void amd64_copy_through_ptr_rm(int dst_ptr_reg, ir_var* src)
{
    char buffer[64];
    if(is_global(src)) sprintf(buffer, "movq %s(%%rip), (%%%s)\n", src->name, amd64_reg_name(dst_ptr_reg));
    else sprintf(buffer, "movq %ld(%%rsp), (%%%s)\n", get_offset(src), amd64_reg_name(dst_ptr_reg));
    asm_add(strdup(buffer));
}

static inline void amd64_copy_through_ptr_ri(int dst_ptr_reg, int64_t src)
{
    char buffer[64];
    sprintf(buffer, "movq $%ld, (%%%s)\n", src, amd64_reg_name(dst_ptr_reg));
    asm_add(strdup(buffer));
}

static inline void amd64_copy_through_ptr_rv(int dst_ptr_reg, ir_value* src)
{
    if(src->type == IR_VAR) amd64_copy_through_ptr_rm(dst_ptr_reg, src->content.var);
    else amd64_copy_through_ptr_ri(dst_ptr_reg, src->content.lit.i);
}

// dst = dst - src
static inline void amd64_sub_rr(int dst, int src)
{
    char buffer[64];
    sprintf(buffer, "subq %%%s, %%%s\n", amd64_reg_name(src), amd64_reg_name(dst));
    asm_add(strdup(buffer));
}

static inline void amd64_sub_rm(int dst, ir_var* src)
{
    char buffer[64];
    if(is_global(src)) sprintf(buffer, "subq %s(%%rip), %%%s\n", src->name, amd64_reg_name(dst));
    else sprintf(buffer, "subq %ld(%%rsp), %%%s\n", get_offset(src), amd64_reg_name(dst));
    asm_add(strdup(buffer));
}

static inline void amd64_sub_mr(ir_var* dst, int src)
{
    char buffer[64];
    if(is_global(dst)) sprintf(buffer, "subq %%%s, %s(%%rip)\n", amd64_reg_name(src), dst->name);
    else sprintf(buffer, "subq %%%s, %ld(%%rsp)\n", amd64_reg_name(src), get_offset(dst));
    asm_add(strdup(buffer));
}

static inline void amd64_sub_ri(int dst, int64_t src)
{
    char buffer[64];
    sprintf(buffer, "subq $%ld, %%%s\n", src, amd64_reg_name(dst));
    asm_add(strdup(buffer));
}

static inline void amd64_sub_rv(int reg, ir_value* value)
{
    if(value->type == IR_VAR) amd64_sub_rm(reg, value->content.var);
    else amd64_sub_ri(reg, value->content.lit.i);
}

// dst = dst + src
static inline void amd64_add_rr(int dst, int src)
{
    char buffer[64];
    sprintf(buffer, "addq %%%s, %%%s\n", amd64_reg_name(src), amd64_reg_name(dst));
    asm_add(strdup(buffer));
}

static inline void amd64_add_rm(int dst, ir_var* src)
{
    char buffer[64];
    if(is_global(src)) sprintf(buffer, "addq %s(%%rip), %%%s\n", src->name, amd64_reg_name(dst));
    else sprintf(buffer, "addq %ld(%%rsp), %%%s\n", get_offset(src), amd64_reg_name(dst));
    asm_add(strdup(buffer));
}

static inline void amd64_add_mr(ir_var* dst, int src)
{
    char buffer[64];
    if(is_global(dst)) sprintf(buffer, "addq %%%s, %s(%%rip)\n", amd64_reg_name(src), dst->name);
    else sprintf(buffer, "addq %%%s, %ld(%%rsp)\n", amd64_reg_name(src), get_offset(dst));
    asm_add(strdup(buffer));
}

static inline void amd64_add_ri(int dst, int64_t src)
{
    char buffer[64];
    sprintf(buffer, "addq $%ld, %%%s\n", src, amd64_reg_name(dst));
    asm_add(strdup(buffer));
}

static inline void amd64_add_rv(int reg, ir_value* value)
{
    if(value->type == IR_VAR) amd64_add_rm(reg, value->content.var);
    else amd64_add_ri(reg, value->content.lit.i);
}

static inline void amd64_jmp(char* label)
{
    char buffer[64];
    sprintf(buffer, "jmp %s\n", label);
    asm_add(strdup(buffer));
}

// does a bitwise AND, discards the value, and updates some flags, notably ZF and SF
static inline void amd64_test_rr(int r1, int r2)
{
    char buffer[64];
    sprintf(buffer, "test %%%s, %%%s\n", amd64_reg_name(r1), amd64_reg_name(r2));
    asm_add(strdup(buffer));
}

// jump if not zero
static inline void amd64_jnz_r(char* label)
{
    char buffer[64];
    sprintf(buffer, "jnz %s\n", label);
    asm_add(strdup(buffer));
}

// signed multiplication; RAX * reg = RDX:RAX (the result is 128-bit)
static inline void amd64_imul_r(int reg)
{
    char buffer[64];
    sprintf(buffer, "imulq %%%s\n", amd64_reg_name(reg));
    asm_add(strdup(buffer));
}

// RAX * var = RDX:RAX
static inline void amd64_imul_m(ir_var* var)
{
    char buffer[64];
    if(is_global(var)) sprintf(buffer, "imulq %s(%%rip)\n", var->name);
    else sprintf(buffer, "imulq %ld(%%rsp)\n", get_offset(var));
    asm_add(strdup(buffer));
}

// signed multiplication; dst = dst * src (result remains 64-bit)
static inline void amd64_imul_rr(int dst, int src)
{
    char buffer[64];
    sprintf(buffer, "imulq %%%s, %%%s\n", amd64_reg_name(src), amd64_reg_name(dst));
    asm_add(strdup(buffer));
}

static inline void amd64_imul_rm(int dst, ir_var* src)
{
    char buffer[64];
    if(is_global(src)) sprintf(buffer, "imulq %s(%%rip), %%%s\n", src->name, amd64_reg_name(dst));
    else sprintf(buffer, "imulq %ld(%%rsp), %%%s\n", get_offset(src), amd64_reg_name(dst));
    asm_add(strdup(buffer));
}

// dst = dst * imm
static inline void amd64_imul_ri(int dst, int64_t imm)
{
    char buffer[64];
    sprintf(buffer, "imulq $%ld, %%%s\n", imm, amd64_reg_name(dst));
    asm_add(strdup(buffer));
}

static inline void amd64_imul_rv(int dst, ir_value* value)
{
    if(value->type == IR_VAR) amd64_imul_rm(dst, value->content.var);
    else amd64_imul_ri(dst, value->content.lit.i);
}

static inline void amd64_imul_rri(int dst, int src, int64_t imm)
{
    char buffer[64];
    sprintf(buffer, "imulq $%ld, %%%s, %%%s\n", imm, amd64_reg_name(src), amd64_reg_name(dst));
    asm_add(strdup(buffer));
}

// dst = var * imm
static inline void amd64_imul_rmi(int dst, ir_var* var, int64_t imm)
{
    char buffer[64];
    if(is_global(var)) sprintf(buffer, "imulq $%ld, %s(%%rip), %%%s\n", imm, var->name, amd64_reg_name(dst));
    else sprintf(buffer, "imulq $%ld, %ld(%%rsp), %%%s\n", imm, get_offset(var), amd64_reg_name(dst));
    asm_add(strdup(buffer));
}

static inline void amd64_push_r(int reg)
{
    char buffer[64];
    sprintf(buffer, "pushq %%%s\n", amd64_reg_name(reg));
    asm_add(strdup(buffer));
}

static inline void amd64_push_m(ir_var* var)
{
    char buffer[64];
    if(is_global(var)) sprintf(buffer, "pushq %s(%%rip)\n", var->name);
    else sprintf(buffer, "pushq %ld(%%rsp)\n", get_offset(var));
    asm_add(strdup(buffer));
}

static inline void amd64_push_i(int64_t imm)
{
    char buffer[64];
    sprintf(buffer, "pushq $%ld\n", imm);
    asm_add(strdup(buffer));
}

static inline void amd64_push_v(ir_value* value)
{
    if(value->type == IR_VAR) amd64_push_m(value->content.var);
    else amd64_push_i(value->content.lit.i);
}

static inline void amd64_pop_r(int reg)
{
    char buffer[64];
    sprintf(buffer, "popq %%%s\n", amd64_reg_name(reg));
    asm_add(strdup(buffer));
}

static inline void amd64_pop_m(ir_var* var)
{
    char buffer[64];
    if(is_global(var)) sprintf(buffer, "popq %s(%%rip)\n", var->name);
    else sprintf(buffer, "popq %ld(%%rsp)\n", get_offset(var));
    asm_add(strdup(buffer));
}

static inline void amd64_call(char* label)
{
    char buffer[64];
    sprintf(buffer, "call %s\n", label);
    asm_add(strdup(buffer));
}

static inline void amd64_ret(void)
{
    asm_add("ret\n");
}

static inline void amd64_global_var(ir_var* var, int64_t value)
{
    char buffer[64];
    sprintf(buffer, "%s: .%s %ld\n", var->name, amd64_type_to_str(var->type), value);
    asm_add(strdup(buffer));
}

// emit code for the exit linux syscall with the given return value
static inline void _amd64_exit(int64_t status_code)
{
    char buffer[64];
    asm_add("movq $60, %rax\n");
    sprintf(buffer, "movq %ld, %%rdi\n", status_code);
    asm_add(strdup(buffer));
}

#endif