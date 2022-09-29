/* @[$] nl_query.c 1.0 frank Oct 26 1992
 * query user
 * internationalized !
 * this function is catalog independend !
 */

#include	<synonyms.h>
#include	<stdarg.h>
#include	<stdio.h>
#include	<limits.h>
#include	<nl_types.h>
#include	<sgi_nl.h>

/*
 * check for unique query 
 * If input doesn't match the yes/no cases, the function
 * returns -1 and ibuf contains the input string.
 *
 * flag = 0 'no' is default
 * flag = 1 'yes' is default
 *
 * ibuf must be of at least MAX_INPUT size
 */

/* parameter "q" - "cat:nbr"    (yes/no) string         */
/* parameter "qdef" - "cat:nbr"    [yes] default string    */
/* parameter "ystr" - "cat:nbr"    yes string              */
/* parameter "nstr" - "cat:nbr"    no string               */

int 
_sgi_nl_query(FILE *stream, char *ibuf, int flag, char *q,
         char *qdef, char *ystr, char *nstr)
{
	int retval;

	retval = _sgi_nl_query_fd(stream, ibuf, flag, 
				  q, qdef, ystr, nstr, stdout);
	return(retval);
}

int
_sgi_nl_query_fd(FILE *stream, char *ibuf, int flag, char *q,
	 char *qdef, char *ystr, char *nstr, FILE *fd)
{
	register char *yptr, *nptr, *qdp;
	register int i;
	register char ci;
	char format[256];		/* this is enough */

	/*
	 * make format string
	 */
	sprintf(format, "%s%s : ",
	    gettxt(q, "(%s/%s)"),
	    qdef? gettxt(qdef, "[%s]") : "%s");

	/*
	 * get "yes" and "no" string from message catalog
	 */
	yptr = gettxt(ystr, "yes");
	nptr = gettxt(nstr, "no");
	if(qdef) {
	    qdp = (flag)? yptr : nptr;
	} else
	    qdp = "";

	fprintf(fd, format, yptr, nptr, qdp);

	/*
	 * read user input and scan for unique pattern
	 */
	if(fgets(ibuf, MAX_INPUT, stream)) {
	    if(ibuf[0] != '\n') {
		for(qdp = 0, i = 0; ci = ibuf[i]; i++) {
		    if( ! yptr[i]) {
			qdp = nptr;		/* end of 'yes' string */
			break;
		    }
		    if( ! nptr[i]) {
			qdp = yptr;		/* end of 'no' string */
			break;
		    }
		    if((ci == yptr[i]) && (ci == nptr[i]))
			continue;		/* no difference */
		    if(ci == yptr[i])
			qdp = yptr;
		    if(ci == nptr[i])
			qdp = nptr;
		    break;
		}
		if( ! qdp)
			return(-1);
		while(ibuf[i]) {
			if(ibuf[i] == '\n')
				break;
			if(qdp[i] != ibuf[i])
				return(-1);	/* more/other input */
			i++;
		}
	    }
	    if(qdp == nptr)
		return(0);			/* 'no' */
	    if(qdp == yptr)
		return(1);			/* 'yes' */
	}
	return(-1);
}
