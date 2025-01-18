#include <bits/posix2_lim.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <imperivm.h>
#include <util/alloc.h>
#include <frontend/lexer.h>
#include <frontend/parser.h>
#include <templates/set.h>
#include <templates/vector.h>

type_set(token);

ast_expr* parse_expr(int prec);
ast_stmt* parse_stmt(void);
int is_lvalue(ast_expr* e);

int current_token = 0;
ast_ctxt* current_ctxt = 0;
ast_fn* current_fn = 0;
vector_ast_fn* program = 0;
ast_stmt* root = 0;

token* peek(void) { return &tokens[current_token]; }
int is(token_type t) { return tokens[current_token].type == t; }

int match(token_type t) 
{ 
    if(tokens[current_token].type == t) { 
        current_token++; 
        return 1; 
    } 
    return 0; 
}

token* advance(void) 
{ 
    return &tokens[current_token++];
}

void expect(token_type t, char* err)
{
    if(tokens[current_token].type != t) 
        report_error(tokens[current_token].line, err);
    current_token++;
}

// Get a copy of the expression's type info. If it's NULL, return LONG_T.
type_info* expr_get_type(ast_expr* e)
{
    switch(e->type) {
        case EXPR_NONE: goto long_type;
        
        case EXPR_UNARY: {
            if(!e->content.un.type) goto long_type;
            type_info* type = malloc(sizeof(type_info));
            memcpy(type, e->content.un.type, sizeof(type_info));
            return type;
        }

        case EXPR_BINARY: {
            if(!e->content.bin.type) goto long_type;
            type_info* type = malloc(sizeof(type_info));
            memcpy(type, e->content.bin.type, sizeof(type_info));
            return type;
        }

        case EXPR_VARIABLE: {
            if(!e->content.var.type) goto long_type;
            type_info* type = malloc(sizeof(type_info));
            memcpy(type, e->content.var.type, sizeof(type_info));
            return type;
        }

        case EXPR_FN_CALL: {
            type_info* type = malloc(sizeof(type_info));
            memcpy(type, e->content.call.fn->ret_type, sizeof(type_info));
            return type;
        }

        case EXPR_LITERAL: goto long_type;
    }

    long_type:;
    type_info* type = malloc(sizeof(type_info));
    type->base = LONG_T;
    type->ptr_layers = 0;
    return type;
}

int get_op_prec(token_type type)
{
    switch(type) {
        case OR_OR: return 5;
        case AND_AND: return 6;
        case PLUS: case MINUS: return 10;
        case STAR: case SLASH: case PERCENT: return 20;
        case LESSER: case GREATER: case LESSER_EQUAL: case GREATER_EQUAL: return 30;
        case EQUAL_EQUAL: case NOT_EQUAL: return 40;
        case PLUS_PLUS: case MINUS_MINUS: return 50;
        case LPAREN: return 60;
        
        default: return 0;
    }
}

ast_op token_to_op(token_type type)
{
    switch(type) {
        case PLUS: return O_PLUS;
        case MINUS: return O_MINUS;
        case STAR: return O_TIMES;
        case SLASH: return O_BY;
        case GREATER: return O_GREATER;
        case GREATER_EQUAL: return O_GREATER_EQUAL;
        case LESSER: return O_LESSER;
        case LESSER_EQUAL: return O_LESSER_EQUAL;
        case EQUAL_EQUAL: return O_EQUIVALENT;
        case NOT_EQUAL: return O_NOT_EQUIVALENT;
        case AND_AND: return O_LOGICAL_AND;
        case OR_OR: return O_LOGICAL_OR;
        case NOT: return O_NOT;
        case AND: return O_BINARY_AND;
        case OR: return O_BINARY_OR;
        case PERCENT: return O_MOD;
        case XOR: return O_XOR;
        case GREATER_GREATER: return O_RSHIFT;
        case LESSER_LESSER: return O_LSHIFT;
        
        default:
        return O_NOP; // reserved code to let the calling function know it didn't find a matching operator
    }
}

// Returns 1 if the given token is right-associative, and 0 otherwise.
int is_right_asoc(token* t)
{
    return token_is(t, 5, EQUAL, PLUS_EQUAL, MINUS_EQUAL, STAR_EQUAL, SLASH_EQUAL);
}

// Parse literals, identifiers and expressions with prefix operators.
ast_expr* parse_nud(void)
{
    token* t = peek();
    ast_expr* e = calloc(1, sizeof(ast_expr));

    switch(t->type) {
        case NUMBER:
        advance();
        e->type = EXPR_LITERAL;
        e->content.lit.type = LIT_NUMBER;
        e->content.lit.content.number.content.ld = atoll(t->lexeme);
        e->content.lit.content.number.type = N_LONG;
        break;

        case IDENTIFIER:
        advance();
        if(match(LPAREN)) {
            // it's a function call
            e->type = EXPR_FN_CALL;
            e->content.call.fn = 0;
            e->content.call.args = vector_ast_expr_new();

            // first check if the function being called is declared
            for(int i = 0; i < program->n_values; i++)
                if(strcmp(program->values[i]->name, t->lexeme) == 0)
                    e->content.call.fn = program->values[i];
            if(!e->content.call.fn) report_error(t->line, "No such function");
            
            // then parse its arguments, if any
            while(!match(RPAREN)) {
                vector_ast_expr_add(e->content.call.args, parse_expr(0));
            }
        }
        else {
            // it's a variable
            e->type = EXPR_VARIABLE;

            // first check if the variable exists
            ast_var* decl = 0;
            for(int i = 0; i < current_ctxt->vars->n_values; i++) {
                if(strcmp(current_ctxt->vars->values[i]->name, t->lexeme) == 0) {
                    decl = current_ctxt->vars->values[i];
                    break;
                }
            }
            if(!decl) report_error(t->line, "No such variable in this context");

            e->content.var.name = strdup(t->lexeme);
            e->content.var.def_ctxt = decl->def_ctxt;
            e->content.var.type = decl->type;
        }
        break;

        case MINUS: {
            advance();
            ast_expr* operand = parse_expr(10);
            e->type = EXPR_UNARY;
            e->content.un.op = O_NEGATE;
            e->content.un.e = operand;
            e->content.un.type = expr_get_type(operand);
            break;
        }

        case STAR: {
            // dereference
            advance();
            ast_expr* operand = parse_expr(0);
            e->type = EXPR_UNARY;
            e->content.un.op = O_DEREFERENCE;
            e->content.un.e = operand;

            e->content.un.type = expr_get_type(operand);
            if(!e->content.un.type->ptr_layers) report_error(t->line, "Dereferencing a non-pointer");
            e->content.un.type->ptr_layers--;
            break;
        }

        case AND: {
            // address-of
            ast_expr* operand = parse_expr(0);
            if(!is_lvalue(operand)) report_error(t->line, "Cannot get address of non-lvalue");
            e->type = EXPR_UNARY;
            e->content.un.op = O_REFERENCE;
            e->content.un.e = operand;

            e->content.un.type = expr_get_type(operand);
            e->content.un.type->ptr_layers++;
            break;
        }

        case NOT: {
            // logical NOT
            ast_expr* operand = parse_expr(0);
            e->type = EXPR_UNARY;
            e->content.un.op = O_NOT;
            e->content.un.e = operand;
            break;
        }

        case PLUS_PLUS: 
        case MINUS_MINUS: {
            advance();
            ast_expr* operand = parse_expr(get_op_prec(t->type));
            e->type = EXPR_UNARY;
            e->content.un.e = operand;
            e->content.un.op = t->type == PLUS_PLUS ? O_INCREMENT : O_DECREMENT;
            break;
        }

        case LPAREN:
        advance();
        e = parse_expr(0);
        expect(RPAREN, "Expected closing parenthesis");
        break;

        case SEMICOLON:
        /*e->type = EXPR_LITERAL;
        e->content.lit.type = LIT_NUMBER;
        e->content.lit.content.number.type = N_LONG;
        e->content.lit.content.number.content.ld = 1;*/
        free(e);
        return 0;
        break;

        default: report_error(t->line, "Unexpected token");
    }

    return e;
}

// Parse expressions with infix operators.
ast_expr* parse_led(ast_expr* left)
{
    token* t;
    if(peek()->type == SEMICOLON) return left;
    t = advance();

    ast_expr* e = calloc(1, sizeof(ast_expr));

    e->type = EXPR_BINARY;
    e->content.bin.left = left;
    e->content.bin.op = token_to_op(t->type);

    int r_prec = get_op_prec(t->type); // right precedence
    if(is(SEMICOLON)) report_error(peek()->line, "Incomplete binary expression");
    e->content.bin.right = parse_expr(r_prec);
    e->content.bin.type = expr_get_type(left);

    return e;
}

// Parse an expression based on its precedence.
ast_expr* parse_expr(int prec)
{
    ast_expr* left = parse_nud();
    if(is(SEMICOLON)) return left;

    while(prec < get_op_prec(peek()->type)) left = parse_led(left);

    return left;
}

int is_lvalue(ast_expr* e)
{
    if(!e) return 0;

    switch(e->type) {
        case EXPR_NONE: return 0;
        case EXPR_UNARY: return is_lvalue(e->content.un.e);
        case EXPR_BINARY: 
        return is_lvalue(e->content.bin.left) || is_lvalue(e->content.bin.right);
        case EXPR_FN_CALL: return 0;
        case EXPR_LITERAL: return 0;
        case EXPR_VARIABLE: return 1;
    }

    return 0;
}

type_info* parse_type(void)
{
    // up to three base type specifiers are possible for each type
    token* t1 = 0;
    token* t2 = 0;
    token* t3 = 0;
    type_info* t = malloc(sizeof(type_info));

    if(token_is(peek(), 6, SIGNED, UNSIGNED, LONG, INT, CHAR, VOID)) t1 = advance();
    if(token_is(peek(), 6, SIGNED, UNSIGNED, LONG, INT, CHAR, VOID)) t2 = advance();
    if(token_is(peek(), 6, SIGNED, UNSIGNED, LONG, INT, CHAR, VOID)) t3 = advance();

    if(!t1) report_error(peek()->line, "Expected type");

    if(t1 && t2 && t3) {
        if(t1->type == SIGNED && t2->type == LONG && t3->type == INT) goto _signed_long;
        if(t1->type == UNSIGNED && t2->type == LONG && t3->type == INT) goto _unsigned_long;
        if(t1->type == SIGNED && t2->type == INT && t3->type == LONG) goto _signed_long;
        if(t1->type == UNSIGNED && t2->type == INT && t3->type == LONG) goto _unsigned_long;
        if(t1->type == INT && t2->type == LONG && t3->type == SIGNED) goto _signed_long;
        if(t1->type == INT && t2->type == LONG && t3->type == UNSIGNED) goto _unsigned_long;
        if(t1->type == LONG && t2->type == INT && t3->type == SIGNED) goto _signed_long;
        if(t1->type == LONG && t2->type == INT && t3->type == UNSIGNED) goto _unsigned_long;
        if(t1->type == INT && t2->type == SIGNED && t3->type == LONG) goto _signed_long;
        if(t1->type == INT && t2->type == UNSIGNED && t3->type == LONG) goto _unsigned_long;
        report_error(peek()->line, "Invalid type");
    }
    else if(t1 && t2) {
        if(t1->type == SIGNED && t2->type == LONG) goto _signed_long;
        if(t1->type == UNSIGNED && t2->type == LONG) goto _unsigned_long;
        if(t1->type == SIGNED && t2->type == INT) goto _signed_int;
        if(t1->type == UNSIGNED && t2->type == INT) goto _unsigned_int;
        if(t1->type == SIGNED && t2->type == CHAR) goto _signed_char;
        if(t1->type == UNSIGNED && t2->type == CHAR) goto _unsigned_char;
        if(t1->type == LONG && t2->type == INT) goto _signed_long;
        if(t1->type == INT && t2->type == LONG) goto _signed_long;
        if(t1->type == LONG && t2->type == SIGNED) goto _signed_long;
        if(t1->type == LONG && t2->type == UNSIGNED) goto _unsigned_long;
        if(t1->type == INT && t2->type == SIGNED) goto _signed_int;
        if(t1->type == INT && t2->type == UNSIGNED) goto _unsigned_int;
        if(t1->type == CHAR && t2->type == SIGNED) goto _signed_char;
        if(t1->type == CHAR && t2->type == UNSIGNED) goto _unsigned_char;
        report_error(peek()->line, "Invalid type");
    }
    else {
        if(t1->type == SIGNED) goto _signed_int;
        if(t1->type == UNSIGNED) goto _unsigned_int;
        if(t1->type == INT) goto _signed_int;
        if(t1->type == LONG) goto _signed_long;
        if(t1->type == CHAR) goto _signed_char;
        if(t1->type == VOID) goto _void;
    }

    _signed_long:    t->base = LONG_T;   goto ptrs;
    _unsigned_long:  t->base = ULONG_T;  goto ptrs;
    _signed_int:     t->base = INT_T;    goto ptrs;
    _unsigned_int:   t->base = UINT_T;   goto ptrs;
    _signed_char:    t->base = CHAR_T;   goto ptrs;
    _unsigned_char:  t->base = UCHAR_T;  goto ptrs;
    _void:           t->base = VOID_T;   goto ptrs;

    ptrs:
    t->ptr_layers = 0;
    while(match(STAR)) t->ptr_layers++;

    return t;
}

ast_stmt* parse_block(ast_ctxt* ctxt)
{
    ast_stmt* b = malloc(sizeof(ast_stmt));
    b->type = STMT_BLOCK;
    b->content.b.fn = current_fn;
    b->content.b.stmts = vector_ast_stmt_new();
    b->content.b.ctxt = ctxt;

    if(!ctxt) {
        ast_ctxt* new_ctxt = malloc(sizeof(ast_ctxt));
        new_ctxt->parent = current_ctxt;
        new_ctxt->vars = vector_ast_var_new();
        vector_ast_var_clone(new_ctxt->vars, current_ctxt->vars);
        b->content.b.ctxt = new_ctxt;
    }

    current_ctxt = b->content.b.ctxt;
    match(LBRACE);
    while(!match(RBRACE)) {
        vector_ast_stmt_add(b->content.b.stmts, parse_stmt());
    }
    current_ctxt = current_ctxt->parent;

    return b;
}

ast_stmt* parse_fn_decl(type_info* type, token* name)
{
    ast_fn* fwd_decl = 0; // set to the forward declaration, if it exists

    for(int i = 0; i < program->n_values; i++) {
        if(strcmp(program->values[i]->name, name->lexeme) == 0) {
            if(program->values[i]->body) {
                // redeclaration, throw a parse error
                char buffer[1024] = {0};
                sprintf(buffer, "Function %s already exists\n", name->lexeme);
                report_error(name->line, buffer);
            }
            else fwd_decl = program->values[i];
        }
    }

    // no need to allocate a new struct, just append the body to the forward declaration
    if(fwd_decl) {
        current_fn = fwd_decl;
        while(!is(LBRACE)) advance();
        advance(); // gobble up the `{`
        fwd_decl->body = &parse_block(fwd_decl->ctxt)->content.b;
        fwd_decl->body->fn = fwd_decl;
        current_fn = 0;

        // now I need a statement to return
        // the forward declaration created a stmt, just need to find it
        for(int i = 0; i < root->content.b.stmts->n_values; i++) {
            if(root->content.b.stmts->values[i]->type == STMT_FUNCTION
            && &(root->content.b.stmts->values[i]->content.fn) == fwd_decl) {
                return root->content.b.stmts->values[i];
            }
        }

        // this should be unreachable
        assert(0);

    }

    ast_stmt* fn = calloc(1, sizeof(ast_stmt));
    fn->type = STMT_FUNCTION;
    fn->content.fn.ret_type = type;
    fn->content.fn.rets = vector_ast_ret_new();
    fn->content.fn.ctxt = malloc(sizeof(ast_ctxt));
    fn->content.fn.name = strdup(name->lexeme);
    current_fn = &fn->content.fn;
    fn->content.fn.ctxt->parent = root->content.b.ctxt;
    fn->content.fn.ctxt->vars = vector_ast_var_new();
    vector_ast_var_clone(fn->content.fn.ctxt->vars, root->content.b.ctxt->vars);
    fn->content.fn.params = vector_ast_var_new();
    vector_ast_fn_add(program, &fn->content.fn);

    match(LPAREN);
    while(!match(RPAREN)) {
        match(COMMA); // matters after the first parameter is parsed
        type_info* type = parse_type();
        if(type->base == VOID_T && !type->ptr_layers) {
            // sometype somefn(void)
            if(tokens[current_token-2].type != LPAREN)
                report_error(peek()->line, "Expected void to be the only type in parameter list");
            expect(RPAREN, "Expected void to be the only type in parameter list");
            break;
        }
        if(!is(IDENTIFIER)) 
            report_error(peek()->line, "Expected identifier after parameter type");
        
        ast_var* param = malloc(sizeof(ast_var));
        param->type = type;
        param->name = strdup(peek()->lexeme);
        param->def_ctxt = fn->content.fn.ctxt;
        advance(); // consume the name

        int shadowed = 0;
        for(int i = 0; i < fn->content.fn.ctxt->vars->n_values; i++) {
            if(strcmp(fn->content.fn.ctxt->vars->values[i]->name, param->name) == 0) {
                shadowed = 1;
                fn->content.fn.ctxt->vars->values[i] = param;
                break;
            }
        }
        if(!shadowed) vector_ast_var_add(fn->content.fn.ctxt->vars, param);
    }

    // all parameters are parsed, save them in the function itself
    vector_ast_var_clone(fn->content.fn.params, fn->content.fn.ctxt->vars);
    
    // now, since the function's context also contains global variables,
    // they need to be pruned from the param vector
    for(int i = 0; i < fn->content.fn.params->n_values; i++)
        if(vector_ast_var_contains(root->content.b.ctxt->vars, fn->content.fn.params->values[i]))
            vector_ast_var_remove(fn->content.fn.params, i);

    if(is(LBRACE)) {
        // parse the function body
        fn->content.fn.body = &parse_block(fn->content.fn.ctxt)->content.b;
    }
    else expect(SEMICOLON, "Expected semicolon after function forward declaration");

    current_fn = 0;
    return fn;
}

ast_expr* parse_var_decl(type_info* type, token* name)
{
    ast_expr* var = calloc(1, sizeof(ast_expr));
    var->type = EXPR_VARIABLE;
    expect(EQUAL, "Expected initializer for variable declaration");

    if(is(SEMICOLON)) report_error(peek()->line, "Empty initializer for variable declaration");
    ast_expr* init = parse_expr(0);
    var->content.var.name = strdup(name->lexeme);
    var->content.var.type = type;
    var->content.var.value = init;
    var->content.var.def_ctxt = current_ctxt;
    
    // go through all variables in this context and check if this is a redeclaration (not merely a shadowing)
    for(int i = 0; i < current_ctxt->vars->n_values; i++)
        if(strcmp(current_ctxt->vars->values[i]->name, var->content.var.name) == 0)
            if(var_def_ctxt(current_ctxt->vars->values[i]->name, current_ctxt) == current_ctxt)
                report_error(name->line, "Variable redeclaration");
    
    vector_ast_var_add(current_ctxt->vars, &var->content.var);

    expect(SEMICOLON, "Expected semicolon after variable declaration");
    return var;
}

ast_if parse_if(void)
{
    ast_if if_stmt = {0};

    expect(LPAREN, "Expected open parenthesis after if");
    if_stmt.cond = parse_expr(0);
    if(!if_stmt.cond) report_error(peek()->line, "Expected condition");
    expect(RPAREN, "Expected closing parenthesis after if condition");

    if_stmt.if_true = parse_stmt();
    if(match(ELSE)) if_stmt.if_false = parse_stmt();

    return if_stmt;
}

ast_while parse_while(void)
{
    ast_while while_stmt = {0};

    expect(LPAREN, "Expected open parenthesis after while");
    while_stmt.cond = parse_expr(0);
    if(!while_stmt.cond) report_error(peek()->line, "Expected condition");
    expect(RPAREN, "Expected closing parenthesis after while condition");

    while_stmt.body = parse_stmt();

    return while_stmt;
}

ast_stmt* parse_stmt(void)
{
    ast_stmt* s = calloc(1, sizeof(ast_stmt));
    token* t = peek();

    switch(t->type) {
        case RETURN:
        advance();
        s->type = STMT_RETURN;
        s->content.ret.fn = current_fn;
        s->content.ret.is_last = 1;
        s->content.ret.var = parse_expr(0);
        if(!s->content.ret.var) {
            // no expr returned, check if the fn is void
            if(current_fn->ret_type->base != VOID_T || (current_fn->ret_type->base == VOID_T && current_fn->ret_type->ptr_layers))
                report_error(t->line, "Empty return statement in non-void function");
        }
        // an expr is returned, check if fn is non-void
        else if(current_fn->ret_type->base == VOID_T && !current_fn->ret_type->ptr_layers)
            report_error(t->line, "Void function returns a value");
        
        for(int i = 0; i < current_fn->rets->n_values; i++) 
            current_fn->rets->values[i]->is_last = 0;
        vector_ast_ret_add(current_fn->rets, &s->content.ret);

        expect(SEMICOLON, "Expected semicolon after return statement");
        break;

        case IDENTIFIER:;
        // assignment or stmt_expr
        // either way there's an expression on the lhs
        ast_expr* e = parse_expr(0);

        if(match(EQUAL)) {
            if(!is_lvalue(e)) 
                report_error(peek()->line, "Cannot assign to a non-lvalue");
            
            s->type = STMT_COPY;
            s->content.copy.dst = e;
            s->content.copy.src = parse_expr(0);

            if(!s->content.copy.src) 
                report_error(peek()->line, "Empty expression in assignment");
        }
        else {
            s->type = STMT_EXPR;
            s->content.expr = e;
        }
        expect(SEMICOLON, "Expected semicolon after statement");
        break;

        case IF:
        advance();
        s->type = STMT_IF;
        s->content.if_stmt = parse_if();
        break;

        case WHILE:
        advance();
        s->type = STMT_WHILE;
        s->content.while_stmt = parse_while();
        break;

        case LBRACE:
        s = parse_block(0);
        break;

        case INT:
        case CHAR:
        case LONG:
        case SIGNED:
        case UNSIGNED:
        case VOID:;
        type_info* type = parse_type();

        // either a function decl or a variable decl
        // either way it should have an identifier now
        if(!is(IDENTIFIER)) report_error(peek()->line, "Expected identifier after type");
        token* name = advance();

        if(is(LPAREN)) {
            free(s);
            s = parse_fn_decl(type, name);
        }

        else {
            s->type = STMT_DECL;
            s->content.expr = parse_var_decl(type, name);
        }
        break;

        default:
        // try stuff
        if(peek()->type == STAR) {
            // either stmt_copy (*a = 0) or stmt_expr (*a++ for instance)
            ast_expr* lhs = parse_expr(0);
            
            if(match(EQUAL)) {
                s->type = STMT_DEREF;
                s->content.copy.dst = lhs;
                s->content.copy.src = parse_expr(0);
                if(!s->content.copy.src) report_error(peek()->line, "Expected expression on assignment rhs");
                expect(SEMICOLON, "Expected semicolon after statement");
                break;
            }

            s->type = STMT_DEREF;
            s->content.expr = lhs;
            expect(SEMICOLON, "Expected semicolon after statement");
            break;
        }

        printf("%s\n", peek()->lexeme);
        report_error(peek()->line, "what?");
    }

    return s;
}

void parse(void)
{
    program = vector_ast_fn_new();
    root = malloc(sizeof(ast_stmt));
    root->type = STMT_BLOCK;
    root->content.b.ctxt = malloc(sizeof(ast_ctxt));
    root->content.b.ctxt->parent = 0;
    root->content.b.ctxt->vars = vector_ast_var_new();
    root->content.b.fn = 0;
    root->content.b.stmts = vector_ast_stmt_new();
    current_ctxt = root->content.b.ctxt;
    
    while(!is(END)) {
        vector_ast_stmt_add(root->content.b.stmts, parse_stmt());
    }
}

// returns the context the given var was defined in;
// works just fine with shadowed variables as well
ast_ctxt* var_def_ctxt(char* var_name, ast_ctxt* start)
{
    ast_ctxt* highest = start;
    while(start) {
        for(int i = 0; i < start->vars->n_values; i++) {
            ast_var* var = start->vars->values[i];
            if(var->name == var_name) highest = start;
        }
        start = start->parent;
    }

    return highest;
}
