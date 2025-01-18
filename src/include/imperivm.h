#ifndef _IMPERIVM_H
#define _IMPERIVM_H

#define min(a, b) ((a) < (b) ? (a) : (b))
#define mem_fail() no_mem(__func__, __FILE__, __LINE__)

void __attribute__((noreturn)) ir_exit(void);
void __attribute__((noreturn)) report_error(int, char*);
void __attribute__((noreturn)) no_mem(char*, char*, int);
void report(int, char*, char*);
void ir_init(void);

extern char* ir_out;

#endif
