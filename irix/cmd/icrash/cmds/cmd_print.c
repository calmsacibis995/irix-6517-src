#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_print.c,v 1.14 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * print_cmd() -- Run the 'print' command.
 */
int
print_cmd(command_t cmd)
{
	int size, i, j = 0, e = 0, tag;
	kaddr_t addr;
	k_uint_t val;
	char *buf, *next, *end, *exp;
	node_t *np;
	dw_type_info_t *t;
	dw_die_t *d;

	/* If there is nothing to evaluate, just return
	 */
	if (cmd.nargs == 0) {
		return(0);
	}

	/* Count the number of bytes necessary to hold the entire expression
	 * string.
	 */
	for (i = 0; i < cmd.nargs; i++) {
		j += (strlen(cmd.args[i]) + 1);
	}

	/* Allocate space for the expression string and copy the individual
	 * arguments into it.
	 */
	buf = (char *)kl_alloc_block(j, K_TEMP);
	for (i = 0; i < cmd.nargs; i++) {
		strcat(buf, cmd.args[i]);
		if ((i + 1) < cmd.nargs) {
			strcat(buf, " ");
		}
	}

	/* Walk through the expression string, expression by expression.
	 * Note that a comma (',') is the delimiting character between
	 * expressions.
	 */
	next = buf;
	while (next) {
		if (end = strchr(next, ',')) {
			*end = (char)0;
		}

		/* Copy the next expression to a separate expression string.
		 * A seperate expresison string is necessary because it is 
		 * likely to get freed up in eval() when variables get expanded.
		 */
		exp = (char *)kl_alloc_block(strlen(next) + 1, K_TEMP);
		strcpy(exp, next);

		/* Evaluate the expression
		 */
		np = eval(&exp, 0, &e);
		if (e) {
			print_eval_error(cmd.com, exp, 
					(np ? np->tok_ptr : (char*)NULL), e, CMD_NAME);
			free_nodes(np);
			kl_free_block((k_ptr_t)buf);
			kl_free_block((k_ptr_t)exp);
			return(0);
		}
		if (print_eval_results(np, cmd.ofp, cmd.flags)) {
			free_nodes(np);
			kl_free_block((k_ptr_t)buf);
			return(1);
		}
		kl_free_block((k_ptr_t)exp);

		if (end) {
			next = end + 1;
			fprintf(cmd.ofp, " ");
		}
		else {
			next = (char*)NULL;
			fprintf(cmd.ofp, "\n");
		}
		free_nodes(np);
	}
	kl_free_block((k_ptr_t)buf);
	return(1);
}

#define _PRINT_USAGE "[-d] [-o] [-x] [-w outfile] expression"

/*
 * print_usage() -- Print the usage string for the 'print' command.
 */
void
print_usage(command_t cmd)
{
	CMD_USAGE(cmd, _PRINT_USAGE);
}

/*
 * print_help() -- Print the help information for the 'print' command.
 */
void
print_help(command_t cmd)
{
	CMD_HELP(cmd, _PRINT_USAGE,
		"Evaluate an expression and print the result. An expression can "
		"consist of numeric values, operators, kernel variables, typedefs, "
		"struct/union members, or a combination of above. The following "
		"are some examples of valid expressions:\n\n"
		"    (((2*3+4/2)*2+(2/6))/2)"
		"\n\n"
		"    *((struct socket*)0xa80000001019f2c8)->so_rcv->sb_mb"
		"\n\n"
		"    (((pte_t*)0xa8000000005ef088)->pte_pfn)<<14|0xa800000000000000");
}

/*
 * print_parse() -- Parse the command line arguments for 'print'.
 */
int
print_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_DECIMAL|C_OCTAL|C_HEX);
}
