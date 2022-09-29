 /*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

/*
 * parser.c -- command parser
 */

/*#include <sys/param.h>*/
#include <sys/types.h>
#include <sys/systm.h>
#include <saio.h>
#include "parser.h"
#include <setjmp.h>
#include <stringlist.h>
#include <ctype.h>
#include "mp.h"
#include "dbgmon.h"	/* for ERROR_RANGE,ADDR_RANGE defines */
#include <libsc.h>
#ifdef NETDBX
#include "coord.h"
#endif /* NETDBX */

#define	streq(x,y)	(strcmp(x, y) == 0)

static char *get_word(char **cpp);
static int repeat_command (char *);

/*
 * command_parser -- get input line and vector to appropriate command routine
 */

/*ARGSUSED*/
void
command_parser(struct cmd_table *ctbl, char *prompt, int x, struct cmd_table *y)
{
	int argc;
	struct cmd_table *ct, *found_in_tbl;
	static struct string_list argv; /* keep stack small */
	static struct edit_linebuf elinebuf;
	static char savebuf[LINESIZE+10];
	static char rprompt[LINESIZE+10];
	char *linebuf;

	ACQUIRE_USER_INTERFACE();

	strcpy(rprompt, prompt);
	savebuf[0] = '\0';

	for ( ; ; ) {
		puts(rprompt);
		linebuf = edit_gets(&elinebuf);
		if (linebuf == NULL)
			continue;
		argc = _argvize(linebuf, &argv);
		if (argc == 0) { /* check for saved repeatable command */
			if (savebuf[0] && (argc=_argvize(savebuf, &argv))) {
				if (ct = lookup_cmd(ctbl, argv.strptrs[0]))
					found_in_tbl = ctbl;
				else if (ct=lookup_cmd(pd_ctbl, argv.strptrs[0]))
					found_in_tbl = pd_ctbl;
				else
					found_in_tbl = NULL;
			
				if (found_in_tbl)
					if (!ct->ct_routine(argc, argv.strptrs,
							0, found_in_tbl))
						continue;
			}
		} else {
			if (ct=lookup_cmd(ctbl, argv.strptrs[0]))
				found_in_tbl = ctbl;
			else if (ct=lookup_cmd(pd_ctbl, argv.strptrs[0]))
				found_in_tbl = pd_ctbl;
			else
				found_in_tbl = NULL;

			if (!found_in_tbl) {
				if (_dbxdump(linebuf) &&
						_kpcheck(argc,argv.strptrs,1))
					printf("%s: Command not found.\n",
							argv.strptrs[0]);
			} else if (ct->ct_routine(argc, argv.strptrs, 0,found_in_tbl))
				usage(ct);
			else if (*linebuf && repeat_command(argv.strptrs[0])){
				strcpy (rprompt, "DBG<");
				rprompt[4] = *argv.strptrs[0];
				rprompt[5] = '\0';
				strcat (rprompt, ">: ");
				strcpy (savebuf, linebuf);
				continue;
			}
		}
		strcpy(rprompt, prompt);
		savebuf[0] = '\0';
	}
}

#ifdef NETDBX
/*ARGSUSED*/
void
execute_command(struct cmd_table *ctbl, char *cmdline)
{
	int argc;
	struct cmd_table *ct, *found_in_tbl;
	static struct string_list argv; /* keep stack small */

	argc = _argvize(cmdline, &argv);
	if (ct=lookup_cmd(ctbl, argv.strptrs[0]))
		found_in_tbl = ctbl;
	else if (ct=lookup_cmd(pd_ctbl, argv.strptrs[0]))
		found_in_tbl = pd_ctbl;
	else
		found_in_tbl = NULL;

	if (!found_in_tbl) {
		if (_dbxdump(cmdline) && _kpcheck(argc,argv.strptrs,1))
			printf("%s: Command not found.\n", argv.strptrs[0]);
	} else if (ct->ct_routine(argc, argv.strptrs, 0,found_in_tbl))
		usage(ct);
}
#endif /* NETDBX */

/*
 * _argvize -- break input line into tokens
 */
int
_argvize(char *linebuf, struct string_list *slp)
{
	register int argc;
	register char *word;

	init_str(slp);
	for (argc = 0; *linebuf; argc++) {
		if ((word = get_word(&linebuf)) == NULL)
			return 0;
		new_str1(word, slp);
	}

	return(argc);
}

/*
 * get_word -- break line into "words"
 */
static char *
get_word(char **cpp)
{
	static char word[LINESIZE];
	register char *wp;
	register int c;
	char *cp;

	cp = *cpp;

	while (isspace(*cp))
		cp++;

	wp = word;
	while ((c = *cp) && !isspace(c) && wp < &word[LINESIZE-1]) {
		switch (c) {

		case '"':
		case '\'':
			cp++;	/* skip opening quote */

			while (*cp && *cp != c && wp < &word[LINESIZE-2])
				*wp++ = *cp++;

			if (*cp == c) /* skip closing quote */
				cp++;
			break;

		default:
			*wp++ = *cp++;
			break;
		}
	}

	while (isspace(*cp))
		cp++;

	*cpp = cp;
	*wp = 0;
	if (wp == &word[LINESIZE-1])
		printf("Line too long");
	else if (wp == word)		/* ignore lines of whitespace */
		return(NULL);
	return(word);
}

/*
 * lookup_cmd -- search cmd table
 */
struct cmd_table *
lookup_cmd(struct cmd_table *cmd_table, char *cp)
{
	register struct cmd_table *ct;

	for (ct = cmd_table; ct->ct_string; ct++)
		if (streq(ct->ct_string, cp))
			return(ct);
	return((struct cmd_table *)0);
}

/*
 * usage -- print usage line for a command
 */
void
usage(struct cmd_table *ct)
{
	printf("Usage: %s\n", ct->ct_usage);
}

/*
 * help -- print usage line for all commands
 */
/* ARGSUSED */
int
help(int argc, char **argv, char **envp, struct cmd_table *cmd_table)
{
	register struct cmd_table *ct;

	if (argc > 1) {
		while (--argc > 0) {
			argv++;
			if ((ct = lookup_cmd(cmd_table, *argv)) != 0 ||
			    (ct = lookup_cmd(pd_ctbl, *argv)) != 0)
				printf("%s\n", ct->ct_usage);
			else if (! _kpcheck(argc, argv, 0))
				printf("%s: Kernel function "
				       "(use kp or ?? for list)\n", *argv);
			else
				printf("%s: Not a command\n", *argv);
		}
		return(0);
	}
	printf("COMMANDS:\n");
	for (ct = cmd_table; ct->ct_string; ct++)
		printf("\t%s\n", ct->ct_usage);

	for (ct = pd_ctbl; ct->ct_string; ct++)
		printf("\t%s\n", ct->ct_usage);
	printf("\tUse ?? for list of kernel functions\n");
	printf("\tRANGEs are specified as one of:\n");
	printf("\t\tBASE_ADDRESS#COUNT\n");
	printf("\t\tSTART_ADDRESS:END_ADDRESS\n");
	return(0);
}

/*
 * repeat_command -- check to see if command is a good one to repeat
 *
 */
static int
repeat_command (char *arg0)
{
#ifdef SN0
	if ( ((*arg0=='s' || *arg0=='S' ) && *(arg0+1) == '\0') ||
#else
	if ( ((*arg0=='s' || *arg0=='S' || *arg0=='c') && *(arg0+1) == '\0') ||
#endif
							!strcmp(arg0,"goto") )
		return 1;

	return 0;
}


/*
 * Parse a range expression having one of the following forms:
 *
 *	base:limit	base and limit are byte-addresses
 *	base#count	base is a byte-address, count is in size-units
 *	base		byte-address, implicit count of 1 size-unit
 *
 * Return 0 on syntax error, 1 otherwise.  Return in ra->ra_base the base
 * address, truncated to be size-aligned.  Return in ra->ra_count a size-unit
 * count, and the size in bytes in ra->ra_size.
 *
 * NB: size must be a power of 2.
 */
int
parse_range(register char *cp, register int size, register struct range *ra)
{
	numinp_t baseaddr;
	int count;

	ASSERT((size & (size - 1)) == 0);
	count = 0;
	if (parse_sym_range(cp, &baseaddr, &count, size))
		{
			ra->ra_count = count;
			return 0;
		}


	ra->ra_count = count;
	ra->ra_base = (long)baseaddr;
	if (ra->ra_count == 0)
		ra->ra_count = 1;
	ra->ra_size = size;
	return 1;
}
