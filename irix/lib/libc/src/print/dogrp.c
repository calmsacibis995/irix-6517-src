/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.5 $"

#include <synonyms.h>
#include <locale.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "print.h"

#define  _min(a,b)     (a>b ? b : a)

/* ----------------------------------------------------	*
 * __do_group():					*
 * =============					*
 *							*
 * This routine implements the X/Open XPG4 feature of 	*
 * thousands grouping using the non-monetary character.	*
 *							*
 * The excerpt taken from the X/Open manual page on	*
 * 'fprintf' follows:					*
 *							*
 * The flag character quote (') causes the integer	*
 * portion of the result of a decimal conversion	*
 * (%i, %d, %u, %f, %g or %G) will be formatted with	*
 * thousands grouping characters.  For other conversions*
 * the behaviour is undefined.  The non-mometary	*
 * character is used.					*
 *							*
 *	dig_to_left:	number of digits to the left of	*
 *			decimal in the actual value of	*
 *			dval				*
 *							*
 *	strsize:    	max length of 'str' buffer.    	*
 * ----------------------------------------------------	*/
#define NDIG 82 /* from ecvt.c */

void
__do_group(char *str,
	double dval,
	int prec,
	int dig_to_left,
	int strsize,
	int maxfsig)
{
	char *s;		/* work buffer for string handling   */

#pragma set woff 1209
	/* a work buffer for the whole formatted str                     */
	char temp_buff[max(MAXDIGS, 1+max(MAXFCVT+MAXEXP, 2*MAXECVT))];
#pragma reset woff 1209

	char *t;		/* the grouping string of the locale */
	char *separator;	/* thousand separator string of the locale */
	int repeat = 0;		/* flag for repeating the grouping number */
	int sep_width;		/* the width of the thousand separator */
	int decpt;		/* a decimal number to show where the
				 * radix character is when counting
				 * from beginning
				 */
	int sign;		/* 1 for negative, 0 for positive    */
	char *b; 		/* points to the beginning of the output */
	char cvtbuf[NDIG+2];
	char *number;		/* work buffer for converted string of fcvt() */
	int   i, fcvt_prec, j, k;

	if (dig_to_left <= 0)		/* determine the precision to be used */
		fcvt_prec = _min(prec, maxfsig);
	else if (dig_to_left >= maxfsig)
		fcvt_prec = maxfsig-dig_to_left;
	else
		fcvt_prec = _min(prec, maxfsig-dig_to_left);
	number = fcvt_r(dval, fcvt_prec, &decpt, &sign, cvtbuf);

	{ 
		struct lconv *table;
		table = localeconv();
		if (table == NULL) t = NULL;
		else
		{
			t = table->grouping;
			separator = table->thousands_sep;
		}
	}

	b = &temp_buff[0] + (sizeof(temp_buff) - 1);
	*b-- = '\0';

	s = number+decpt-1; /* points to the digit preceed radix */

	if (*t) { /* grouping is defined */

		/*************************************************************/
		/* get grouping format,eg: 3;0                               */
		/*************************************************************/

		j = dig_to_left;
		repeat = 0;
		sep_width = (int)strlen(separator);
		k = sizeof(temp_buff);
		while (j > 0) {
			if (!repeat) {
				i = 0;		/* get group size */
				if (isdigit(*t+'0') ) {
					i = *t++;
				}

				if (i) {		/* group != 0 */
					k = i;		/* save current group */
				} else {
					i = k;		/* restore prev group */
					repeat++;
				}
			} else
				i = k;		/* restore prev. group */
			if (i > j)		/* this is the last group */
				i = j;
			for (; i > 0; i--, j--)      /* output the group */
				*b-- = *s--;

			/*
			 * insert separator only when it is not the last group,
			 * and the thousand separator is defined in the current
			 * locale.
			 */
			for (i = sep_width-1; j && *separator && i >= 0; i--)
				*b-- = *(separator+i);
		}
		if (strlen(b+1) <= strsize)
			strcpy(str, b+1);
		else
			strncpy(str, b+1, strsize);
	} else 
		*str = '\0';
}
