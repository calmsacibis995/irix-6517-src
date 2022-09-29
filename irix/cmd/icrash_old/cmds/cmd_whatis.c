#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_whatis.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#define _KERNEL  1
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <libelf.h>
#include <dwarf.h>
#include <libdwarf.h>
#include <syms.h>
#include <sym.h>

#include "icrash.h"
#include "extern.h"
#include "eval.h"
#include "dwarflib.h"

/*
 * whatis_cmd()
 */
int
whatis_cmd(command_t cmd)
{
	int size, i, j = 0, e, tag;
	kaddr_t addr;
	k_uint_t val;
	char *buf;
	node_t *np;
	dw_type_info_t *t;
	dw_die_t *d;

	if ((cmd.nargs == 0) && !(cmd.flags & C_LIST)) {
		if (cmd.flags & C_ALL) {
			dw_dowhatis(cmd);
			return(0);
		}
		else {
			return(1);
		}
	}

	if (cmd.flags & C_LIST) {

		symdef_t *sdp;

		struct_banner(cmd.ofp, BANNER|SMAJOR);
		for (i = 0; i < cmd.nargs; i++) {
			sdp = sym_lkup(stp, cmd.args[i]);
			if (KL_ERROR) {
				kl_print_error(K);
			}
			else {
				print_struct(sdp, cmd.flags, cmd.ofp);
				if ((DEBUG(DC_GLOBAL, 1) ||
						(cmd.flags & C_FULL)) && i < cmd.nargs - 1) {
					fprintf(cmd.ofp, "\n");
					struct_banner(cmd.ofp, BANNER|SMAJOR);
				}
			}
		}
		struct_banner(cmd.ofp, SMAJOR);
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
	buf = (char *)alloc_block(j, B_TEMP);
	for (i = 0; i < cmd.nargs; i++) {
		strcat(buf, cmd.args[i]);
		if ((i + 1) < cmd.nargs) {
			strcat(buf, " ");
		}
	}

	/* Evaluate the expression
	 */
	np = eval(&buf, C_WHATIS|C_NOVARS, &e);
	if (e) {
		print_eval_error(cmd.com, buf, 
			(np ? np->tok_ptr : (char*)NULL), e, CMD_NAME);
		free_block((k_ptr_t)buf);
		if (np) {
			free_nodes(np);
		}
		return(1);
	}

	/* Print the results
	 */
	switch (np->node_type) {

		case NUMBER : {
			int ptr_cnt = 0;
			base_t *bt;
			type_t *tp;

			/* Check to see if we point to a type (because the resulting
			 * numeric value was the result of a typedef). If we do, use
			 * the base type information to print out the resulting type.
			 * Otherwise, determine if the resulting value fits into a 
			 * char, short, int, long (long long) of whatever.
			 */
			if (tp = np->type) {
				while (tp && tp->flag == POINTER_FLAG) {
					ptr_cnt++;
					tp = tp->t_next;
				}
				if (!tp || (tp->flag != BASE_TYPE_FLAG)) {
					e = E_SYNTAX_ERROR;
					print_eval_error(cmd.com, buf, 
							(np ? np->tok_ptr : (char*)NULL), e, CMD_NAME);
					free_nodes(np);
					free_block((k_ptr_t)buf);
					if (np) {
						free_nodes(np);
					}
					return(1);
				}
				bt = (base_t*)tp->t_ptr;
				if (bt->actual_name) {
					fprintf(cmd.ofp, "%s;\n", bt->actual_name);
				}
				else {
					fprintf(cmd.ofp, "%s;\n", bt->name);
				}
			}
			else {
				if (np->value > 0xffffffff) {
					if (PTRSZ64(K)) {
						fprintf(cmd.ofp, "long;\n");
					}
					else {
						fprintf(cmd.ofp, "long long;\n");
					}
				}
				else {
					fprintf(cmd.ofp, "int;\n");
				}
			}
			break;
		}

		case TYPE_DEF : {

			int ptr_cnt = 0;
			type_t *tp;
			k_ptr_t ptr;

			tp = np->type;
			while (tp && tp->flag == POINTER_FLAG) {
				ptr_cnt++;
				tp = tp->t_next;
			}

			if (np->flags & DWARF_TYPE_FLAG) {

				/* Get the type information
				 */
				t = (dw_type_info_t *)tp->t_ptr;
				if (t->t_type->d_tag == DW_TAG_typedef) {
					d = t->t_actual_type;
				}
				else {
					d = t->t_type;
				}
				if ((d->d_tag == DW_TAG_base_type) && 
							!(np->flags & ADDRESS_FLAG)) {
					dw_print_type(0, t, 0, cmd.flags, cmd.ofp);
					free_nodes(np);
					free_block((k_ptr_t)buf);
					return(0);
				}

				/* Print out the actual type
				 */
				if (t->t_link) {
					if  ((((t->t_tag == DW_TAG_structure_type) ||
								(t->t_tag == DW_TAG_union_type)) && 
								!(np->flags & POINTER_FLAG))) { 
						dw_print_type(0, t, 0, cmd.flags, cmd.ofp);
					}
					else if (np->flags & INDIRECTION_FLAG) {
						dw_type_info_t *t1;

						t1 = (dw_type_info_t*)alloc_block(sizeof(*t1), B_TEMP);
						if (!dw_type_info(t->t_actual_type->d_off, t1, 0)) {
							/* ERROR! */
						}
						dw_free_type_info(t);
						dw_print_type(0, t1, 0, cmd.flags, cmd.ofp);
					}
					else {
						dw_print_type(0, t->t_link, 0, cmd.flags, cmd.ofp);
					}
				}
				else {
					dw_print_type(0, t, 0, cmd.flags, cmd.ofp);
				}
			} 
			else if (np->flags & BASE_TYPE_FLAG) {
				base_t *bt;

				/* Get the type information
				 */
				bt = (base_t *)np->type->t_ptr;
				if (bt->actual_name) {
                    fprintf(cmd.ofp, "%s;\n", bt->actual_name);
                }
                else {
                    fprintf(cmd.ofp, "%s;\n", bt->name);
                }
			}
			break;
		}

		case VARIABLE :
			/* If we get here, there was no type info available.
			 * The ADDRESS_FLAG should be set (otherwise we would have
			 * returned an error). So, print out the address.
			 */ 
			fprintf(cmd.ofp, "0x%llx\n", np->address);
			break;
	}
	free_nodes(np);
	free_block((k_ptr_t)buf);
	return(1);
}

#define _WHATIS_USAGE "[-a] [-f] [-l] [-w outfile] expression"

/*
 * whatis_usage() -- Print the usage string for the 'whatis' command.
 */
void
whatis_usage(command_t cmd)
{
	CMD_USAGE(cmd, _WHATIS_USAGE);
}

/*
 * whatis_help() -- Print the help information for the 'whatis' command.
 */
void
whatis_help(command_t cmd)
{
	CMD_HELP(cmd, _WHATIS_USAGE,
		"Display type specific information for expression (see the print "
		"command for what constitutes a valid expression). If the -a option "
		"is specified, display a list of all structures, unions, typedefs "
		"and variables. When, in conjunction with the -a option, the -f "
		"option is specified, expand all struct and union definitions.");
}

/*
 * whatis_parse() -- Parse the command line arguments for 'whatis'.
 */
int
whatis_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_FULL|C_ALL|C_LIST);
}
