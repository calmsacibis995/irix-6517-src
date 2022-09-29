%{
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "globals.h"
#include "node.h"

// Global variables (see descriptions in "globals.h"):
//
list_t parsetree;
idl_subsystem_t *subsystem_node = NULL;
char *msg_channel = "MAIN";

// Local variables:
//
static int seen_subsystem = 0;		// subsystem statement parsed
static int complained_interface = 0;	// parse latch: only complain once
					//   about missing subsystemid and
					//   subsystemname statements

static int opcode = 0;			// opcode for next interface definition

static int interface_is_asynchronous;	// TRUE if interface currently being
					//   parsed is asynchronous (or has
					//   an attached condition)
static int parameter_direction;		// direction of parameter currently 
					//   being parsed
%}

%start file

%union
{
	int intVal;
	char *strVal;
	struct listitem_t *listitemVal;
	struct list_t *listVal;
}

/* keyword tokens */
%token IMPORT SUBSYSTEM OPCODE SYNCHRONOUS ASYNCHRONOUS CALLBACK
%token IF IN OUT INOUT POINTERTO VALUEOF BUFFEROF VECTOROF PROLOGUE EPILOGUE

/* pattern tokens */
%token <intVal> INTEGER
%token <strVal> IDENTIFIER QUOTE_STRING ANGLE_STRING PAREN_STRING BRACE_STRING
%token <strVal> CTYPE CHANNEL

/* special non-terminal types */
%type <intVal> file
%type <listitemVal> declaration
%type <listitemVal> subsystem import opcode interface
%type <strVal> import_filename
%type <intVal> synchronicity
%type <strVal> synchronicity_condition
%type <strVal> idl_type
%type <listVal> parameters
%type <listitemVal> parameter
%type <strVal> prologue epilogue
%type <intVal> inout valuereference debugsize
%type <strVal> type

%%

file
	: declarations { $$ = compilation_errors; }
	;

declarations
	: /* empty */
	| declarations declaration { parsetree.append($2); }
	;

declaration
	: subsystem
	| import
	| opcode
	| interface
	;

/*
 * Interface subsystem identification specification:
 *
 * ``subsystem "NAME" ID'':
 *
 * Assign a name and unique integer identifier to the service subsystem that
 * the IDL source file defines.  The NAME will be used in naming the output
 * files, the interface message definitions, and the client and server
 * interfaces.  The ID will be used by the lower layer message
 * demultiplexing to hand off incoming messages to the appropriate server
 * message dispatcher routine.  A subsystem specification must be stated
 * before any interface specification.  It is an error for more than one
 * subsystem specification to be present.
 */
subsystem
	: SUBSYSTEM QUOTE_STRING INTEGER
	    {
		if (seen_subsystem)
			yyerror("only one subsystem statement allowed");
		seen_subsystem = 1;
		// strip quotes from subsystem name
		int len = strlen($2)-2;
		memmove($2, $2+1, len);
		$2[len] = '\0';
		// verify that subsystem id is in legal range
		if ($3 < SUBSYSTEMID_MIN || $3 > SUBSYSTEMID_MAX)
			yyerror("subsystem id %d out of range %d to %d",
				$3, SUBSYSTEMID_MIN, SUBSYSTEMID_MAX);
		subsystem_node = new idl_subsystem_t($2, $3);
		assert(subsystem_node);
		$$ = subsystem_node;
	    }
	| SUBSYSTEM QUOTE_STRING INTEGER CHANNEL
	    {
		if (seen_subsystem)
			yyerror("only one subsystem statement allowed");
		seen_subsystem = 1;
		// strip quotes from subsystem name
		int len = strlen($2)-2;
		memmove($2, $2+1, len);
		$2[len] = '\0';
		// verify that subsystem id is in legal range
		if ($3 < SUBSYSTEMID_MIN || $3 > SUBSYSTEMID_MAX)
			yyerror("subsystem id %d out of range %d to %d",
				$3, SUBSYSTEMID_MIN, SUBSYSTEMID_MAX);
		msg_channel = $4;
		subsystem_node = new idl_subsystem_t($2, $3);
		assert(subsystem_node);
		$$ = subsystem_node;
	    }
	;

/* ``import "FILE" | <FILE>'':
 *
 * cause a corresponding ``#include'' to be emitted in the client and server
 * stub files.
 */
import
	: IMPORT import_filename
		{
			idl_import_t *imported = new idl_import_t($2);
			assert(imported);
			$$ = imported;
		}
	;

import_filename
	: QUOTE_STRING
	| ANGLE_STRING
	;

/*
 * Interface opcode specification:
 *
 * ``opcode [+]VALUE''
 *
 * The opcode declaration can be used to set the next generated opcode (thus,
 * one can imagine that there is a default ``opcode 0'' preceeding every IDL
 * source file).  If the optional "+" is present it will cause the current
 * next opcode value to be incremented by VALUE thereby skipping the opcode
 * values [opcode, opcode+VALUE-1].  Opcodes are unsigned integers in the
 * range 0 to 255.
 */
opcode
	: OPCODE INTEGER
	    {
		int new_opcode = $2;
		idl_opcode_t *idl_op;
		if (new_opcode >= OPCODE_MIN && new_opcode <= OPCODE_MAX)
			opcode = new_opcode;
		else
			yyerror("opcode out of range %u to %u",
				OPCODE_MIN, OPCODE_MAX);
		idl_op = new idl_opcode_t(new_opcode);
		assert(idl_op);
		$$ = idl_op;
	    }
	| OPCODE '+' INTEGER
	    {
		int new_opcode = opcode + $3;
		idl_opcode_t *idl_op;
		if (new_opcode >= OPCODE_MIN && new_opcode <= OPCODE_MAX)
			opcode = new_opcode;
		else
			yyerror("resulting opcode, %u, out of range %u to %u",
				opcode, OPCODE_MIN, OPCODE_MAX);
		idl_op = new idl_opcode_t(new_opcode);
		assert(idl_op);
		$$ = idl_op;
	    }
	;

/*
 * Interface specification:
 *
 * ``synchronous|asynchronous [if (CONDITION)]
 *   NAME(PARAMETERS)
 *   [debug_prologue {DEBUG-PROLOGUE-CODE}]
 *   [debug_epilogue {DEBUG-EPILOGUE-CODE}]'':
 *
 * Define a subsystem interface called NAME.  The definition consists of a
 * specification as to whether the interface is synchronous or asynchronous,
 * the interface NAME and PARAMETERS, and optional prologue and epilogue
 * debug code.  The synchronicity specification can either be "synchronous"
 * or "asynchronous".  This can be followed by an optional conditional to
 * indicate that the routine is "synchronous" ("asynchronous") "if"
 * CONDITION is true, or the opposite if false.
 */
interface
	: synchronicity idl_type synchronicity_condition IDENTIFIER
	    {
		if (!complained_interface) {
			complained_interface = 1;
			if (!seen_subsystem)
				yyerror("subsystem required before interface definitions");
		}
		if (opcode < OPCODE_MIN || opcode > OPCODE_MAX)
			yyerror("interface opcode, %u, out of range %u to %u",
				opcode, OPCODE_MIN, OPCODE_MAX);

		if ($1 == CALLBACK && $3 != NULL)
			yyerror("callback interfaces may not be conditional");

		interface_is_asynchronous = $1 == ASYNCHRONOUS || $3 != NULL;
		if (interface_is_asynchronous) {
			if ($2 != NULL)
				yyerror("%s interface may not have a type specified",
					$3 != NULL ? "Possibly asynchronous" :
						     "Asynchronous");
			else
				$2 = "void";
		} else if ($2 == NULL)
			yyerror("Synchronous interface must have a type specified");
	    }
	'(' parameters ')' prologue epilogue
	    {
		idl_interface_t *iface;

		iface = new idl_interface_t(opcode++, $1, $2, $3, $4, $7, $9, $10);
		assert(iface);
		$$ = iface;
	    }
	;

idl_type
	: /* empty */	{ $$ = NULL; }
	| CTYPE
	    {
		if (strcmp($1, "int") == 0)
			$$ = $1;
		else if (strcmp($1, "void") == 0)
			$$ = $1;
		else
			yyerror("interface type must be either int or void");
	    }
	;

synchronicity
	: SYNCHRONOUS { $$ = SYNCHRONOUS; }
	| ASYNCHRONOUS { $$ = ASYNCHRONOUS; }
	| CALLBACK { $$ = CALLBACK; }
	;

synchronicity_condition
	: /* empty */		{ $$ = NULL; }
	| IF PAREN_STRING	{ $$ = $2; }
	;

parameters
	: parameter
	    {
		list_t *head = new list_t();
		assert(head);
		assert($1->prev == NULL);
		head->append($1);
		$$ = head;
	    }
	| parameters ',' parameter
	    {
		assert($3->prev == NULL);
		$1->append($3);
		$$ = $1;
	    }
	;

/*
 * Interface parameter specification:
 *
 * ``in|out|inout valueof|pointerto|bufferof|vectorof [trace_size] TYPE PARAM''
 *
 * Interface parameters are qualified with input/output and value/reference
 * specifications.  The input/output qualification is "in", "out", "inout".
 * The value/reference qualification is "valueof", "pointerto".
 *
 * The input/output qualifier specifies whether a parameter is only an input
 * into the interface ("in"), only a result from the interface ("out"), or
 * an input to the interface and also used to return a result ("inout").
 *
 * The value/reference qualifier specifies whether the parameter is passed
 * by value ("valueof") or by reference ("pointerto").  A "valueof" parameter
 * is passed by value to a client stub, copied into a request message,
 * and then passed by value to a server interface implementation routine.
 * A "pointerto" parameter is passed by reference to a client stub,
 * dereferenced and copied into a request message, and a pointer to the
 * parameter in the request message is passed to a server interface
 * implementation routine.  Thus, to litterally pass a pointer to some object
 * in the client address space to the server (say, to pass a buffer address
 * to a remove file server), one would type the parameter as a pointer and
 * qualify it as "valueof".  E.g. ``in valueof caddr_t buf''.
 *
 * Note: there are several restrictions on parameters:
 *
 *  1. Asynchronous and conditionally a/synchronous interfaces can't have
 *     output parameters ("out" or "inout").  If we asynchronously return
 *     we can't assign a value to an output parameter.
 *
 *  2. The "out valueof" and "inout valueof" parameter qualifier combinations
 *     are illegal.  Again, we can't [productively] assign a value to an
 *     output parameter passed by value.
 */
parameter
	: inout
	    {
		parameter_direction = $1;
		if (parameter_direction != IN  && interface_is_asynchronous)
			yyerror("asynchronous interface must have \"in\" parameters");
	    }
	valuereference
	    {
		if (parameter_direction != IN && $3 == VALUEOF)
			yyerror("non-\"in\" parameter must not be \"valueof\"");
	    }
	debugsize type IDENTIFIER
	    {
		idl_parameter_t *param;
		param = new idl_parameter_t($1, $3, $5, $6, $7);
		assert(param);
		if (param->valref == BUFFEROF || param->valref == VECTOROF) {
			int	reference;
			int	inout;
			char	lenname[80];
			idl_parameter_t *cnt_param;

			if (param->inout == OUT) {
				reference = POINTERTO;
				inout = INOUT;
			} else if (param->inout == IN) {
				reference = VALUEOF;
				inout = IN;
			} else
				yyerror("\"%s\"parameter may not be \"inout\"",
					param->valref == BUFFEROF ? "bufferof"
						: "vectorof");
			sprintf(lenname, "%s_count", param->name);
			cnt_param = new idl_parameter_t(inout,
					reference, 0, strdup("size_t"),
					strdup(lenname));
			assert(cnt_param);
			param->chain(cnt_param);
			assert(param->prev == NULL);
			assert(cnt_param->next == NULL);
			assert(param->next == cnt_param);
			assert(cnt_param->prev == param);
		}
		$$ = param;
	    }
	;

inout
	: IN { $$ = IN; }
	| OUT { $$ = OUT; }
	| INOUT { $$ = INOUT; }
	;

valuereference
	: POINTERTO { $$ = POINTERTO; }
	| VALUEOF { $$ = VALUEOF; }
	| BUFFEROF { $$ = BUFFEROF; }
	| VECTOROF { $$ = VECTOROF; }
	;

debugsize
	: /* empty */ { $$ = 0; }
	| INTEGER
	;

type
	: CTYPE
	| IDENTIFIER
	| ANGLE_STRING
	    {
		// XXX If an interface parameter type can not be expressed as a
		// XXX single C identifier, it must be quoted with meta-braces
		// XXX (<>) or defined in as a C typedef.  It's just too hard to
		// XXX try to parse out an indefinite number of lexical tokens
		// XXX or support syntax like ``int (*param)(int, int),'' etc.
		//
		int len = strlen($1)-2;
		memmove($1, $1+1, len);	// strip leading meta-brace: <
		$1[len] = '\0';		// strip trailing meta-brace: >
		$$ = $1;
	    }
	;

prologue
	: /* empty */		{ $$ = NULL; }
	| PROLOGUE BRACE_STRING	{ $$ = $2; }
	;
epilogue
	: /* empty */		{ $$ = NULL; }
	| EPILOGUE
	    {
		if (interface_is_asynchronous)
			yyerror("asynchronous interface cannot have debug epilogue");
	    }
	BRACE_STRING		{ $$ = $3; }
	;
