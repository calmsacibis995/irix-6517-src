#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_set.c,v 1.7 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

void
walk_variables(variable_t *vp, FILE *ofp)
{
	if (!vp) {
		return;
	}
	walk_variables((variable_t *)vp->v_left, ofp);
	if (vp->v_flags & V_TYPEDEF) {
		if (vp->v_flags & V_REC_STRUCT) {
			fprintf(ofp, "%s = <record/struct> %s\n", 
				vp->v_name, vp->v_typestr);
		}
		else {
			fprintf(ofp, "%s = %s\n", vp->v_name, vp->v_typestr);
		}
	}
	else {
		fprintf(ofp, "%s = \"%s\"\n", vp->v_name, vp->v_exp);
	}
	walk_variables((variable_t *)vp->v_right, ofp);
}

/*
 * set_cmd() -- set eval variables
 */
int
set_cmd(command_t cmd)
{
	int i, j = 0, len, ret, e, type = V_TYPEDEF;
	variable_t *vp;
	node_t *np = (node_t *)NULL;
	char *cmdstr, *c, *eqp, *vname, *exp, *buf;

	if (!cmd.nargs) {
		if (cmd.flags & C_COMMAND) {
			set_usage(cmd);
			return(1);
		}

		/* Display all set variables
		 */
		walk_variables(vtab->vt_root, cmd.ofp);
	}
	else {

		/* Count the number of bytes necessary to hold all the command
		 * line arguments (variable name, equal sign, and expression).
		 */
		for (i = 0; i < cmd.nargs; i++) {
			j += (strlen(cmd.args[i]) + 1);
		}

		/* Allocate space for the string and copy the individual
		 * arguments into it.
		 */
		buf = (char *)kl_alloc_block(j, K_TEMP);
		for (i = 0; i < cmd.nargs; i++) {
			strcat(buf, cmd.args[i]);
			if ((i + 1) < cmd.nargs) {
				strcat(buf, " ");
			}
		}

		/* Get the variable name (grab everything up to the 
		 * equal sign). 
		 */
		if (!(eqp = strchr(buf, '='))) {
			fprintf(KL_ERRORFP, "ERROR: syntax error\n");
			kl_free_block((k_ptr_t)buf);
			return(1);
		}

		len = eqp - buf + strspn(eqp + 1, " ") + 1;
		cmdstr = (char *)kl_alloc_block(len + 5, K_TEMP);
		strcat(cmdstr, "set ");
		strncat(cmdstr, buf, len);
		cmdstr[len + 4] = 0;

		/* Zero out the equal sign to terminate the name string. The
		 * expression string starts at (eqp + 1).
		 */
		*eqp++ = 0;

		/* If this is a command line sting, stip off any leading blank
		 * space. Text expressions we leave as is.
		 */
		if (cmd.flags & C_COMMAND) {
			while (*eqp == ' ') {
				eqp++;
			}
		}

		/* Get the pointer to the first character in the variable name.
		 * If it isn't the first character in the string, move it left
		 * so that it is. Also, strip off any trailing blanks.
		 */ 
		if (!(vname = strchr(buf, '$'))) {
			fprintf(KL_ERRORFP, "Cannot set \"%s\": variables must begin with "
				"the character '$'.\n", buf);
			kl_free_block((k_ptr_t)cmdstr);
			kl_free_block((k_ptr_t)buf);
			return(1);
		}
		if (vname != buf) {
			memmove(buf, vname, strlen(vname));
			vname = buf;
		}
		if (c = strchr(vname, ' ')) {
			*c = 0;
		}

		/* Now copy the remainder of the input string into a new
		 * expression string. A seperate expresison string is necessary
		 * because it is likely to get freed up in eval() when variables 
		 * get expanded.
		 */
		exp = (char *)kl_alloc_block(strlen(eqp) + 1, K_TEMP);
		strcpy(exp, eqp);

		/* Check to see if expression is a string (begins with a double
		 * quote ('"') character. Strip off the quotes. Then check to see
		 * if the C_COMMAND flag is set. If it is, drop through. Otherwise
		 * make the variable as type V_STRING.
		 */
		c = exp;
		while (*c == ' ') {
			c++;
		}
		if (*c == '\"') {
			memmove(exp, c + 1, strlen(c) - 1);
			if (c = strchr(exp, '\"')) {
				*c = 0;
			}
			else {
				fprintf(KL_ERRORFP, "ERROR: non-terminating string\n");
				kl_free_block((k_ptr_t)buf);
				kl_free_block((k_ptr_t)cmdstr);
				return(1);
			}
			if (cmd.flags & C_COMMAND) {
				type = V_COMMAND;
			}
			else {
				type = V_STRING;
			}
		}
		else {
			/* If we walked over any blank space, we have to move the
			 * start of the expression over. That is because the block
			 * that gets passed to eval() MUST be the same block that
			 * was allocated by kl_alloc_block(). Otherwise the attempt 
			 * to free it will cause an assertion failure in free_blk().
			 */
			if (c != exp) {
				memmove(exp, c, strlen(c) + 1);
			}
			if (cmd.flags & C_COMMAND) {
				type = V_COMMAND;
			}
			else {
				type = V_TYPEDEF;
			}
		}

		/* Check and see if expression is really an icrash command line. 
		 * If it is, then just pass the entire command line to set_variable().
		 */
		if ((type == V_STRING) || (type == V_COMMAND)) {
			if (set_variable(vtab, vname, exp, (node_t*)NULL, type)) {
				fprintf(KL_ERRORFP, "ERROR: cannot set variable %s\n", vname);
				kl_free_block((k_ptr_t)buf);
				kl_free_block((k_ptr_t)cmdstr);
				return(1);
			}
		}
		else {

			/* Evaluate the expression. There can be quite a few blocks of 
			 * memory used in the representation of a single expression. 
			 * Unless there is some reason to store this information, we 
			 * need to free up the memory resources after we set the variable.
			 */
#ifdef PERM_NODE
			np = eval(&exp, C_B_PERM, &e);
#else
			np = eval(&exp, 0, &e);
#endif /* PERM_NODE */
			if (e) {
				int l;
				char *s;

				print_eval_error(cmdstr, exp, 
					(np ? np->tok_ptr : (char*)NULL), e, CMD_STRING);
				free_nodes(np);
				kl_free_block((k_ptr_t)buf);
				kl_free_block((k_ptr_t)cmdstr);
				kl_free_block((k_ptr_t)exp);
				return(1);
			}
#ifdef PERM_NODE
			ret = set_variable(vtab, vname, exp, np, V_TYPEDEF|V_PERM_NODE);
#else
			ret = set_variable(vtab, vname, exp, np, V_TYPEDEF);
#endif
			if (ret) {
				fprintf(KL_ERRORFP, "ERROR: cannot set variable %s\n", vname);
				kl_free_block((k_ptr_t)buf);
				kl_free_block((k_ptr_t)cmdstr);
				kl_free_block((k_ptr_t)exp);
				return(1);
			}
		}
		kl_free_block((k_ptr_t)cmdstr);
		kl_free_block((k_ptr_t)buf);
		kl_free_block((k_ptr_t)exp);
	}
	return(0);
}

#define _SET_USAGE "[-w outfile] [-c] [var_name] [=] [expression]"

/*
 * set_usage() -- Print the usage string for the 'set' command.
 */
void
set_usage(command_t cmd)
{
	CMD_USAGE(cmd, _SET_USAGE);
}

/*
 * set_help() -- Print the help information for the 'set' command.
 */
void
set_help(command_t cmd)
{
	CMD_HELP(cmd, _SET_USAGE,
		"Set variables to be utilized by the print and whatis "
		"commands");
}

/*
 * set_parse() -- Parse the command line arguments for 'set'.
 */
int
set_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_COMMAND);
}
