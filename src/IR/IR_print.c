#include <IR/IR.h>
#include <stdio.h>
#include <string.h>

void ir_print_var(ir_var* var, char* ir_output)
{
    if(!var) return;
    char buffer[64];
    sprintf(buffer, "%s ", var->name);
    strcat(ir_output, buffer);
}

void ir_print_value(ir_value* value, char* ir_output)
{
    if(!value) return;
    char buffer[64];

    switch(value->type) {
        case IR_NONE:
        sprintf(buffer, "(none) ");
        strcat(ir_output, buffer);
        break;

        case IR_LIT:
        sprintf(buffer, "%ld ", value->content.lit.i);
        strcat(ir_output, buffer);
        break;

        case IR_VAR:
        ir_print_var(value->content.var, ir_output);
    }
}

void ir_print_op(ir_op op, char* ir_output)
{
    char buffer[64];
    switch(op) {
        case IR_NO_OP:          sprintf(buffer, "(NO_OP)"); break;
        case IR_MINUS:          sprintf(buffer, "-");       break;
        case IR_LOGICAL_NOT:    sprintf(buffer, "!");       break;
        case IR_BINARY_NOT:     sprintf(buffer, "~");       break;
        case IR_CAST:           sprintf(buffer, "(cast) "); break;
        case IR_ADD:            sprintf(buffer, "+ ");      break;
        case IR_SUBTRACT:       sprintf(buffer, "- ");      break;
        case IR_MULTIPLY:       sprintf(buffer, "* ");      break;
        case IR_DIVIDE:         sprintf(buffer, "/ ");      break;
        case IR_LESSER:         sprintf(buffer, "< ");      break;
        case IR_LESSER_EQUAL:   sprintf(buffer, "<= ");     break;
        case IR_GREATER:        sprintf(buffer, "> ");      break;
        case IR_GREATER_EQUAL:  sprintf(buffer, ">= ");     break;
        case IR_EQUAL:          sprintf(buffer, "== ");     break;
        case IR_LOGICAL_AND:    sprintf(buffer, "&& ");     break;
        case IR_LOGICAL_OR:     sprintf(buffer, "|| ");     break;
        default:                sprintf(buffer, "?");       break;
    }
    strcat(ir_output, buffer);
}

void ir_print_instr(ir_insn* instr, char* ir_output)
{
    if(!instr) goto unknown;
    char buffer[128];
    if(instr->label) {
        sprintf(buffer, "%s:\n", instr->label);
        strcat(ir_output, buffer);
    }

    switch(instr->type) {
        case IR_NOP: 
        if(!instr->label) goto unknown;
        break;

        case IR_UN:
        ir_print_var(instr->content.un.result, ir_output);
        strcat(ir_output, "= ");
        ir_print_op(instr->content.un.op, ir_output);
        ir_print_value(instr->content.un.operand, ir_output);
        strcat(ir_output, "\n");
        break;

        case IR_BIN:
        ir_print_var(instr->content.bin.result, ir_output);
        strcat(ir_output, "= ");
        ir_print_value(instr->content.bin.left, ir_output);
        ir_print_op(instr->content.bin.op, ir_output);
        ir_print_value(instr->content.bin.right, ir_output);
        strcat(ir_output, "\n");
        break;

        case IR_COPY:
        ir_print_var(instr->content.copy.dst, ir_output);
        strcat(ir_output, "= ");
        ir_print_value(instr->content.copy.src, ir_output);
        strcat(ir_output, "\n");
        break;

        case IR_GOTO:
        sprintf(buffer, "goto %s\n", instr->content.jmp.dst);
        strcat(ir_output, buffer);
        break;

        case IR_IF:
        strcat(ir_output, "if ");
        ir_print_value(instr->content.condjmp.cond, ir_output);
        sprintf(buffer, "goto %s\n", instr->content.condjmp.if_true);
        strcat(ir_output, buffer);
        break;

        case IR_FN_CALL:
        if(instr->content.fn_call.result) {
            ir_print_var(instr->content.fn_call.result, ir_output);
            strcat(ir_output, "= ");
        }
        sprintf(buffer, "call %s\n", instr->content.fn_call.fn_label);
        strcat(ir_output, buffer);
        break;

        case IR_PROC_CALL:
        sprintf(buffer, "call %s\n", instr->content.proc_call.fn_label);
        strcat(ir_output, buffer);
        break;

        case IR_RETURN:
        strcat(ir_output, "return ");
        ir_print_value(instr->content.ret.value, ir_output);
        strcat(ir_output, "\n");
        break;

        case IR_ASSIGN_REF:
        ir_print_var(instr->content.assign_ref.dst, ir_output);
        strcat(ir_output, "= &");
        ir_print_value(instr->content.assign_ref.src, ir_output);
        strcat(ir_output, "\n");
        break;

        case IR_ASSIGN_DEREF:
        ir_print_var(instr->content.assign_deref.dst, ir_output);
        strcat(ir_output, "= *");
        ir_print_value(instr->content.assign_deref.src, ir_output);
        strcat(ir_output, "\n");
        break;

        case IR_DEREF_ASSIGN:
        strcat(ir_output, "*");
        ir_print_var(instr->content.deref_assign.dst, ir_output);
        strcat(ir_output, "= ");
        ir_print_value(instr->content.deref_assign.src, ir_output);
        strcat(ir_output, "\n");
        break;

        default: unknown:
        strcat(ir_output, "(invalid instruction)");
    }
}