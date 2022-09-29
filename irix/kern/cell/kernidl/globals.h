#if !defined(_globals_h_)
#define _globals_h_

#include <limits.h>
#include "list.h"
#include "node.h"

typedef enum msgsystem {
	MSG_XPC
} msgsystem_t;

// Main program state.
//
extern char *myname;			// name we were invoked as
extern int main_argc;			// original main program argc/argv
extern char **main_argv;
extern char *output_dir;		// directory to place output files

// Lexical and parse routines:
//
extern int yylex(void);
extern int yyparse(void);
extern int yyerror(char *msg, ...);

// Compilation state:
//
extern list_t parsetree;		// decoded parse tree
extern int compilation_errors;		// number of compilation errors
extern idl_subsystem_t *subsystem_node;	// pointers to subsystem node in
					//   parsetree

// Code generation:
//
extern int emit_files();

// Micelaneous definitions:
//
#define	SUBSYSTEMID_MIN	0		// subsystemids are unsigned 8-bit
#define	SUBSYSTEMID_MAX	UCHAR_MAX	//   integers (see <ksys/mesg.h>)
#define OPCODE_MIN	0		// opcodes are unsigned 16-bit
#define	OPCODE_MAX	USHRT_MAX	//   integers (see <ksys/mesg.h>)

#endif /* _globals_h_ */
