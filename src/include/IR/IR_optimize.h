#ifndef _IMPERIVM_IR_IR_OPTIMIZE_H
#define _IMPERIVM_IR_IR_OPTIMIZE_H

#include <IR/IR.h>

int ir_get_block(int* start, int* end);
var_vector* ir_get_vars(int start, int end);
var_vector* ir_get_local_vars(char* fn);
ast_fn* ir_get_ast_fn(char* fn_name);
void ir_block_remove_unused_assignments(int start, int end);
void ir_move_instr_after(int src, int dst);
void ir_remove_instruction(ir_insn* instr);
void ir_block_reorder_instructions(int start, int end);
void ir_remove_redundant_assignments(void);
ir_value* ir_short_circuit(ast_expr* e);
var_graph* ir_get_interference_graph(var_vector* vars, int start, int end);

#endif