#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmds.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <fcntl.h>
#include "icrash.h"
#include "extern.h"
#include "eval.h"
#include "variable.h"
#include "cmds.h"

/* global variables 
 */
struct _commands commands;
char ofile[256];                                /* output file             */
char pfile[256];                                /* process file            */
unsigned have_args;                             /* if command has args ... */
unsigned mask[MAX_SEARCH_SIZE / 4];
jmp_buf jbuf;

struct _commands cmdset[] = {
	{ "?", "help" },
	{ "addtype", 0, addtype_cmd, addtype_parse, addtype_help, addtype_usage },
	{ "anon", 0, anon_cmd, anon_parse, anon_help, anon_usage },
	{ "anontree", 0, anontree_cmd, anontree_parse, anontree_help, 
	  anontree_usage },
	{ "avlnode", 0, avlnode_cmd, avlnode_parse, avlnode_help, avlnode_usage },
	{ "base", 0, base_cmd, base_parse, base_help, base_usage },
#ifdef ICRASH_DEBUG
	{ "block", 0, block_cmd, block_parse, block_help, block_usage },
	{ "bucket", 0, bucket_cmd, bucket_parse, bucket_help, bucket_usage },
#endif /* ICRASH_DEBUG */
	{ "config", 0, config_cmd, config_parse, config_help, config_usage },
	{ "ctrace", 0, ctrace_cmd, ctrace_parse, ctrace_help, ctrace_usage },
	{ "curkthread", 0, curkthread_cmd, curkthread_parse, curkthread_help, curkthread_usage },
	{ "d", "debug" },
	{ "debug", 0, debug_cmd, debug_parse, debug_help, debug_usage },
	{ "defkthread", 0, defkthread_cmd, defkthread_parse, defkthread_help, defkthread_usage },
	{ "defk", "defkthread" },
	{ "defproc", "defkthread" },
	{ "die", 0, die_cmd, die_parse, die_help, die_usage },
	{ "dirmem", 0, dirmem_cmd, dirmem_parse, dirmem_help, dirmem_usage },
	{ "dis", 0, dis_cmd, dis_parse, dis_help, dis_usage },
	{ "dump", 0, dump_cmd, dump_parse, dump_help, dump_usage },
	{ "eframe", 0, eframe_cmd, eframe_parse, eframe_help, eframe_usage },
	{ "etrace", 0, etrace_cmd, etrace_parse, etrace_help, etrace_usage },
	{ "eval", "print" },
	{ "f", "file" },
	{ "file", 0, file_cmd, file_parse, file_help, file_usage },
	{ "findsym", 0, findsym_cmd, findsym_parse, findsym_help, findsym_usage },
	{ "from", 0, from_cmd, from_parse, from_help, from_usage },
	{ "fru", 0, fru_cmd, fru_parse, fru_help, fru_usage },
	{ "fs", "fstype" },
	{ "fstype", 0, fstype_cmd, fstype_parse, fstype_help, fstype_usage },
	{ "fsym", "findsym" },
	{ "func", 0, func_cmd, func_parse, func_help, func_usage },
	{ "h", "history" },
	{ "help", 0, help_cmd, help_parse, help_help, help_usage },
	{ "hinv", 0, hinv_cmd, hinv_parse, hinv_help, hinv_usage },
	{ "history", 0, history_cmd, history_parse, history_help, history_usage },
	{ "hubreg", 0, hubreg_cmd, hubreg_parse, hubreg_help, hubreg_usage },
	{ "hwpath", 0, hwpath_cmd, hwpath_parse, hwpath_help, hwpath_usage },
	{ "i", "inode" },
	{ "icrashdef", 0, icrashdef_cmd, icrashdef_parse, icrashdef_help, icrashdef_usage },
	{ "in", "inpcb" },
	{ "inode", 0, inode_cmd, inode_parse, inode_help, inode_usage },
	{ "inpcb", 0, inpcb_cmd, inpcb_parse, inpcb_help, inpcb_usage },
#ifdef ANON_ITHREADS
	{ "ithread", 0, ithread_cmd, ithread_parse, ithread_help, ithread_usage },
#endif
	{ "kthread", 0, kthread_cmd, kthread_parse, kthread_help, kthread_usage },
	{ "ktrace", 0, ktrace_cmd, ktrace_parse, ktrace_help, ktrace_usage },
	{ "mbuf", 0, mbuf_cmd, mbuf_parse, mbuf_help, mbuf_usage },
	{ "mem", "memory" },
	{ "memory", 0, memory_cmd, memory_parse, memory_help, memory_usage },
#ifdef ICRASH_DEBUG
	{ "mempool", 0, mempool_cmd, mempool_parse, mempool_help, mempool_usage },
#endif /* ICRASH_DEBUG */
	{ "mlinfo", 0, mlinfo_cmd, mlinfo_parse, mlinfo_help, mlinfo_usage },
	{ "mntinfo", 0, mntinfo_cmd, mntinfo_parse, mntinfo_help, mntinfo_usage},
	{ "mrlock", 0, mrlock_cmd, mrlock_parse, mrlock_help, mrlock_usage },
	{ "nm", "symbol" },
	{ "nmlist", "namelist" },
	{ "namelist", 0, namelist_cmd, namelist_parse, namelist_help, namelist_usage },
	{ "nodepda", 0, nodepda_cmd, nodepda_parse, nodepda_help, nodepda_usage },
	{ "od", "dump" },
	{ "outfile", 0, outfile_cmd, outfile_parse, outfile_help, outfile_usage },
	{ "p", "print" },
#ifdef ICRASH_DEBUG
	{ "page", 0, page_cmd, page_parse, page_help, page_usage },
#endif /* ICRASH_DEBUG */
	{ "pager", 0, pager_cmd, pager_parse, pager_help, pager_usage },
	{ "pd", "printd" },
	{ "pda", 0, pda_cmd, pda_parse, pda_help, pda_usage },
	{ "pde", 0, pde_cmd, pde_parse, pde_help, pde_usage },
	{ "pfdat", 0, pfdat_cmd, pfdat_parse, pfdat_help, pfdat_usage },
	{ "pid", 0, pid_cmd, pid_parse, pid_help, pid_usage },
	{ "po", "printo" },
	{ "preg", "pregion" },
	{ "pregion", 0, pregion_cmd, pregion_parse, pregion_help, pregion_usage },
	{ "print", 0, print_cmd, print_parse, print_help, print_usage },
	{ "printd", 0, printd_cmd, printd_parse, printd_help, printd_usage },
	{ "printo", 0, printo_cmd, printo_parse, printo_help, printo_usage },
	{ "printx", 0, printx_cmd, printx_parse, printx_help, printx_usage },
	{ "proc", 0, proc_cmd, proc_parse, proc_help, proc_usage },
	{ "ptov", 0, ptov_cmd, ptov_parse, ptov_help, ptov_usage },
	{ "px", "printx" },
	{ "q!", "quit" },
	{ "q", "quit" },
	{ "queue", 0, queue_cmd, queue_parse, queue_help, queue_usage },
	{ "quit", 0, quit_cmd, quit_parse, quit_help, quit_usage },
	{ "reg", "region" },
	{ "region", 0, region_cmd, region_parse, region_help, region_usage },
	{ "report", 0, report_cmd, report_parse, report_help, report_usage },
	{ "rnode", 0, rnode_cmd, rnode_parse, rnode_help, rnode_usage },
	{ "runq", 0, runq_cmd, runq_parse, runq_help, runq_usage },
	{ "s", "struct", },
	{ "sbe", 0, sbe_cmd, sbe_parse, sbe_help, sbe_usage },
	{ "search", 0, search_cmd, search_parse, search_help, search_usage },
	{ "sema", 0, sema_cmd, sema_parse, sema_help, sema_usage },
	{ "set", 0, set_cmd, set_parse, set_help, set_usage },
	{ "sh", 0, shell_cmd, shell_parse, shell_help, shell_usage },
	{ "sizeof", 0, sizeof_cmd, sizeof_parse, sizeof_help, sizeof_usage },
	{ "slpproc", 0, slpproc_cmd, slpproc_parse, slpproc_help, slpproc_usage },
	{ "lsnode", 0, lsnode_cmd, lsnode_parse, lsnode_help, lsnode_usage },
	{ "soc", "socket" },
	{ "socket", 0, socket_cmd, socket_parse, socket_help, socket_usage },
	{ "stack", 0, stack_cmd, stack_parse, stack_help, stack_usage },
	{ "stat", 0, stat_cmd, stat_parse, stat_help, stat_usage },
	{ "sthread", 0, sthread_cmd, sthread_parse, sthread_help, sthread_usage },
	{ "str", "stream" },
	{ "strace", 0, strace_cmd, strace_parse, strace_help, strace_usage },
	{ "stream", 0, stream_cmd, stream_parse, stream_help, stream_usage },
	{ "string", "strings" },
	{ "strings", 0, string_cmd, string_parse, string_help, string_usage },
	{ "strst", "strstat" },
	{ "strstat", 0, strstat_cmd, strstat_parse, strstat_help, strstat_usage },
	{ "struct", 0, struct_cmd, struct_parse, struct_help, struct_usage },
	{ "swap", 0, swap_cmd, swap_parse, swap_help, swap_usage },
	{ "sym", "symbol" },
	{ "symbol", 0, symbol_cmd, symbol_parse, symbol_help, symbol_usage },
	{ "t", "trace" },
	{ "tcp", "tcpcb" },
	{ "tcpcb", 0, tcpcb_cmd, tcpcb_parse, tcpcb_help, tcpcb_usage },
	{ "tlb", "tlbdump" },
	{ "tlbdump", 0, tlb_cmd, tlb_parse, tlb_help, tlb_usage },
	{ "trace", 0, trace_cmd, trace_parse, trace_help, trace_usage },
	{ "type", 0, type_cmd, type_parse, type_help, type_usage },
	{ "un", "unpcb" },
	{ "unpcb", 0, unpcb_cmd, unpcb_parse, unpcb_help, unpcb_usage },
	{ "unset", 0, unset_cmd, unset_parse, unset_help, unset_usage },
	{ "uthread", 0, uthread_cmd, uthread_parse, uthread_help, uthread_usage },
	{ "ut", "utrace" },
	{ "utrace", 0, utrace_cmd, utrace_parse, utrace_help, utrace_usage },
	{ "vertex", 0, vertex_cmd, vertex_parse, vertex_help, vertex_usage },
	{ "vfs", 0, vfs_cmd, vfs_parse, vfs_help, vfs_usage },
	{ "vnode", 0, vnode_cmd, vnode_parse, vnode_help, vnode_usage },
	{ "vproc", 0, vproc_cmd, vproc_parse, vproc_help, vproc_usage },
	{ "vsocket", 0, vsocket_cmd, vsocket_parse, vsocket_help, vsocket_usage },
	{ "vtop", 0, vtop_cmd, vtop_parse, vtop_help, vtop_usage },
	{ "w", "walk" },
	{ "walk", 0, walk_cmd, walk_parse, walk_help, walk_usage },
	{ "whatis", 0, whatis_cmd, whatis_parse, whatis_help, whatis_usage },
	{ "xthread", 0, xthread_cmd, xthread_parse, xthread_help, xthread_usage },
	{ "zone", 0, zone_cmd, zone_parse, zone_help, zone_usage },
	{ (char *)0 }
};

/*
 * parse_flags() -- Parse the command line flag information.
 */
int
parse_flags(command_t *cmd, int index, int flags)
{
	int extra, i = 0, j, k;

	have_args = 0;
	pfile[0] = 0;
	while (cmd->args[i]) {
		extra = 0;
		if ((cmd->args[i][0] == '-') && (strlen(cmd->args[i]) > 1)) {
			switch (cmd->args[i][1]) {
				case 'a' :
					if (flags & C_ALL) {
						cmd->flags |= C_ALL;
						break;
					}
					return (0);

				case 'c' :
					if (flags & C_COMMAND) {
						cmd->flags |= C_COMMAND;
						break;
					}
					return (0);

				case 'd' :
					if (flags & C_DECIMAL) {
						cmd->flags |= C_DECIMAL;
						break;
					}
					return (0);

				case 'f' :
					if (flags & C_FULL) {
						cmd->flags |= C_FULL;
						break;
					}
					return (0);

				case 'k' :
					if (flags & C_KTHREAD) {
						cmd->flags |= C_KTHREAD;
						break;
					}
					return (0);

				case 'l' :
					if (flags & C_LIST) {
						cmd->flags |= C_LIST;
						break;
					}
					return (0);

				case 'm' :
					if (flags & C_MASK) {
					    int nchars;
						k_uint_t value;
					    char *s, w[9];

					    cmd->flags |= C_MASK;

					    if (i == (cmd->nargs - 1)) {
							return (0);
					    }

					    /* Fill mask with all zeros */
					    bzero(mask, sizeof(mask));
					    nchars = strlen(cmd->args[i+1]);
					    if (nchars > MAX_SEARCH_SIZE)
						nchars = MAX_SEARCH_SIZE;


					    /* Check to see if the hex value 
					     * provided in pattern has a
					     * 0x prefix. If it does, then 
					     * strip it off (it messes up
					     * mask and nbytes count).
					     */
					    if (nchars > 2 && 
						(!strncmp(cmd->args[i+1], 
							  "0X", 2) || 
						 !strncmp(cmd->args[i+1], 
							  "0x", 2))) 
					      {
						nchars -= 2;
						bcopy (&cmd->args[i+1][2], 
						       cmd->args[i+1], nchars);
						cmd->args[i+1][nchars] = 0;
					      }


					    s = cmd->args[i+1];
					    j = 0;
					    while (j < nchars / 8) {
							strncpy(w, s, 8);
							w[8] = 0;
							GET_VALUE(w, &value);
							if (KL_ERROR) {
								fprintf(cmd->fp, "%s: invalid mask value\n", w);
								return (0);
							}
							mask[j] = (uint)value;
							j++;
							s += 8;
					    }
					    if (nchars % 4) {
							for (k = (nchars % 4) * 2; k < 8; k++) {
								w[k] = '0';
							}
							w[8] = 0;
							GET_VALUE(w, &value);
							if (KL_ERROR) {
								fprintf(cmd->fp, "%s: invalid mask value\n", w);
								return (0);
							}
							mask[j] = (uint)value;
					    }
					    extra = 1;
					    break;
					}
					return (0);

				case 'n' :
					if (flags & C_NEXT) {
						cmd->flags |= C_NEXT;
					    break;
					}
					return (0);

				case 'o' :
					if (flags & C_OCTAL) {
						cmd->flags |= C_OCTAL;
					    break;
					}
					return (0);

				case 'u' :
					if (!(flags & C_UTHREAD)) 
					{
						return (0);
					}

					cmd->flags |= C_UTHREAD;
					if (i == (cmd->nargs - 1)) 
					{
						return (0);
					} 
					else if (i != (cmd->nargs - 1)) 
					{
						strcpy(pfile, cmd->args[i+1]);
						/*
						 * What is this "extra" ?.
						 * This is causing a memory leak.
						 * XXX - to get rid of the leak 
						 *       do a free_block here.
						 */
						free_block((k_ptr_t)
							   cmd->args[i+1]);
						extra = 1;
						break;
					}
					return (0);

				case 'p' :
					if (!(flags & C_PROC)) {
						return (0);
					}

					cmd->flags |= C_PROC;
					if (i == (cmd->nargs - 1)) {
						return (0);
					} else if (i != (cmd->nargs - 1)) {
						strcpy(pfile, cmd->args[i+1]);
						/*
						 * What is this "extra" ?.
						 * This is causing a memory leak.
						 * XXX - to get rid of the leak 
						 *       do a free_block here.
						 */
						free_block((k_ptr_t)
							   cmd->args[i+1]);
						extra = 1;
						break;
					}
					return (0);

				case 's' :
					if (flags & C_STRUCT) {
						cmd->flags |= C_STRUCT;
						break;
					}
					return (0);

				case 'v' :
					if (flags & C_VERTEX) {
						cmd->flags |= C_VERTEX;
						break;
					}
					return (0);

				case 'w' :
					if (!(flags & C_WRITE)) {
						return (0);
					}

					cmd->flags |= C_WRITE;
					if (i == (cmd->nargs - 1)) {
						fprintf(cmd->fp, "No ofile specified!\n");
						return(-1);
					} else if (i != (cmd->nargs - 1)) {
						strcpy(ofile, cmd->args[i+1]);
						extra = 1;
						break;
					}
					return (0);

				case 'x' :
					if (flags & C_HEX) {
						cmd->flags |= C_HEX;
						break;
					}
					return (0);

				case 'B' :
					if (flags & C_BYTE) {
						cmd->flags |= C_BYTE;
						break;
					}
					return (0);

				case 'D' :
					if (flags & C_DWORD) {
						cmd->flags |= C_DWORD;
						break;
					}
					return (0);

				case 'H' :
					if (flags & C_HWORD) {
						cmd->flags |= C_HWORD;
						break;
					}
					return (0);

				case 'S' :
					if (flags & C_SIBLING) {
						cmd->flags |= C_SIBLING;
						break;
					}
					return (0);

				case 'W' :
					if (flags & C_WORD) {
						cmd->flags |= C_WORD;
						break;
					}
					return (0);

				default  :
					have_args++;
					return (0);
			}
			free_block((k_ptr_t)cmd->args[i]);
			for (j = i; j < cmd->nargs - extra; j++) {
				cmd->args[j] = cmd->args[j + 1 + extra];
			}
			cmd->nargs -= (1 + extra);
		} else if (cmd->args[i][0] == '|') {
			if (strlen(cmd->args[i]) == 1) {
				strcpy(cmd->pipe_cmd, cmd->args[i + 1]);
				strcat(cmd->pipe_cmd, " ");
				for (j = i + 2; j < cmd->nargs; j++) {
					strcat(cmd->pipe_cmd, cmd->args[j]);
					if (j < cmd->nargs - 1) {
						strcat(cmd->pipe_cmd, " ");
					}
				}
			} else {
				strcpy(cmd->pipe_cmd, &cmd->args[i][1]);
				strcat(cmd->pipe_cmd, " ");
				for (j = i + 1; j < cmd->nargs; j++) {
					strcat(cmd->pipe_cmd, cmd->args[j]);
					if (j < cmd->nargs - 1) {
						strcat(cmd->pipe_cmd, " ");
					}
				}
			}
			cmd->nargs -= (cmd->nargs - i);
			free_block((k_ptr_t)cmd->args[i]);
			cmd->args[i] = (char*)0;
		} else {
			have_args++;
			i++;
		}
	}
	return (1);
}

/*
 * parse_cmd() -- Parse the flags that are passed in based on the command
 *                given and the valid options.  Note that we only mark the
 *                conditionals based on what is given to us.  Some commands
 *                will ignore the flags we set, but at least we set them.
 */
int
parse_cmd(command_t *cmd, int index, FILE *ofp)
{
	char *p;
	int i = 0, j, flags;

	/* initialize the output file, pipe command argument, and all flags */
	ofile[0] = 0;
	cmd->pipe_cmd[0] = 0;
	cmd->flags = 0;

	/* set the output file in case we call the help or usage function */
	cmd->fp = ofp;

	flags = (*cmdset[index].cmdparse)(*cmd);

	if (flags & C_SELF) {
		/* self-parsing flags ... do nothing */
	} else {
		if (!parse_flags(cmd, index, flags)) {
			return (0);
		}

		/* make sure arguments are correct 
		 */
		if ((flags & C_TRUE) && !have_args) {

			/* The C_PROC flag overrides the C_TRUE flag.
			 */
			if (!(flags & C_PROC) || !pfile) {
				return(0);
			}
		}
		else if ((flags & C_FALSE) && have_args) {
			return(0);
		}
	}

	/* Check to see if pager_flag is set. If it is, set a pipe command
	 * to output to page. If there already is a pipe command, fail with
	 * an error message (can't do both).
	 */
	if (pager_flag) {
		if (strlen(cmd->pipe_cmd)) {
			fprintf(ofp, "Can not use pager with pipe command\n");
			return (0);
		}

		/* set the pager based on the PAGER environment, if possible 
		 */
		if (p = getenv("PAGER")) {
			sprintf(cmd->pipe_cmd, "%s", p);
		} else {
			sprintf(cmd->pipe_cmd, "/usr/bin/pg");
		}
	}

	/* cannot use -w flag with pipe command 
	 */
	if (strlen(cmd->pipe_cmd)) {
		if ((cmd->ofp = popen(cmd->pipe_cmd, "w")) == NULL) {
			fprintf(KL_ERRORFP, "Could not execute cmd (%s)\n", 
				cmd->pipe_cmd);
			return (0);
		}
		KL_ERRORFP = cmd->ofp;
		cmd->flags |= C_PIPE;
		setbuf(cmd->ofp, NULL);
	} 
	else if (strlen(ofile)) {
		if((cmd->ofp = fopen(ofile, "a")) == (FILE*)0) {
			fprintf(KL_ERRORFP, "Cannot open file :%s\n", ofile);
			return (0);
		}
		KL_ERRORFP = cmd->ofp;
		cmd->flags |= C_WRITE;
	} 
	else {
		cmd->ofp = ofp;
	}

	/* print out debugging information 
	 */
	if (strlen(cmd->pipe_cmd) && (klib_debug > 4)) {
		fprintf(cmd->ofp, "set_flags: pipe_cmd=%s\n", cmd->pipe_cmd);
	}

	/* return 1 for valid parse 
	 */
	return (1);
}

/*
 * get_cmdline()
 */
char *
get_cmdline()
{
	int i = 0, j = 0;
	char *cmdline, *tline = (char *)NULL, *get_history_event(), *readline();
	static char *line = (char *)NULL;

	cmdline = (char *)alloc_block(256, B_PERM);

	if (line != (char *)NULL) {
		free(line);
		line = (char *)NULL;
	}
	while (!i) {
		line = readline("\n>> ");
		if ((line != (char *)NULL) && (line[0] != '\n') && (line[0] != '\0')) {
			tline = get_history_event(line, &j, 0);
			if (tline != (char *)NULL) {
				fprintf(KL_ERRORFP, "(%s = %s)\n", line, tline);
				for (j = 0; j < strlen(tline); j++) {
					if (!isspace(tline[j])) {
						break;
					}
				}
				add_history(&tline[j]);
				strncpy(cmdline, &tline[j], 255);
			} 
			else {
				for (j = 0; j < strlen(line); j++) {
					if (!isspace(line[j])) {
						break;
					}
				}
				add_history(&line[j]);
				strncpy(cmdline, &line[j], 255);
			}
			i++;
		}
		else if (line == NULL)
		{
			exit(-1);
		}
	}
	return(cmdline);
}

/*
 * get_cmd() -- Get a command on input 
 *
 *   Use readline() from the GNU libraries.
 */
void
get_cmd(command_t *c, char *s)
{
	int i = 0, j = 0, len;
	char *cp, *ecp, *tline = (char *)NULL, *get_history_event(), *readline();
	char *cmdline;
	static char *line = (char *)NULL;

	if (!s) {
		cmdline = get_cmdline();
		strcpy(c->com, cmdline);
		free_block((k_ptr_t)cmdline);
	} 
	else {
		strncpy(c->com, s, 255);
	}

	/*
	 * Parse the input comamnd line and separate into arguments. If 
	 * an argument begins with a double quote ('"'), include all text
	 * through the next double quote (or end of line) in the argument.
	 */
	i = 0;
	cp = c->com;

	/* Get the command name
	 */
	while (*cp != ' ') {
		if (*cp == NULL) {
			c->args[0] = 0;
			c->nargs = 0;
			return;
		}
		cp++;
	}
	*cp++ = 0;
	while (*cp == ' ') {
		cp++;
	}
	if (*cp == NULL) {
		c->args[0] = 0;
		c->nargs = 0;
		return;
	}

	/* Now get the arguments (if there are any)
	 */
	while (*cp != NULL) {
		if (i == 128) {
			if (klib_debug) {
				fprintf(KL_ERRORFP, "Too many command line arguments!\n");
			}
			c->nargs = i;
			return;
		}
		if (*cp == '\"') {
			ecp = cp + 1;
			while (*ecp != '\"') {
				if (*ecp == NULL) {
					break;
				}
				ecp++;
			}
			len = (uint)ecp - (uint)cp + 1;
			ecp++;
		} 
		else {
			ecp = cp;

			/* Step over the non-blank characters
			 */
			while (*ecp != ' ') {
				if ((*ecp == NULL) || (*ecp == '\"')) {
					break;
				}
				ecp++;
			}
			len = (uint)ecp - (uint)cp;
		}
		c->args[i] = alloc_block(len + 1, B_TEMP);
		bcopy(cp, c->args[i], len);
		c->args[i][len] = 0;
		i++;

		/* If we have reached the end of the input line then
		 * return.
		 */
		if (*ecp == NULL) {
			if (i <= 128) {
				c->args[i] = 0;
			}
			c->nargs = i;
			return;
		}

		cp = ecp;
		while (*cp == ' ') {
			cp++;
		}
	}
	c->args[i] = 0;
	c->nargs = i;
}

/*
 * clean_cmd()
 */
void
clean_cmd(command_t *cmd) 
{
	int i;

	for (i = 0; i < cmd->nargs; i++) {
		free_block((k_ptr_t)cmd->args[i]);
	}
}

/*
 * cat_cmd_args()
 */
char *
cat_cmd_args(command_t cmd)
{
	int i;
	char *cmdstr;

	cmdstr = alloc_block(1024, B_TEMP);

	for (i = 0; i < cmd.nargs; i++) {
		strcat (cmdstr, cmd.args[i]);
		if (i < (cmd.nargs - 1)) {
			switch(cmd.args[i +1][0]) {

				case '*' :
				case '-' :
				case '+' :
				case '/' :
				case '%' :
				case '|' :
				case '(' :
				case ')' :
				case '&' :
				case '^' :
				case '.' :
				case '<' :
				case '>' :
					break;

				default:
					switch(cmdstr[strlen(cmdstr) -1]) {

						case '*' :
						case '-' :
						case '+' :
						case '/' :
						case '%' :
						case '|' :
						case '(' :
						case ')' :
						case '&' :
						case '^' :
						case '.' :
						case '<' :
						case '>' :
							break;

						default :
							strcat (cmdstr, " ");
							break;
					}
			}
		}
	}
	return(cmdstr);
}

/*
 * close_ofp() -- Close off the pipe or file information.
 */
int
close_ofp(command_t cmd)
{
	if (cmd.flags & C_PIPE) 
	{
		KL_ERRORFP = stderr;
		return(pclose(cmd.ofp));
	} else if (cmd.flags & C_WRITE) 
	{
		KL_ERRORFP = stderr;
		fclose(cmd.ofp);
	}
	return 1;
}

/*
 * run_cmd()
 */
int
run_cmd(command_t *cmd, FILE *ofp)
{
	int i;
	char *p;

	p = cmd->com;
	i = 0;
	while (cmdset[i].cmd != (char *)0) {
		if (!strcmp(cmdset[i].cmd, p)) {
			if(cmdset[i].alias != (char *)0) {
				p = cmdset[i].alias;
				i = 0;
			} 
			else {
				if (!parse_cmd(cmd, i, ofp)) {
					cmd->ofp = KL_ERRORFP;
					(*cmdset[i].cmdusage)(*cmd);
					return (-1);
				} 
				else {
					(*cmdset[i].cmdfunc)(*cmd);
					return (0);
				}
			}
		} 
		else {
			i++;
		}
	}
	return(1);
}

/*
 * run_var_cmd() 
 *
 *   Convert a command variable to a command line and try to run it.
 */
int
run_var_cmd(command_t *cmd, variable_t *vp, FILE *ofp)
{
	int ret;
	char *c, *t, *cmdline, *lasts = (char*)NULL;

	cmdline = (char *)alloc_block(strlen(vp->v_exp) + 1, B_TEMP);
	strcpy(cmdline, vp->v_exp);
	c = cmdline;
	t = next_command(&c);
	while (*t) {
		t += strspn(t, " ");
		get_cmd(cmd, t);
		if (run_cmd(cmd, ofp)) {
			clean_cmd(cmd);
			free_block((k_ptr_t)cmdline);
			return(1);
		}
		t = next_command(&c);
	}
	return(0);
}

/*
 * checkrun_cmd() -- Check that a command is valid, and run it, if possible.
 */
int
checkrun_cmd(command_t cmd, FILE *ofp)
{
	int ret;
	char *c, *t, *cmdline, *lasts = (char*)NULL;
	variable_t *vp;

	if (ret = run_cmd(&cmd, ofp)) {
		if (ret == 1) {
			/* We didn't match with one of the standard commands. So,
			 * check and see if there are any eval variables that match
			 * for this name (an eval variable can be a command line).
			 */
			if (vp = find_variable(vtab, cmd.com, V_COMMAND)) {
				cmdline = (char *)alloc_block(strlen(vp->v_exp) + 1, B_TEMP);
				strcpy(cmdline, vp->v_exp);
				c = cmdline;
				t = next_command(&c);
				while (*t) {
					t += strspn(t, " ");
					get_cmd(&cmd, t);
					if (run_cmd(&cmd, ofp)) {
						close_ofp(cmd);
						clean_cmd(&cmd);
						free_block((k_ptr_t)cmdline);
						return(-1);
					}
					t = next_command(&c);
				}
			}
		}
		else {
			close_ofp(cmd);
			clean_cmd(&cmd);
			return (-1);
		}
	}
	close_ofp(cmd);
	clean_cmd(&cmd);
	return (0);
}

/*
 * next_command()
 */
char *
next_command(char **next) 
{
	int length, open_quote = 0;
	char *c, *n;

	length = strlen(*next) + 1;
	c = *next;

	while (*c) {
		if (*c == '\"') {
			if (open_quote) {
				open_quote = 0;
			}
			else {
				open_quote = 1;
			}
		}
		if (*c == ';') {
			if (open_quote == 0) {
				*c = 0;
				n = *next;
				*next = (c + 1);
				return(n);
			}
		}
		c++;
	}

	/* Where at end of the command line. make sure that *next points
	 * to the NULL terminating character.
	 */
	n = *next;
	*next = c;
	return(n);
}

/*
 * process_commands() -- set up command functions.
 */
int
process_commands(FILE *ofp)
{
	int ret;
	char *c, *t, *cmdline, *lasts = (char*)NULL;
	command_t cmd;
	variable_t *vp;

	/* Process through the command set, and make sure to set up all of
	 * the command line arguments appropriately. If we receive a signal, 
	 * or when returning from executing a command, free any memory
	 * blocks that were temporarily allocated -- but not freed.
	 */
	while (1) {

		if (setjmp(jbuf)) {
			clean_strmtab();
			free_temp_blocks();
			fprintf(ofp, "\n");
		}

		cmdline = get_cmdline();
		c = cmdline;
		t = next_command(&c);
		while (*t) {
			t += strspn(t, " ");
			get_cmd(&cmd, t);
			if (ret = run_cmd(&cmd, ofp)) {
				if (ret == 1) {
					if (vp = find_variable(vtab, cmd.com, V_COMMAND)) {
						run_var_cmd(&cmd, vp, ofp);
					}
					else {
						fprintf(KL_ERRORFP, "%s: unknown command\n", cmd.com);
					}
				}
			}
			close_ofp(cmd);
			clean_cmd(&cmd);
			t = next_command(&c);
		}

		clean_strmtab();
		free_temp_blocks();
	}
}

static struct sigaction sigact;

/*
 * sigon()
 */
void
sigon()
{
	if (sigaction(SIGINT, &sigact, NULL) < 0) {
		/* XXX - need to set error
		 */
		return;
	}
	if (sigaction(SIGPIPE, &sigact, NULL) < 0) {
		/* XXX - need to set error
		 */
		return;
	}	
#ifndef ICRASH_DEBUG
	if (sigaction(SIGABRT, &sigact, NULL) < 0) {
		/* XXX - need to set error
		 */
		return;              
	}
	if (sigaction(SIGSEGV, &sigact, NULL) < 0) {
		/* XXX - need to set error
		 */
		return;              
	}
	if (sigaction(SIGBUS, &sigact, NULL) < 0) {
		/* XXX - need to set error
		 */
		return;
	}
#endif
}

/*
 * sig_setup() -- initialize the sigactions struct
 */
int
sig_setup() 
{
	/* Don't block the signal, and return verbose info 
	 */
	sigact.sa_flags = (SA_NODEFER | SA_SIGINFO);
	sigact.sa_sigaction = sig_handler; 
	if (sigemptyset(&sigact.sa_mask) < 0) {
		return(1);
	}
	sigon();
	return(0);
}

/*
 * sig_handler() -- Handle signals
 */
void
sig_handler(int sig, siginfo_t *sip, void *p)
{
	ucontext_t *up = p;       /* .h has wrong type in struct sigaction */
	greg_t *gregs = up->uc_mcontext.gregs;
	greg_t epc, sp, ra, badaddr;

	epc = gregs[CTX_EPC];
	ra = gregs[CTX_RA];
	sp = gregs[CTX_SP];
	if (sip) {
		badaddr = (greg_t)sip->si_addr;
	}

	switch (sig) {

#ifndef ICRASH_DEBUG
		case SIGABRT:           

			/* abort called (assert() failed)
			 */
			fprintf(KL_ERRORFP, "ICRASH ERROR TRAP: ABORT\n");
			fprintf(KL_ERRORFP, "  EPC=0x%llx, RA=0x%llx, SP=0x%llx, "
				"BADADDR=0x%llx\n", epc, ra, sp, badaddr);
			break;

		case SIGSEGV:           

			/* out-of-range access 
			 */
			fprintf(KL_ERRORFP, "ICRASH ERROR TRAP: SEGV ");
			switch (sip->si_code) {

				case SEGV_MAPERR:
					fprintf(KL_ERRORFP, "(address not mapped to object)\n");
					break;

				case SEGV_ACCERR:
					fprintf(KL_ERRORFP, "(invalid permissions for mapped "
						"object)\n");
					break;

				case SI_USER:
					fprintf(KL_ERRORFP, "(sent by kill or sigsend, pid %d, "
						"uid %d)\n", sip->si_pid, sip->si_uid);
					break;

				case SI_QUEUE:
					fprintf(KL_ERRORFP, "(sent by sigqueue, pid %d, uid %d)\n", 
						sip->si_pid, sip->si_uid);
					break;

				default:
					fprintf(KL_ERRORFP, "(unknown si_code = %d)\n", 
						sip->si_code);
			}
			fprintf(KL_ERRORFP, "  EPC=0x%llx, RA=0x%llx, SP=0x%llx, "
				"BADADDR=0x%llx\n", epc, ra, sp, badaddr);
			break;

		case SIGBUS:
			fprintf(KL_ERRORFP, "ICRASH ERROR TRAP: BUSERR ");
			switch (sip->si_code) {
				case BUS_ADRALN:
					fprintf(KL_ERRORFP, "(invalid address alignment)\n");
					break;

				case BUS_ADRERR:
					fprintf(KL_ERRORFP, "(non-existent physical address)\n");
					break;

				case BUS_OBJERR:
					fprintf(KL_ERRORFP, "(object specific hardware error)\n");
					break;

				case SI_USER:
					fprintf(KL_ERRORFP, "(sent by kill or sigsend, pid %d, "
							"uid %d)\n", sip->si_pid, sip->si_uid);
					break;

				case SI_QUEUE:
					fprintf(KL_ERRORFP, "(sent by sigqueue, pid %d, uid %d)\n", 
							sip->si_pid, sip->si_uid);
					break;

				default:
					fprintf(KL_ERRORFP, 
						"(unknown si_code = %d)\n", sip->si_code);
			}
			fprintf(KL_ERRORFP, "  EPC=0x%llx, RA=0x%llx, SP=0x%llx, "
				"BADADDR=0x%llx\n", epc, ra, sp, badaddr);
			break;
#endif

		case SIGINT:
			if (klib_debug) {
				fprintf(KL_ERRORFP, "Received SIGINT signal\n");
			}
			break;

		case SIGPIPE:
			if (klib_debug) {
				fprintf(KL_ERRORFP, "Received SIGPIPE signal\n");
			}
			break;

		default:
			fprintf(KL_ERRORFP, "Error:  unexpectedly got signal %d; "
				"exiting\n", sig);
			break;
	}

	/* Make sure we get ready to trap signals again
	 */
	sigon();

	/* Go back to handling commands.
	 */
	longjmp(jbuf, 0);
}
