#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <imperivm.h>
#include <frontend/lexer.h>
#include <IR/IR.h>
#include <IR/IR_print.h>
#include <backend/amd64/amd64.h>

char* ir_out = 0;
int verbose_asm = 0;
int print_blocks = 0;

void __attribute__((noreturn)) no_mem(char* fn, char* file, int line)
{
    printf("%s (%s:%d): malloc failed\n", fn, file, line);
    exit(1);
}

void __attribute__((noreturn)) ir_exit(void)
{
    printf("IR: %ld\n", ir->n_values);
    char* mem = calloc(1, 65536);
    for(int i = 0; i < ir->n_values; i++) ir_print_instr((ir_insn*) ir->values[i], mem + 128 * i);
    for(int i = 0; i < ir->n_values; i++) printf("%s", mem + 128 * i);
    putchar('\n');
    exit(1);
}

void __attribute__((noreturn)) report_error(int line, char* message)
{
    report(line, NULL, message);
    //printf("\n\nAST so far:\n");
    //print_ast();
    exit(1);
}

void report(int line, char* code, char* message)
{    
    if(code) {
        // only print one line of code
        char buffer[1024] = {0}; // assume lines aren't longer than 1024 characters
        char* ptr = strchr(code, '\n');
        if(!ptr) ptr = strchr(code, 0);
        strncpy(buffer, code, ptr - code);
        printf("%s\n%d | %s\n\n", message, line, buffer);
        return;
    }

    printf("%d | %s\n\n", line, message);
}

int main(int argc, char* argv[])
{
    if(argc > 2 && strncmp(argv[2], "--ir=", 5) == 0) {
        ir_out = argv[2] + 5;
        fclose(fopen(ir_out, "w")); // reset the file
    }
    if(argc > 2 && (strcmp(argv[2], "--verbose-asm") == 0 || (argv[3] && strcmp(argv[3], "--verbose-asm") == 0))) 
        verbose_asm = 1;

    if(verbose_asm && argv[3] && strcmp("--print-blocks", argv[3]) == 0) print_blocks = 1;

    FILE* file = fopen(argv[1], "rb");
    if(!file) goto bad_file;
    
    fseek(file, 0, SEEK_END);
    uint64_t fsize = ftell(file);
    rewind(file);
    
    char* src = malloc(fsize+1);
    src[fsize] = 0;
    uint64_t read = fread(src, 1, fsize, file);
    if(read != fsize) goto bad_read;
    fclose(file);

    run(src);
    free(src);
    parse();
    ir_init();

    amd64_init();
    int start, end;
    
    while(ir_get_block(&start, &end)) {
        if(print_blocks) printf("\nBB ===\n");
        var_graph* g = ir_get_interference_graph(ir_get_vars(start, end), start, end);
        amd64_color_registers(g, start, end);
        amd64_translate(g, start, end);
        free(g);
    }

    for(int i = 0; i < amd64_asm->n_values; i++) 
        printf("%s", amd64_asm->values[i]);

    return 0;

    bad_file:
    printf("Couldn't open %s\n", argv[1]);
    return 1;

    bad_read:
    printf("Read only %ld/%ld bytes\n", read, fsize);
    return 1;

    return 0;
}