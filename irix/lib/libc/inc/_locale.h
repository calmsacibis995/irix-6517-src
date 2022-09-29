/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.9 $"

#include <locale.h>
#include <nl_types.h>
#include <locale_attr.h>

#define LC_NAMELEN	64		/* maximum part name length (inc. \0) */
#define SZ_CTYPE	(257 + 257)	/* is* and to{upp,low}er tables */
#define SZ_CODESET	7		/* bytes for codeset information */
#define SZ_NUMERIC	2		/* bytes for numeric editing */
#define SZ_TOTAL	(SZ_CTYPE + SZ_CODESET)
#define NM_UNITS	0		/* index of decimal point character */
#define NM_THOUS	1		/* index of thousand's sep. character */

/* _cur_locale[][] defined in _loc_data.c - array size [LC_ALL][LC_NAMELEN] */
extern char _cur_locale[][LC_NAMELEN];

/* _ctype[] defined in _ctype.c - array size [SZ_TOTAL] */
/* This symbol is preserved for backward binary compatibility */
/* All this information is now stored in __libc_attr and referenced there */
extern unsigned char _ctype[];

/* _numeric[] defined in _loc_data.c - array size [SZ_NUMERIC] */
extern unsigned char _numeric[];

/* _ctype_dflt defined in _ctype.c - default ctype information */
extern const __ctype_t _ctype_dflt;

/* catopen.c */
extern int _mmp_opened;
extern int _cat_init(const char *, nl_catd);

/* Last alias name and alias value */
extern char	_alias_name [ LC_NAMELEN ];
extern char *	_alias_val ;

char *_nativeloc(int, int * search_alias);	/* trunc. name for category's "" locale */
char *_fullocale(const char *, const char *);	/* complete path */
int _set_tab_numeric ( const char * );		/* fill _numeric[] */
int _set_tab_ctype ( const char * );		/* fill the ctype portion of __libc_attr */
void _set_res_collate ( const char * );		/* fill the collate portion of __libc_attr */

/* Default functions for euc isset[23] functions */
int	__isset2 ( int c );
int	__isset3 ( int c );
int	__iscodeset ( int codeset, wint_t wc );
