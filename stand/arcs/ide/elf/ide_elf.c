#include <sys/types.h>
#include <sys/file.h>
#include <saio.h>
#include <elf.h>		
#include "ide_elf.h"

/* Globals for elf load stuff */
struct symbol_table _core_symbol_table;   /* contains core's symbol table */
char* _module_gp_ptr;                     /* ptr to the argv[1] entry which will
					     contain the gp address of the module */

