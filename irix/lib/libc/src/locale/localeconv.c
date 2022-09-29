/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/localeconv.c	1.8"
#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <locale.h>
#include "_locale.h"
#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

static char __lconv_dot[]  = ".";
static char __lconv_null[] = "";
static char __lconv_null2[2] = {0x00, 0x00};

static struct lconv	lformat = {
	__lconv_dot,	/* decimal_point */
	__lconv_null2,	/* thousands grouping */
	__lconv_null,	/* grouping */
	__lconv_null,	/* int_curr_symbol */
	__lconv_null,	/* currency symbol */
	__lconv_null,	/* mon_decimal_point */
	__lconv_null,	/* mon_thousands_sep */
	__lconv_null,	/* mon_grouping */
	__lconv_null,	/* positive sign */
	__lconv_null,	/* negative sign */
	CHAR_MAX,	/* int_frac_digits */
	CHAR_MAX,	/* frac_digits */
	CHAR_MAX,	/* p_cs_precedes */
	CHAR_MAX,	/* p_sep_by_space */
	CHAR_MAX,	/* n_scs_precedes */
	CHAR_MAX,	/* n_sep_by_space */
	CHAR_MAX,	/* p_sign_posn */
	CHAR_MAX,	/* n_sign_posn */
};
/*
 * montbl actually write out a struct that contains offsets into the
 * second part of the file. They wimped out by simply using the same
 * lconv struct and casting the types to be ints. This of course
 * doesn't work. We define the real struct in terms of 32 bit quantities
 * here. To be totally portable, we should export this struct and teach
 * montbl to use it.
 */
struct 	lconvoff 	{
	__uint32_t decimal_point;
	__uint32_t thousands_sep;
	__uint32_t grouping;
	__uint32_t int_curr_symbol;
	__uint32_t currency_symbol;
	__uint32_t mon_decimal_point;
	__uint32_t mon_thousands_sep;
	__uint32_t mon_grouping;
	__uint32_t positive_sign;
	__uint32_t negative_sign;
	char int_frac_digits;
	char frac_digits;
	char p_cs_precedes;
	char p_sep_by_space;
	char n_cs_precedes;
	char n_sep_by_space;
	char p_sign_posn;
	char n_sign_posn;
};

static char	sv_lc_numeric[LC_NAMELEN] = "C";
static char	sv_lc_monetary[LC_NAMELEN] = "C";

/* move out of function scope so we get a global symbol for use with data cording */
static char	*ostr = NULL; /* pointer to last set locale for LC_MONETARY */
static char	*onstr = NULL; /* pointer to last set locale for LC_NUMERIC */

struct lconv *
localeconv()
{
	int	fd;
	VOID	*str;
	struct stat	buf;
	struct lconvoff	*monetary;
	LOCKDECLINIT(l, LOCKLOCALE);

	if (strcmp(_cur_locale[LC_NUMERIC], sv_lc_numeric) != 0) {
		lformat.decimal_point[0] = _numeric[0];
		lformat.decimal_point[1] = '\0';
		lformat.thousands_sep[0] = _numeric[1];
		lformat.thousands_sep[1] = '\0';
		if ((fd = open(_fullocale(_cur_locale[LC_NUMERIC],"LC_NUMERIC"), O_RDONLY)) == -1)
			goto err4;
		if ((fstat(fd, &buf)) != 0 || (str = malloc((size_t)buf.st_size)) == NULL)
			goto err5;
		if (buf.st_size > 2) {
			if ((read(fd, str, (size_t)buf.st_size)) != buf.st_size)
				goto err6;

			/* if a previous locale was set for LC_NUMERIC, free it */
			if (onstr != NULL)
				free(onstr);
			onstr = str;

			lformat.grouping = (char *)str + 2;
		} else
			lformat.grouping = "";
		close(fd);
		(void) strcpy(sv_lc_numeric, _cur_locale[LC_NUMERIC]);
	}

	if (strcmp(_cur_locale[LC_MONETARY], sv_lc_monetary) == 0) {
		UNLOCKLOCALE(l);
		return(&lformat);
	}

	if ((fd = open(_fullocale(_cur_locale[LC_MONETARY],"LC_MONETARY"), O_RDONLY)) == -1)
		goto err1;
	if ((fstat(fd, &buf)) != 0 || (str = malloc((size_t)buf.st_size + 2)) == NULL)
		goto err2;
	if ((read(fd, str,(size_t)buf.st_size)) != buf.st_size)
		goto err3;
	close(fd);

	/* if a previous locale was set for LC_MONETARY, free it */
	if (ostr != NULL)
		free(ostr);
	ostr = str;

	monetary = (struct lconvoff *)str;
	str = (char *)str + sizeof(struct lconvoff);
	lformat.int_curr_symbol = (char *)str + monetary->int_curr_symbol;
	lformat.currency_symbol = (char *)str + monetary->currency_symbol;
	lformat.mon_decimal_point = (char *)str + monetary->mon_decimal_point;
	lformat.mon_thousands_sep = (char *)str + monetary->mon_thousands_sep;
	lformat.mon_grouping = (char *)str + monetary->mon_grouping;
	lformat.positive_sign = (char *)str + monetary->positive_sign;
	lformat.negative_sign = (char *)str + monetary->negative_sign;
	lformat.int_frac_digits = monetary->int_frac_digits;
	lformat.frac_digits = monetary->frac_digits;
	lformat.p_cs_precedes = monetary->p_cs_precedes;
	lformat.p_sep_by_space = monetary->p_sep_by_space;
	lformat.n_cs_precedes = monetary->n_cs_precedes;
	lformat.n_sep_by_space = monetary->n_sep_by_space;
	lformat.p_sign_posn = monetary->p_sign_posn;
	lformat.n_sign_posn = monetary->n_sign_posn;

	(void) strcpy(sv_lc_monetary, _cur_locale[LC_MONETARY]);
	UNLOCKLOCALE(l);
	return(&lformat);

err3:	free(str);
err2:	close(fd);
err1:	(void) strcpy(_cur_locale[LC_MONETARY], sv_lc_monetary);
	UNLOCKLOCALE(l);
	return(&lformat);

err6:	free(str);
err5:	close(fd);
err4:	(void) strcpy(_cur_locale[LC_NUMERIC], sv_lc_numeric);
	UNLOCKLOCALE(l);
	return(&lformat);
}
