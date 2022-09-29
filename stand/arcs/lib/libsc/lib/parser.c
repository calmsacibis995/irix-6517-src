 #ident "$Revision: 1.59 $"

/*
 * parser.c -- command parser
 */

#include <sys/debug.h>
#include <saioctl.h>
#include <parser.h>
#include <setjmp.h>
#include <stringlist.h>
#include <ctype.h>
#include <arcs/io.h>
#include <arcs/errno.h>
#include <libsc.h>
#include <libsc_internal.h>

#define	streq(x,y)	(strcmp(x, y) == 0)

struct cmd_table *lookup_cmd(struct cmd_table *, char *);
static char *get_word(char **);
extern jmp_buf perr_buf;

/*
 * command_parser -- get input line and vector to appropriate command routine
 */
void
command_parser(struct cmd_table *cmd_table, char *prompt, int search_path,
	struct cmd_table *parent_table)
{
	register struct cmd_table *ct;
	static struct edit_linebuf elinebuf;
	char *linebuf;
	struct string_list argv;
	LONG err;
	int argc;

	for (;;) {
		setjmp(perr_buf);
		ioctl(StandardIn,TIOCSETXOFF,1);
		printf("%s", prompt);
		if ((linebuf = edit_gets(&elinebuf)) == 0)
			return;
	        argc = _argvize(expand(linebuf, 0), &argv);
		if (argc == 0)
			continue;

		ct = lookup_cmd(cmd_table, argv.strptrs[0]);
		if (!ct && parent_table)
			ct = lookup_cmd(parent_table, argv.strptrs[0]);
		/*
		 * if not an internal command try searching path
		 * for a file to boot, if search_path is non-zero
		 */
		if (!ct) {
			if (search_path) {
				setenv ("kernname", argv.strptrs[0]);
				if (err = Execute(argv.strptrs[0],
						(LONG)argv.strcnt,
						argv.strptrs,environ))
                                                /* else error printed by loader already */
                                                /* so we know about errors when files are */
                                                /* found but aren't executable */
                                                if(err == ENOENT)
						printf("Unable to execute %s: %s\n",
						argv.strptrs[0],
						arcs_strerror(err));
				continue;
			}
			printf("%s: Command not found.\n", argv.strptrs[0]);
			continue;
		}
		if ((*ct->ct_routine)(argc, argv.strptrs, environ, cmd_table))
			usage(ct);
	}
}

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

		if((word = get_word(&linebuf)) == NULL)
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
	register char c;
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
		parse_error("Line too long");
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
	char *sp;

	for (ct = cmd_table; ct->ct_string; ct++) {
		sp = ct->ct_string;
		if (*sp == '.')
		    sp++;
		if (streq(sp, cp))
			return(ct);
	}
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
/*ARGSUSED*/
int
help(int argc, char **argv, char **envp, struct cmd_table *cmd_table)
{
	register struct cmd_table *ct;

	if (argc > 1) {
		while (--argc > 0) {
			argv++;
			ct = lookup_cmd(cmd_table, *argv);
			if (ct)
				printf("%s\n", ct->ct_usage);
			else
				printf("Not a command: %s\n", *argv);
		}
		return(0);
	}
	printf("Commands:\n");
	for (ct = cmd_table; ct->ct_string; ct++) {
		if (*ct->ct_string == '.')
#ifndef DEBUG
			continue		/* remove "silent" commands */
#endif
			;
		printf("\t%s\n", ct->ct_usage);
	}
#ifdef DEBUG
	printf("\nCOMMAND FLAGS\n");
	printf("\tcommands that reference memory take widths of:\n");
	printf("\t\t-b -- byte, -h -- halfword, -w -- word (default)\n");
	printf("\tRANGE's are specified as one of:\n");
	printf("\t\tBASE_ADDRESS#COUNT\n");
	printf("\t\tSTART_ADDRESS:END_ADDRESS\n");
	printf("Erase single characters by CTRL-H or DEL\n");
	printf("Rubout entire line by CTRL-U\n");
#endif
	return(0);
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
	register __psint_t sizemask;

	ASSERT((size & (size - 1)) == 0);
	cp = atob_ptr(cp, &ra->ra_base);
	sizemask = ~(size - 1);

	ra->ra_base &= sizemask;
	switch (*cp) {
	  case ':':

#if _MIPS_SIM == _ABI64
		if (*atobu_L(++cp, (unsigned long long *)&ra->ra_count))
#else
                if (*atobu(++cp, (unsigned *)&ra->ra_count))
#endif
			return 0;

                /*
                 * ra_count holds the range's limit address, so turn it into
                 * a size-unit count.
                 */
                ra->ra_count = ((ra->ra_count&sizemask) - ra->ra_base) / size;
                break;

	  case '#':

#if _MIPS_SIM == _ABI64

		if (*atobu_L(++cp, (unsigned long long *)&ra->ra_count))
                        return 0;
#else
		if (*atobu(++cp, (unsigned *)&ra->ra_count))
			return 0;
#endif
		break;

	  case '\0':
		ra->ra_count = 1;
		break;

	  default:
		return 0;
	}
	if (ra->ra_count == 0)		/* MIPS allowed count < 0 */
		ra->ra_count = 1;
	ra->ra_size = size;		/* XXX needed? */
	return 1;
}
