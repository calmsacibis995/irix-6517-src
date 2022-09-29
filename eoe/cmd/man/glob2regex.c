/*
**  NAME
**	glob2regex - convert a shell-style glob expr to a regular expr
**
**  SYNOPSIS
*/

char*
glob2regex(char* out, const char* in)

/*
**  DESCRIPTION
**	glob2regex converts the glob expression character string 'in' to a
**	the regex(3X) style regular expression.  This conversion permits the
**	use of a single expression matching algorithm yet allows globbing
**	style input.  The globbing style is similar to csh(1) globbing style
**	with the exception that curly brace '{' or'ing notation is not
**	supported.  Also ~ is ignored as it only applies to path names.
**
**	A pointer to 'out' is returned as a matter of convenience as is the
**	style for other string(3) routines.
**
**  SEE ALSO
**	csh(1), regex(3X), string(3).
**
**  AUTHOR
**	Dave Ciemiewicz (Ciemo)
**
**  COPYRIGHT (C) Silicon Graphics, 1991, All rights reserved.
**
*/

{
    char* retval = out;
    const char* i;

    for(i=in; *i; i++) {
	switch (*i) {
	/*
	** Ignore certain regex meta characters which no correspondence in
	** globbing.
	*/
	case '^':
	case '$':
	case '+':
	case '.':
	case '(':
	case ')':
	case '{':	/* Curly brace or'ing notation is not supported */
	case '}':
	    *out = '\\'; out++;
	    *out = *i; out++;
	    break;

	/*
	** Map single character match
	*/
	case '?':
	    *out = '.'; out++;	
	    break;

	/*
	** Map any string of characters match
	*/
	case '*':
	    *out = '.'; out++;	
	    *out = '*'; out++;	
	    break;

	/*
	** All other characters are treated as is.  This fall through also
	** supports the square bracket '[' match character set notation which
	** is the same for both globbing and regex.
	*/
	default:
	    *out = *i; out++;
	}
    }
    *out = *i;
    return retval;
}

#ifdef TEST
main()
{
    char* buf1[512];
    char* buf2[512];

    while (gets(buf1)) {
	glob2regex(buf2, buf1);
	printf("%s\n", buf2);
    }
}
#endif
