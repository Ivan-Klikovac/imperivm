#include <assert.h>
#include <IR/IR.h>
#include <IR/IR_optimize.h>
#include <backend/amd64/amd64.h>
#include <backend/amd64/amd64_asm.h>
#include <templates/vector.h>

#define FN() ;//if(verbose_asm) printf("%s\n", __func__)

// assumes var has its register; spills the previous reg value if needed and loads this
void ensure_reg(ir_var* var)
{
    int reg = get_reg(var);
    if(!check_reg(var)) {
        amd64_spill(reg, reg_status[reg]);
        amd64_load(reg, var);
        reg_status[reg] = var;
    }
}

// stores a value from reg into var (either in memory or in its reg) while doing housekeeping
void amd64_store(ir_var* var, int reg)
{
    if(has_reg(var)) {
        int var_reg = get_reg(var);
        if(!check_reg(var)) amd64_spill(var_reg, reg_status[var_reg]);
        amd64_mov(var_reg, reg);
        reg_status[var_reg] = var;
    }
    else amd64_spill(reg, var);
}

int stackframe_find(ir_var* var)
{
    var_vector* local_vars = stack_status->values[stack_status->n_values-1];
    for(int i = 0; i < local_vars->n_values; i++) 
        if(local_vars->values[i] && strcmp(var->name, local_vars->values[i]->name) == 0)
            return i;
    
    assert(1);
    return -1;
}

// this must be at the start of every function
void amd64_prologue(void)
{
    // save old rbp
    amd64_push(RBP);
    amd64_mov(RBP, RSP);

    // if the fn uses RBX, R12, R13, R14 or R15, I need to save them on the stack first
    // to be safe, I'll just push them regardless
    amd64_push(RBX);
    amd64_push(R12);
    amd64_push(R13);
    amd64_push(R14);
    amd64_push(R15);

    // allocate space for params and locals
    var_vector* params = symtable_get();
    var_vector* local_vars = ir_get_local_vars(amd64_current_fn->name);
    amd64_sub_ri(RSP, (min(params->n_values, 6) + local_vars->n_values) * 8);

    // then update the internal compiler state to reflect this
    // first build the stack frame
    stackframe_build();

    // the stackframe will be as follows:
    // arguments passed through the stack (n >= 0)
    // return address
    // stack space for the register params (0 <= n <= 6)
    // stack space for local variables

    // add the stack arguments
    for(int i = params->n_values - 1; i > 5; i--) stackframe_add(params->values[i]);
    // then the return address, or rather a dummy in its place
    stackframe_add(ir_dummy_var());
    // then the reg args
    for(int i = 0; i < min(params->n_values, 6); i++) stackframe_add(params->values[i]);
    // and finally the local variables
    for(int i = 0; i < local_vars->n_values; i++) stackframe_add(local_vars->values[i]);
    vector_ir_var_free(local_vars);

    // put the register-passed arguments on the stack in their parameter slots
    // this is terribly unoptimized but it works
    // otherwise I'd have to adjust register coloring to take sysv into account (cba)
    switch(min(params->n_values, 6)) {
        case 6: amd64_spill(R9, params->values[5]);
        case 5: amd64_spill(R8, params->values[4]);
        case 4: amd64_spill(RCX, params->values[3]);
        case 3: amd64_spill(RDX, params->values[2]);
        case 2: amd64_spill(RSI, params->values[1]);
        case 1: amd64_spill(RDI, params->values[0]);
    }
}

// this must be at the end of every function
void amd64_epilogue(void)
{
    // deallocate local vars to clean up the stack
    // so add the number of variables after the dummy return value
    int n = 0;
    for(int i = 0; i < stackframe_top->n_values; i++) {
        if(strcmp(stackframe_top->values[i]->name, "dummy") == 0) {
            n = stackframe_top->n_values - (i + 1);
            break;
        }
    }
    amd64_add_ri(RSP, n * 8);

    amd64_pop(R15);
    amd64_pop(R14);
    amd64_pop(R13);
    amd64_pop(R12);
    amd64_pop(RBX);

    amd64_mov(RSP, RBP);
    amd64_pop(RBP);
    amd64_ret();
}

// fix this later
void amd64_fn_call(ir_fn_call* call)
{
    if(!call->args->n_values) goto call;

    int i = call->args->n_values - 1;

    push_arg:;
    ir_value* arg = call->args->values[i];
    if(i >= 6 && i--) amd64_push(arg);
    switch(i) {
        case 5: amd64_mov(R9, arg);
        case 4: amd64_mov(R8, arg);
        case 3: amd64_mov(RCX, arg);
        case 2: amd64_mov(RDX, arg);
        case 1: amd64_mov(RSI, arg);
        case 0: amd64_mov(RDI, arg);
        break;

        default: goto push_arg;
    }

    call:
    amd64_call(call->fn_label + 3); // go past `fn.`
    if(call->result) amd64_spill(RAX, call->result);
}

void amd64_proc_call(ir_proc_call* call)
{
    if(!call->args->n_values) goto call;

    int i = call->args->n_values - 1;

    push_arg:;
    ir_value* arg = call->args->values[i];
    if(i >= 6 && i--) amd64_push(arg);
    switch(i) {
        case 5: amd64_mov(R9, arg);
        case 4: amd64_mov(R8, arg);
        case 3: amd64_mov(RCX, arg);
        case 2: amd64_mov(RDX, arg);
        case 1: amd64_mov(RSI, arg);
        case 0: amd64_mov(RDI, arg);
        break;

        default: goto push_arg;
    }
    
    call:
    amd64_call(call->fn_label + 3);
}

void amd64_un_rr(ir_un* un)
{
    FN();
    ir_var* operand = un->operand->content.var;
    ir_var* result = un->result;

    int reg_result = get_reg(result);
    if(!check_reg(result)) amd64_spill(reg_result, reg_status[reg_result]);

    // move the operand into the result register, and do the calculation there
    if(check_reg(operand)) amd64_mov(reg_result, get_reg(operand));
    else amd64_load(reg_result, operand);

    switch(un->op) {
        case IR_MINUS:
        amd64_neg(reg_result);
        break;

        case IR_BINARY_NOT:
        amd64_not(reg_result);
        break;

        case IR_LOGICAL_NOT:
        amd64_logical_not_r(reg_result);
        break;

        case IR_DEREFERENCE:
        amd64_mov(R15, reg_result);
        amd64_deref_mov_rr(reg_result, R15);
        break;

        default: break;
    }

    reg_status[reg_result] = result;
}

// the operand is stored in a register, and the result can be a register or memory location
void amd64_un_mr(ir_un* un)
{
    FN();
    ir_var* operand = un->operand->content.var;
    ir_var* result = un->result;
    
    int reg_operand = get_reg(operand);
    ensure_reg(operand);

    switch(un->op) {
        case IR_MINUS:       
        amd64_neg(reg_operand);
        break;
        
        case IR_BINARY_NOT:
        amd64_not(reg_operand);
        break;
        
        case IR_LOGICAL_NOT:
        amd64_logical_not_r(reg_operand);
        break;

        case IR_DEREFERENCE:
        amd64_deref_mov_mr(result, reg_operand);
        break;
        
        default: break;
    }
    reg_status[reg_operand] = 0; // the value in the register changed

    // now move it into the result register
    // potential optimization here
    amd64_spill(reg_operand, result);
}

// the operand can be either a memory location or immediate value
void amd64_un_rm(ir_un* un)
{
    FN();
    ir_var* result = un->result;
    int reg_result = get_reg(result);
    if(!check_reg(result)) amd64_spill(reg_result, reg_status[reg_result]);
    
    amd64_load(reg_result, un->operand);

    switch(un->op) {
        case IR_MINUS:
        amd64_neg(reg_result);
        break;

        case IR_BINARY_NOT:
        amd64_not(reg_result);
        break;

        case IR_LOGICAL_NOT:
        amd64_logical_not_r(reg_result);
        break;

        case IR_DEREFERENCE:
        amd64_mov(R15, reg_result);
        amd64_deref_mov_rr(reg_result, R15);
        break;

        default: break;
    }

    reg_status[reg_result] = result;
}

void amd64_un_mm(ir_un* un)
{
    FN();
    ir_var* result = un->result;

    amd64_load(13, un->operand);

    switch(un->op) {
        case IR_MINUS:       amd64_neg(13);           break;
        case IR_BINARY_NOT:  amd64_not(13);           break;
        case IR_LOGICAL_NOT: amd64_logical_not_r(13); break;

        case IR_DEREFERENCE:
        amd64_load(R15, un->operand);
        amd64_deref_mov_mr(un->result, R15);
        break;

        default: break;
    }

    amd64_spill(13, result);
}

void amd64_bin_rrr(ir_bin* bin)
{
    FN();
    ir_var* result = bin->result;
    ir_var* left = bin->left->content.var;
    ir_var* right = bin->right->content.var;
    int reg_left = get_reg(left);
    int reg_right = get_reg(right);
    int reg_result = get_reg(result);

    ensure_reg(left);
    ensure_reg(right);
    if(!check_reg(result)) amd64_spill(reg_result, reg_status[reg_result]);

    switch(bin->op) {
        case IR_SUBTRACT:
        // I want: result = left - right
        // sub(dst, src) is dst = dst - src
        // so I need to move left into reg_result and sub(result, right)
        amd64_mov(reg_result, reg_left);
        amd64_sub_rr(reg_result, reg_right);
        break;

        case IR_ADD:
        amd64_mov(reg_result, reg_left);
        amd64_add_rr(reg_result, reg_right);
        break;

        case IR_MULTIPLY:
        amd64_mov(reg_result, reg_left);
        amd64_imul_rr(reg_result, reg_right);
        break;

        case IR_LESSER:
        // for example, result in rax, left in rbx, right in rcx
        // cmp %rcx, %rbx (pay attention to operand ordering, a > b = cmp b, a)
        // setl %al (set if less)
        // movzbq %al, %rax (move with zero-extend from byte to quad)
        amd64_cmp_rr(reg_left, reg_right);
        amd64_setl_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_LESSER_EQUAL:
        amd64_cmp_rr(reg_left, reg_right);
        amd64_setle_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_GREATER:
        amd64_cmp_rr(reg_left, reg_right);
        amd64_setg_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_GREATER_EQUAL:
        amd64_cmp_rr(reg_left, reg_right);
        amd64_setge_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_EQUAL:
        amd64_cmp_rr(reg_left, reg_right);
        amd64_sete_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_NOT_EQUAL:
        amd64_cmp_rr(reg_left, reg_right);
        amd64_setne_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        default: break;
    }

    reg_status[reg_result] = result;
}

void amd64_bin_mrr(ir_bin* bin)
{
    FN();
    ir_var* result = bin->result;
    ir_var* left = bin->left->content.var;
    ir_var* right = bin->right->content.var;
    int reg_left = get_reg(left);
    int reg_right = get_reg(right);

    ensure_reg(left);
    ensure_reg(right);

    switch(bin->op) {
        case IR_SUBTRACT:
        amd64_sub_rr(reg_left, reg_right);
        reg_status[reg_left] = 0;
        amd64_spill(reg_left, result);
        break;

        case IR_ADD:
        amd64_add_rr(reg_left, reg_right);
        reg_status[reg_left] = 0;
        amd64_spill(reg_left, result);
        break;

        case IR_MULTIPLY:
        amd64_imul_rr(reg_left, reg_right);
        amd64_spill(reg_left, result);
        reg_status[reg_left] = 0;
        break;

        case IR_LESSER:
        amd64_cmp_rr(reg_left, reg_right);
        amd64_setl_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        case IR_LESSER_EQUAL:
        amd64_cmp_rr(reg_left, reg_right);
        amd64_setle_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        case IR_GREATER:
        amd64_cmp_rr(reg_left, reg_right);
        amd64_setg_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        case IR_GREATER_EQUAL:
        amd64_cmp_rr(reg_left, reg_right);
        amd64_setge_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        case IR_EQUAL:
        amd64_cmp_rr(reg_left, reg_right);
        amd64_sete_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        case IR_NOT_EQUAL:
        amd64_cmp_rr(reg_left, reg_right);
        amd64_setne_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        default: break;
    }
}

void amd64_bin_rrm(ir_bin* bin)
{
    ir_var* result = bin->result;
    ir_var* left = bin->left->content.var;
    int reg_result = get_reg(result);
    int reg_left = get_reg(left);

    ensure_reg(left);
    if(!check_reg(result)) amd64_spill(reg_result, reg_status[reg_result]);

    switch(bin->op) {
        case IR_SUBTRACT:
        // result = left - right
        // sub(left, right): left = left - right
        // mov left -> result
        amd64_mov(reg_result, reg_left);
        amd64_sub_rv(reg_result, bin->right);
        break;

        case IR_ADD:
        amd64_mov(reg_result, reg_left);
        amd64_add_rv(reg_result, bin->right);
        break;

        case IR_MULTIPLY:
        // register-register-memory multiplication can't be done in a single instruction
        // so move the register operand into the result register (to avoid clobbering the operand register)
        amd64_mov(reg_result, reg_left);
        amd64_imul_rv(reg_result, bin->right);
        break;

        case IR_LESSER:
        amd64_cmp_rv(reg_left, bin->right);
        amd64_setl_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_LESSER_EQUAL:
        amd64_cmp_rv(reg_left, bin->right);
        amd64_setle_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_GREATER:
        amd64_cmp_rv(reg_left, bin->right);
        amd64_setg_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_GREATER_EQUAL:
        amd64_cmp_rv(reg_left, bin->right);
        amd64_setge_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_EQUAL:
        amd64_cmp_rv(reg_left, bin->right);
        amd64_sete_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_NOT_EQUAL:
        amd64_cmp_rv(reg_left, bin->right);
        amd64_setne_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        default: break;
    }

    reg_status[reg_result] = result;
}

void amd64_bin_mrm(ir_bin* bin)
{
    FN();
    ir_var* result = bin->result;
    ir_var* left = bin->left->content.var;
    int reg_left = get_reg(left);
    
    ensure_reg(left);

    switch(bin->op) {
        case IR_SUBTRACT:
        amd64_sub_rv(reg_left, bin->right);
        amd64_spill(reg_left, result);
        reg_status[reg_left] = 0;
        break;

        case IR_ADD:
        amd64_add_rv(reg_left, bin->right);
        amd64_spill(reg_left, result);
        reg_status[reg_left] = 0;
        break;

        case IR_MULTIPLY:
        amd64_imul_rv(reg_left, bin->right);
        amd64_spill(reg_left, result);
        reg_status[reg_left] = 0;
        break;

        case IR_LESSER:
        amd64_cmp_rv(reg_left, bin->right);
        amd64_setl_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        case IR_LESSER_EQUAL:
        amd64_cmp_rv(reg_left, bin->right);
        amd64_setle_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        case IR_GREATER:
        amd64_cmp_rv(reg_left, bin->right);
        amd64_setg_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        case IR_GREATER_EQUAL:
        amd64_cmp_rv(reg_left, bin->right);
        amd64_setge_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        case IR_EQUAL:
        amd64_cmp_rv(reg_left, bin->right);
        amd64_sete_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        case IR_NOT_EQUAL:
        amd64_cmp_rv(reg_left, bin->right);
        amd64_setne_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        default: break;
    }
}

void amd64_bin_rmr(ir_bin* bin)
{
    FN();
    ir_var* result = bin->result;
    ir_var* right = bin->right->content.var;
    int reg_result = get_reg(result);
    int reg_right = get_reg(right);

    ensure_reg(right);
    if(!check_reg(result)) amd64_spill(reg_result, reg_status[reg_result]);

    switch(bin->op) {
        case IR_SUBTRACT:
        amd64_load(reg_result, bin->left);
        amd64_sub_rr(reg_result, reg_right);
        break;

        case IR_ADD:
        amd64_load(reg_result, bin->left);
        amd64_add_rr(reg_result, reg_right);
        break;

        case IR_MULTIPLY:
        amd64_load(reg_result, bin->left);
        amd64_imul_rr(reg_result, reg_right);
        break;

        case IR_LESSER:
        // comparing right with left instead of left with right, and flipping the set insn
        // lesser becomes greater or equal
        amd64_cmp_rv(reg_right, bin->left);
        amd64_setge_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_LESSER_EQUAL:
        amd64_cmp_rv(reg_right, bin->left);
        amd64_setg_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_GREATER:
        amd64_cmp_rv(reg_right, bin->left);
        amd64_setle_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_GREATER_EQUAL:
        amd64_cmp_rv(reg_right, bin->left);
        amd64_setl_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_EQUAL:
        amd64_cmp_rv(reg_right, bin->left);
        amd64_sete_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_NOT_EQUAL:
        amd64_cmp_rv(reg_right, bin->left);
        amd64_setne_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        default: break;
    }

    reg_status[reg_result] = result;
}

void amd64_bin_mmr(ir_bin* bin)
{
    FN();
    ir_var* result = bin->result;
    ir_var* right = bin->right->content.var;
    int reg_right = get_reg(right);

    ensure_reg(right);

    switch(bin->op) {
        case IR_SUBTRACT:
        // left - right = -(right - left)
        amd64_neg(reg_right);
        amd64_sub_rv(reg_right, bin->left);
        amd64_spill(reg_right, result);
        reg_status[reg_right] = 0;
        break;

        case IR_ADD:
        amd64_add_rv(reg_right, bin->left);
        amd64_spill(reg_right, result);
        reg_status[reg_right] = 0;
        break;

        case IR_MULTIPLY:
        amd64_imul_rv(reg_right, bin->left);
        amd64_spill(reg_right, result);
        reg_status[reg_right] = 0;
        break;

        case IR_LESSER:
        amd64_cmp_rv(reg_right, bin->left);
        amd64_setge_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        case IR_LESSER_EQUAL:
        amd64_cmp_rv(reg_right, bin->left);
        amd64_setg_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        case IR_GREATER:
        amd64_cmp_rv(reg_right, bin->left);
        amd64_setle_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        case IR_GREATER_EQUAL:
        amd64_cmp_rv(reg_right, bin->left);
        amd64_setl_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        case IR_EQUAL:
        amd64_cmp_rv(reg_right, bin->left);
        amd64_sete_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;

        case IR_NOT_EQUAL:
        amd64_cmp_rv(reg_right, bin->left);
        amd64_setne_r(R15);
        amd64_movzbq_r(R15);
        amd64_spill(R15, result);
        break;


        default: break;
    }
}

void amd64_bin_mmm(ir_bin* bin)
{
    FN();
    ir_var* result = bin->result;
    amd64_load(R15, bin->left);

    switch(bin->op) {
        case IR_SUBTRACT:
        amd64_sub_rv(R15, bin->right);
        break;

        case IR_ADD:
        amd64_add_rv(R15, bin->right);
        break;

        case IR_MULTIPLY:
        amd64_imul_rv(R15, bin->right);
        break;

        case IR_LESSER:
        amd64_cmp_rv(R15, bin->right);
        amd64_setl_r(R15);
        amd64_movzbq_r(R15);
        break;

        case IR_LESSER_EQUAL:
        amd64_cmp_rv(R15, bin->right);
        amd64_setg_r(R15);
        amd64_movzbq_r(R15);
        break;

        case IR_GREATER:
        amd64_cmp_rv(R15, bin->right);
        amd64_setle_r(R15);
        amd64_movzbq_r(R15);
        break;

        case IR_GREATER_EQUAL:
        amd64_cmp_rv(R15, bin->right);
        amd64_setl_r(R15);
        amd64_movzbq_r(R15);
        break;

        case IR_EQUAL:
        amd64_cmp_rv(R15, bin->right);
        amd64_sete_r(R15);
        amd64_movzbq_r(R15);
        break;

        case IR_NOT_EQUAL:
        amd64_cmp_rv(R15, bin->right);
        amd64_setne_r(R15);
        amd64_movzbq_r(R15);
        break;

        default: break;
    }

    amd64_spill(R15, result);
}

void amd64_bin_rmm(ir_bin* bin)
{
    FN();
    ir_var* result = bin->result;
    int reg_result = get_reg(result);
    if(!check_reg(result)) amd64_spill(reg_result, reg_status[reg_result]);
    amd64_load(reg_result, bin->left);

    switch(bin->op) {
        case IR_SUBTRACT:
        amd64_sub_rv(reg_result, bin->right);
        break;

        case IR_ADD:
        amd64_add_rv(reg_result, bin->right);
        break;

        case IR_MULTIPLY:
        amd64_imul_rv(reg_result, bin->right);
        break;

        case IR_LESSER:
        amd64_cmp_rv(reg_result, bin->right);
        amd64_setl_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_GREATER:
        amd64_cmp_rv(reg_result, bin->right);
        amd64_setg_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_LESSER_EQUAL:
        amd64_cmp_rv(reg_result, bin->right);
        amd64_setle_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_GREATER_EQUAL:
        amd64_cmp_rv(reg_result, bin->right);
        amd64_setge_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_EQUAL:
        amd64_cmp_rv(reg_result, bin->right);
        amd64_sete_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        case IR_NOT_EQUAL:
        amd64_cmp_rv(reg_result, bin->right);
        amd64_setne_r(reg_result);
        amd64_movzbq_r(reg_result);
        break;

        default: break;
    }

    reg_status[reg_result] = result;
}

void amd64_copy_xr(ir_copy* copy)
{
    FN();
    ir_var* dst = copy->dst;
    ir_var* src = copy->src->content.var;
    int reg_src = get_reg(src);
    ensure_reg(src);

    amd64_store(dst, reg_src);
}

// dst has its register, and src is either a memory operand without its register or a literal
void amd64_copy_rv(ir_copy* copy)
{
    FN();
    ir_var* dst = copy->dst;
    int dst_reg = get_reg(dst);
    if(!check_reg(dst)) amd64_spill(dst_reg, reg_status[dst_reg]);
    amd64_load(dst_reg, copy->src);
    reg_status[dst_reg] = dst;
}

void amd64_copy_mm(ir_copy* copy)
{
    FN();
    amd64_load(13, copy->src);
    amd64_spill(13, copy->dst);
}

void amd64_deref_assign(ir_deref_assign* insn)
{
    if(insn->src->type == IR_VAR && has_reg(insn->src->content.var) && check_reg(insn->src->content.var)) 
        amd64_spill(get_reg(insn->src->content.var), insn->src->content.var);
    
    if(has_reg(insn->dst)) {
        if(!check_reg(insn->dst)) amd64_load(R15, insn->dst);
        else amd64_mov(R15, get_reg(insn->dst));        
    }
    else amd64_load(R15, insn->dst);

    amd64_copy_through_ptr_rv(R15, insn->src);
}

void amd64_condjmp(ir_if* condjmp)
{
    FN();
    int cond_reg = 13;

    if(condjmp->cond->type == IR_LIT) 
        amd64_load(cond_reg, condjmp->cond->content.lit.i);
    else {
        ir_var* cond = condjmp->cond->content.var;
        if(has_reg(cond)) {
            cond_reg = get_reg(cond);
            ensure_reg(cond);
        }
        else amd64_load(cond_reg, cond);
    }
    
    amd64_test_rr(cond_reg, cond_reg);
    amd64_jnz_r(condjmp->if_true);

    //if(condjmp->if_false) {
    //    asm_add("__if_false\n");
    //}
}

void amd64_exit(ir_return* ret)
{
    asm_add("movq $60, %%rax\n");
    if(ret->value) {
        if(ret->value->type == IR_VAR && has_reg(ret->value->content.var) && check_reg(ret->value->content.var)) 
            amd64_mov(5, get_reg(ret->value->content.var));
        else amd64_load(5, ret->value);
    }
    else amd64_load_lit(5, 0);

    asm_add("syscall\n");
}