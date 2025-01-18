#ifndef _IMPERIVM_IR_IR_PRINT_H
#define _IMPERIVM_IR_IR_PRINT_H

#include <IR/IR.h>

void ir_print_var(ir_var* var);
void ir_print_value(ir_value* value);
void ir_print_op(ir_op op);
void ir_print_instr(ir_insn* instr, char* ir_output);

#endif