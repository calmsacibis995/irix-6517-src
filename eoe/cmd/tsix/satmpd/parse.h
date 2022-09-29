#ifndef PARSE_HEADER
#define PARSE_HEADER

#ident "$Revision: 1.1 $"

typedef int (*pl_quote_func) (int);
typedef int (*pl_delim_func) (int);

char  *skipfield (char *, int);
char **parse_line (const char *, pl_quote_func, pl_delim_func, unsigned int *);
void   parse_free (char **);

#endif /* PARSE_HEADER */
