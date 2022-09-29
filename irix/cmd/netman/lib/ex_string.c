/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Construct a string expression, converting any C escapes to binary.
 */
#include <stdio.h>
#include "expr.h"

Expr *
ex_string(char *ptr, int len)
{
	Expr *ex;
	char *cp, c;
	int rem, adj, val;

	ex = expr(EXOP_STRING, EX_NULLARY, ptr);
	ex->ex_str.s_ptr = ptr;
	for (cp = ptr, rem = len; --rem >= 0; cp++) {
		c = *ptr++;
		if (c == '\\' && rem > 0) {
			adj = 1;
			c = *ptr;
			switch (c) {
			  case 'a':
				c = '\007';
				break;
			  case 'b':
				c = '\b';
				break;
			  case 'e':
				c = '\033';
				break;
			  case 'n':
				c = '\n';
				break;
			  case 'r':
				c = '\r';
				break;
			  case 't':
				c = '\t';
				break;
			  case 'x':
			  case 'X':
				if (!sscanf(ptr+1, "%2x%n", &val, &adj))
					break;
				adj++;		/* include the 'x' */
				c = val;
				break;
			  default:
				if (!sscanf(ptr, "%3o%n", &val, &adj))
					break;
				c = val;
			}
			ptr += adj;
			len -= adj;
			rem -= adj;
		}
		*cp = c;
	}
	ex->ex_str.s_len = len;
	*cp++ = *ptr;		/* copy the quote character */
	while (cp <= ptr)
		*cp++ = ' ';	/* pad with blanks */
	return ex;
}
