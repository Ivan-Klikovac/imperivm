project('compiler', 'c')
incdir = include_directories('include')
add_global_arguments('-O0', language : 'c')
add_global_arguments('-ggdb', language : 'c')
add_global_arguments('-g3', language : 'c')
add_global_arguments('-Wno-int-conversion', language : 'c')
add_global_arguments('-Wno-unused-function', language : 'c')
sources = ['main.c', 'frontend/lexer.c', 'frontend/parser.c', 'frontend/vector.c', 'IR/IR.c', 'IR/IR_print.c', 'IR/IR_optimize.c', 'backend/amd64/amd64.c', 'backend/amd64/amd64_translate.c', 'util/alloc.c']
executable('imc', sources, include_directories : incdir)
