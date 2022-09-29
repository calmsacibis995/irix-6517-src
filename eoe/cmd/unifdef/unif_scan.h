#ifndef UNIF_SCAN_H
#define UNIF_SCAN_H
/*
 * Unifdef lexical scanner interface.  The x.tab.h file is copied from
 * the yacc-generated y.tab.h file only when token definitions change.
 * Our makefile insures that it exists before compiling any source that
 * includes this file.
 */
#include "x.tab.h"

/*
 * A clearer name for the token value type.
 */
typedef YYSTYPE	tokval_t;

/*
 * Collect the next token from the specified string, or from a previously
 * specified string if the first argument is 0.  If the tokval_t pointer
 * is non-null, return token value through it.  Return a token type code
 * directly.
 */
int	scantoken(char *, tokval_t *);

/*
 * Given an #else or #endif line in a buffer, rewrite the directive to be
 * ANSI-conformant by commenting out any tokens that come after the #else
 * or #endif.  The arguments are the buffer and its size.
 */
void	ansify(char *, int);

#endif
