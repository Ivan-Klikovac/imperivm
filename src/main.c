#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/fcntl.h>
#include <imperivm.h>
#include <frontend/lexer.h>
#include <IR/IR.h>
#include <IR/IR_print.h>
#include <backend/amd64/amd64.h>

char* ir_out = 0;
FILE* outfile = 0;
int verbose_asm = 0;
int print_blocks = 0;

void __attribute__((noreturn)) no_mem(const char* fn, char* file, int line)
{
    printf("imc: %s (%s:%d): malloc failed\n", fn, file, line);
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

void __attribute__((noreturn)) help(void)
{
    printf("Usage: imc [options] file\nOptions:\n");
    printf("    %-36s%s\n", "--output       (-o) [file]", "Specify the final executable");
    printf("    %-36s%s\n", "--ir           (-i) [file]", "Dump the IR into the specified file");
    printf("    %-36s%s\n", "--verbose-asm  (-v)", "Output the IR alongside the resulting assembly (implies --asm-only)");
    printf("    %-36s%s\n", "--print-blocks (-p)", "Show basic block boundaries (assumes --verbose-asm)");
    printf("    %-36s%s\n", "--asm-only     (-a)", "Only output assembly");
    printf("    %-36s%s\n", "--static       (-s)", "Force static linking");
    printf("    %-36s%s\n", "--help         (-h)", "Print help information and exit");
    printf("    %-36s%s\n", "--version      (-n)", "Print version information and exit");
    
    exit(0);
}

void __attribute((noreturn)) version(void)
{
    puts("--- imc 1.2.1\n\
The IMPERIVM C compiler\n\
This is free software, distributed under GPLv3. See LICENSE.md for details.");
    
    exit(0);
}

int main(int argc, char* argv[])
{
    char* output = 0;
    static int asm_only = 0;
    static int static_linking = 0;

    if(argc < 2) goto no_args;

    int c, optindex = 0;
    while(1) {
        static struct option opts[] = 
        {
            {"ir", required_argument, 0, 0},
            {"output", required_argument, 0, 'o'},
            {"verbose-asm", no_argument, &verbose_asm, 1},
            {"print-blocks", no_argument, &print_blocks, 1},
            {"asm-only", no_argument, &asm_only, 1},
            {"static", no_argument, &static_linking, 1},
            {"help", no_argument, 0, 'h'},
            {"version", no_argument, 0, 'n'},
            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "vpahsno:", opts, &optindex);
        if(c == -1) break;

        switch(c) {
            case 0:
            if(opts[optindex].flag) break;
            if(optindex == 0) ir_out = strdup(optarg);
            if(optindex == 1) output = strdup(optarg);
            if(optindex == 6) help();
            if(optindex == 7) version();
            break;

            case 'v':
            verbose_asm = 1;
            break;

            case 'p':
            print_blocks = 1;
            break;

            case 'a':
            asm_only = 1;
            break;

            case 'h':
            help();
            break;

            case 's':
            static_linking = 1;
            break;

            case 'n':
            version();
            break;

            case 'o':
            output = strdup(optarg);
            break;

            case '?':
            printf("imc: unknown argument: %s\n", opts[optindex].name);
            break;
        }
    }

    if(print_blocks && !verbose_asm) {
        printf("imc: specify --verbose-asm for --print-blocks\n");
        return 1;
    }

    if(verbose_asm) asm_only = 1;

    if(asm_only && static_linking) {
        printf("imc: mutually exclusive arguments: --static, --asm-only\n");
        return 1;
    }

    FILE* file = fopen(argv[argc-1], "rb");
    if(!file) goto bad_file;
    
    fseek(file, 0, SEEK_END);
    uint64_t fsize = ftell(file);
    rewind(file);
    
    char* src = malloc(fsize+1);
    src[fsize] = 0;
    uint64_t read = fread(src, 1, fsize, file);
    if(read != fsize) goto bad_read;
    fclose(file);

    // overwrite the previous contents, if any
    if(ir_out) {
        FILE* ir = fopen(ir_out, "w");
        fclose(ir);
    } 

    run(src);
    free(src);
    parse();
    ir_init();

    outfile = stdout;
    if(output) outfile = fopen(output, "wb");

    amd64_init();

    int start, end;
    while(ir_get_block(&start, &end)) {
        if(print_blocks) fprintf(outfile, "\n<bb>\n");
        var_graph* g = ir_get_interference_graph(ir_get_vars(start, end), start, end);
        amd64_color_registers(g, start, end);
        amd64_translate(g, start, end);
        free(g);
    }

    if(asm_only && !verbose_asm) {
        for(int i = 0; i < amd64_asm->n_values; i++) 
            fprintf(outfile, "%s", amd64_asm->values[i]);

        return 0;
    }
    else if(asm_only) return 0;

    // --- now call GCC to assemble and link

    // generate a temporary file to store the asm
    char* temp_name = malloc(L_tmpnam + 2);
    if(!tmpnam(temp_name)) goto no_temp;
    
    // add .s so that gcc will know what to do with this
    // could've also specified `-x` but eh
    strcat(temp_name, ".s");

    FILE* asm_temp = fopen(temp_name, "wb");
    for(int i = 0; i < amd64_asm->n_values; i++)
        fprintf(asm_temp, "%s", amd64_asm->values[i]);
    fclose(asm_temp);

    int len = 0; // output file name length; either 0 or strlen(output) if output is specified
    if(output) len = strlen(output) + 4; // necessary ` -o `
    char* system_buf = malloc(3 + 1 + L_tmpnam + 2 + len + static_linking * 8 + 1);
    if(!system_buf) mem_fail();

    sprintf(system_buf, "gcc %s%s%s%s",
            temp_name, 
            len ? " -o " : "",
            len ? output : "",
            static_linking ? " -static" : "");

    system(system_buf);
    return 0;

    no_args:
    printf("Usage: imc [options] file\nRun imc --help for more info\n");
    return 1;

    bad_file:
    printf("imc: couldn't open %s\n", argv[argc-1]);
    return 1;

    bad_read:
    printf("imc: read only %ld/%ld bytes\n", read, fsize);
    return 1;

    no_temp:
    printf("imc: tmpnam failed\n");
} 
