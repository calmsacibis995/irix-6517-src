#ident	"include/parser.h:  $Revision"
#ifndef PARSER_H
#define PARSER_H
/*
 * parser.h -- definitions for command parser routines
 */

#include <sgidefs.h>

/*
 * range structure for parse_range() result parameter
 */
struct range {
	__psint_t	ra_base;	/* range base address in bytes */
	__psint_t	ra_count;	/* number of units in range */
	int		ra_size;	/* unit size in bytes */
};

/*
 * cmd_table -- interface between parser and command execution routines
 * Add new commands by making entry here.
 */
struct cmd_table {
	char	*ct_string;		/* command name */
					/* implementing routine */
	int	(*ct_routine)(int, char *[], char *[], struct cmd_table *);	
	char	*ct_usage;		/* syntax */
};

#define CT_ROUTINE	(int (*)(int, char *[], char *[], struct cmd_table *))

#define	LINESIZE	256		/* line buffer size */

#define EDIT_HISTORY	10
struct edit_linebuf {
	char	lines[EDIT_HISTORY][LINESIZE];
	int	current;		/* current position */
};

/*
 * Public parsing operations.  The arguments are:
 *	struct cmd_table *ct;	// pointer to table or to a particular entry
 *	char	*prompt;
 *	int	dopathsearch;	// if true, use $path to find commands
 *	struct cmd_table *pct;	// parent command table for extensibility
 *	char	*commandname;
 *	char	*cp;		// pointer to range string
 *	int	size;		// unit size for range count
 *	struct range *range;	// resulting range
 *	int	*intp;		// integer result of atobu()
 *	char	*message;
 *	int	argc;		// a la main()'s arguments
 *	char	**argv;
 *
 * void command_parser(ct, prompt, dopathsearch, pct);
 * struct cmd_table *lookup_cmd(ct, commandname);
 * int parse_range(cp, size, range);
 * void usage(ct);
 */
void command_parser(struct cmd_table *, char *, int, struct cmd_table *);
struct cmd_table *lookup_cmd(struct cmd_table *, char *);
int parse_range(register char *, int, struct range *);
int help(int, char **, char **, struct cmd_table *);
char *edit_gets(struct edit_linebuf *);
void usage(struct cmd_table *);
void parse_error(char *);

#endif /* PARSER_H */
