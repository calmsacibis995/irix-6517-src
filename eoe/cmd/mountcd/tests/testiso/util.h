/*
 *	util.h
 *
 *	Description:
 *		Header for util.c++
 *
 *	History:
 *      rogerc      04/12/91    Created
 */

/*
 * These are necessary because the ISO committe didn't have the grace
 * to provide structure definitions that would obey our alignment
 * restrictions.
 */
#define CHARSTOLONG(chars) ((chars)[0] << 24 | (chars)[1] << 16\
| (chars)[2] << 8 | (chars)[3])
#define CHARSTOSHORT(chars) ((chars)[0] << 8 | (chars)[1])

extern char *progname;

void
error( int err = 0, char *str = progname );

char *
to_unixname( char *name, int length, int notranslate );
