#ifndef __SSDI_H__
#define __SSDI_H__
#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.3 $"

/*
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */


/* 	Maximum # of characters in database name or src name */
#define _SSDI_MAXDBNAME			32
#define _SSDI_MAXSRCNAME		32

/* 	Max. # of srcs for a database, including compiled-in ones */ 
#define _SSDI_MAXSRCS			 8

/* 	Max. # of different databases, like passwd, hosts, etc.*/ 
#define _SSDI_MAXDBS			 16

/*
 * 	Symbol that should be exported by dynamic sources; the
 *	variable corresponding to the symbol holds the ssdi vector.
 * 	Format is: <srcname>_<dbname>_ssdi_funcs.
 *	Length of vector is determined by the database.
 */

#define _SSDI_MAXSSDISYM		(_SSDI_MAXDBNAME + sizeof("_") + _SSDI_MAXSRCNAME + sizeof("_ssdi_funcs"))

/*  	Path name of config file */
#define _SSDI_CONFIG_FILE	"/etc/ssdi.conf"

/*  	Where to look for shared-lib objs for a given source. */
#define _SSDI_STD_SRC_DIR 	"/usr/lib"


/*
 * Internal to database interface routines --> used to keep track of
 * information about sources.
 */
typedef void *(*_SSDI_VOIDFUNC)();

struct ssdisrcinfo {
	char		*ssdi_si_name; 		/* name of source */
	_SSDI_VOIDFUNC	*ssdi_si_funcs; 	/* ssdi function vector */
};

/* Test for empty srcinfo descriptor. Sname will be null.  */
#define _SSDI_ISEMPTYSRC(src)	\
	(((src)->ssdi_si_name == NULL) || ((src)->ssdi_si_name[0] == '\0'))

/* Invoke function in src by index, return 1;  if no func, return NULL */
#define _SSDI_INVOKE_VOID(src,idx,cast)	\
     (					\
	((src)->ssdi_si_funcs[(idx)] == 0) ?	\
	(void)NULL : (*(cast)((src)->ssdi_si_funcs[(idx)]))() 	\
     )

/* Invoke function in src by index  if no func, return NULL */
#define _SSDI_INVOKE(src,idx,cast) 		\
     (					\
	((src)->ssdi_si_funcs[(idx)] == 0) ?	\
	(NULL) : (*(cast)((src)->ssdi_si_funcs[(idx)]))() 	\
     )

/* Same as above, but with one argument to target function */
#define _SSDI_INVOKE1(src,idx,cast,arg) 		\
     (					\
	((src)->ssdi_si_funcs[(idx)] == 0) ?	\
	(NULL) : (*(cast)((src)->ssdi_si_funcs[(idx)]))((arg)) 	\
     )

/* Same as above, but with two argument to target function */
#define _SSDI_INVOKE2(src,idx,cast,arg1,arg2) 		\
     (					\
	((src)->ssdi_si_funcs[(idx)] == 0) ?	\
	(NULL) : (*(cast)((src)->ssdi_si_funcs[(idx)]))((arg1),(arg2)) 	\
     )

/* Same as above, but with three argument to target function */
#define _SSDI_INVOKE3(src,idx,cast,arg1,arg2,arg3) 		\
     (					\
	((src)->ssdi_si_funcs[(idx)] == 0) ?	\
	(NULL) : (*(cast)((src)->ssdi_si_funcs[(idx)]))((arg1),(arg2),(arg3))	\
     )

struct ssdiconfiginfo {
	int		ssdi_ci_got_config;
	char		*ssdi_ci_dsrcs[_SSDI_MAXSRCS];
	char		**ssdi_ci_currdsrc;
};

#define _SSDI_InitConfigInfo  { 0, { NULL }, NULL }


/*
 * For a given database, get config info if needed, and load the first
 * dynamic source that is not already loaded. If a source was loaded,
 * information about the source is available in 'sip'. 'ci' keeps track
 * of cached config entry and the next source to load.
 * 	Returns 0 if a source was loaded.
 * 		1 if config entry was not found or no source could be loaded.
 */
int ssdi_get_config_and_load(char *, 		/* database name 	     */
			struct ssdiconfiginfo *,/* config info 		     */
			struct ssdisrcinfo *); 	/* info. about loaded source */

#ifdef __cplusplus
}
#endif
#endif /* !__SSDI_H__ */
