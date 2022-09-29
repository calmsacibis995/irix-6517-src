/* expand.c -- split from parser.c so code so Load() can be used in symmon.
 */

#ident "$Revision: 1.4 $"

#include <ctype.h>
#include <parser.h>
#include <libsc.h>
#include <setjmp.h>

static char *get_var(char **);
static char *get_quote(char **, char);

jmp_buf perr_buf;

/*
 * expand -- deal with environment variables and redirection
 */
char *
expand(char *cp, int ignore_esc)
{
	static char buf[LINESIZE];
	char tmp[LINESIZE];
	register char *bp;
	register char *wp;
	register char c;

	bp = buf;
	while (*cp) {
		switch (c = *cp) {

		/*
		 * someday, it would be nice to have history
		 */
		case '$':
			wp = getenv(get_var(&cp));
			if (!wp)
				parse_error("$variable not defined");
			while (*wp && bp < &buf[LINESIZE-1])
				*bp++ = *wp++;
			break;

		case '"':
			/*
			 * pay the price for recursion!
			 */
			*bp = 0;
			strcpy(tmp, buf);
			bp = &tmp[strlen(tmp)];
			wp = expand(get_quote(&cp, '"'), 1);
			if (bp < &tmp[LINESIZE-1])
				*bp++ = '"';
			while (*wp && bp < &tmp[LINESIZE-1])
				*bp++ = *wp++;
			if (bp < &tmp[LINESIZE-1])
				*bp++ = '"';
			*bp = 0;
			strcpy(buf, tmp);
			bp = &buf[strlen(buf)];
			break;

		case '\'':
			wp = get_quote(&cp, '\'');
			if (bp < &buf[LINESIZE-1])
				*bp++ = '\'';
			while (*wp && bp < &buf[LINESIZE-1])
				*bp++ = *wp++;
			if (bp < &buf[LINESIZE-1])
				*bp++ = '\'';
			break;

		case '\\':
			if (bp < &buf[LINESIZE-1]) {
				*bp++ = c;
				c = *++cp;
			}
			if (ignore_esc && c == '$')
				break;
			/* fall into default case */

		default:
			if (bp < &buf[LINESIZE-1])
				*bp++ = c;
			cp++;
			break;
		}
	}
	if (bp >= &buf[LINESIZE-1])
		parse_error("Line too long");
	*bp = 0;
	return(buf);
}

/*
 * get_var -- environment variable lexical routine
 */
static char *
get_var(char **cpp)
{
	static char buf[LINESIZE];
	char *cp;
	register char *bp;
	register char *wp;
	register char c;

	bp = buf;
	cp = *cpp + 1;	/* skip leading $ */
	if (*cp == '{') {
		wp = get_quote(&cp, '}');
		while (*wp)
			*bp++ = *wp++;
	} else
		for (; (c = *cp) && isalnum(c); cp++)
			*bp++ = c;
	*bp = 0;
	*cpp = cp;
	return(buf);
}


/*
 * get_quote -- quoted string lexical routine
 */
static char *
get_quote(char **cpp, char q)
{
	static char buf[LINESIZE];
	register char *cp;
	register char *bp;
	register char c;

	bp = buf;
	for (cp = *cpp + 1; (c = *cp) && c != q; cp++) {
		if (c == '\\') {	/* \'s are protected inside quotes */
			*bp++ = c;
			c = *++cp;
		}
		*bp++ = c;
	}
	if (c != q)
		parse_error("quote syntax");
	*bp = 0;
	*cpp = ++cp;		/* skip trailing quote mark */
	return(buf);
}

/*
 * parse_error -- deal with syntax errors gracefully
 */
void
parse_error(char *msg)
{
	printf("%s\n", msg);
	longjmp(perr_buf, 1);
}
