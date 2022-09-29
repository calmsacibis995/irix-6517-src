/*
*
* Copyright 1997, Silicon Graphics, Inc.
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

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/catopen.c	1.5.1.2"

/*
 * IMPORTANT:
 * This section is needed since this file also resides in the compilers'
 * libcsup/msg (v7.2 and higher). Once the compilers drop support for
 * pre-IRIX 6.5 releases this can be removed. Please build a libu before
 * checking in any changes to this file.
 *
 */

#ifndef _LIBU
#include "synonyms.h"
#else
#define _cat_init __cat_init
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#define __NLS_INTERNALS 1
#include <nl_types.h>
#undef __NLS_INTERNALS
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "_locale.h"


#if defined(__STDC__) && !defined(_LIBU)
	#pragma weak catopen = _catopen
#if _SGIAPI
	#pragma weak _cat_name = __cat_name
	#pragma weak __catopen_error_code = ___catopen_error_code
	#pragma weak __cat_path_name = ___cat_path_name
#endif
#endif


#if _SGIAPI
typedef nl_catd_t *sginl_catd;
#ifdef _LIBU
#define __catopen_error ___catopen_error
#endif
static int __catopen_error;
#else
static char *_cat_name(char *, char *, int, int);
#endif

#define set_path(x)     { if( ptr2 >= p_path_max ) {     \
	  	            if (nlspath == def_nlspath)  \
		              return 0;                  \
                            nlspath = def_nlspath;       \
			    goto retry_with_def_nlspath; \
                          } else                         \
                            *ptr2++ = (x);               \
                        }


/* _cat_name
 *
 * used internally to find the full pathname to a catalog file.
 * used externally by the explain command to find explanation
 * catalog files.
 */
char *
_cat_name(char *name, char *path, int mode, int path_len)
{
  const char *nlspath;
  const char *lang, *ptr, *ptr3;
  char *ptr2, c, d;
  struct stat *buf;
  const char *def_nlspath;
  struct stat buff;
  char *p_path_max = path + path_len - 1;
    
  /*
   * A name that contains a slash is a pathname
   */
  if (strchr(name, '/') != 0)
    return name;
    

  lang = setlocale ( LC_MESSAGES, NULL );
  if ( !strcmp ( lang, "C" ) )
      def_nlspath = _C_LOCALE_DEF_NLSPATH;
  else
      def_nlspath = DEF_NLSPATH;

  /*
   * Get env variables
   */
  if ((nlspath = getenv(NL_PATH)) == 0){
    /*
     * No env var : use default
     */
    nlspath = def_nlspath;
  }
  
  switch (mode) {
    case 0:
	lang = getenv(NL_LANG);
	break;

    case NL_CAT_LOCALE:
	/* lang is already set to the value of setlocale (LC_MESSAGES, NULL ) */
	break;

    default: 
	lang = NL_DEF_LANG;
	break;
  }

#if 0 /* MK removed to be the XPG4 based */
  if ((lang = getenv(NL_LANG)) == 0 && (lang = setlocale(LC_MESSAGES,(char*)NULL)) == 0)
    lang = NL_DEF_LANG;
#endif 
  if (lang  == 0 ) 
	lang = NL_DEF_LANG; 	/* can't let this be null */

    
  /*
   * Scan nlspath and replace if needed
   */
retry_with_def_nlspath:
  ptr = nlspath;
  ptr2 = path;
  *ptr2 = '\0';
  for (;;){
    switch (c = *ptr++){
    case ':':
      if (ptr2 == path){
        /*
         * Leading ':' Check current directory
         */
        if (access(name, 4) == 0)
          return name;
        continue;
      }
    /*FALLTHROUGH*/
    case '\0':
	/*
	 * Found a complete path
	 */
      	*ptr2 = '\0';
        /*
	 * Test to see that path is not a pure directory,
	 * if it is then attempt toopen it and append the
	 * filename "name" to it, in an attempt to succeed
	 * This syntax is not explicitly defined in XPG2, but
	 * is a logical extension to it. (XVS tests it too).
	 */
	
	buf = &buff;
	stat(path, buf);
	if ((buf -> st_mode) & S_IFDIR) {
		(void) strcat(path, "/");
		if (access(strcat(path, name), 4) == 0)
			return path;
		} /* otherwise branch out to end */
	else
		if ( access(path, 4) == 0)
			return path;
	
	if (c == '\0'){
		/*
	 	 * File not found
	 	 */
	  	if (nlspath == def_nlspath)
		    return 0;
		nlspath = def_nlspath;
		ptr = nlspath;
        }
        ptr2 = path;
        *ptr2 = '\0';
        continue;
    case '%':
      /*
       * Expecting var expansion
       */
      switch(c = *ptr++){
      case 'L':
        ptr3 = lang;
        while ((d = *ptr3++) != 0)
	  set_path(d);
        continue;
      case 'l':
        ptr3 = lang;
        while ((d = *ptr3++) != 0 && d != '.' && d != '_')
	  set_path(d);
        continue;
      case 't':
        ptr3 = strchr(lang, '_');
        if (ptr3++ != 0){
          while ((d = *ptr3++) != 0 && d != '.')
	    set_path(d);
        }
        continue;
      case 'c':
        ptr3 = strchr(lang, '.');
        if (ptr3++ != 0){
          while ((d = *ptr3++) != 0)
	    set_path(d);
        }
        continue;
      case 'N':
        ptr3 = name;
        while ((d = *ptr3++) != 0)
	  set_path(d);
        continue;
      case '%':
	set_path('%');
        continue;
      default:
	set_path('%');
	set_path(c);
        continue;
      }
    default:
      set_path(c);
      continue;
    }
  }
}

/*ARGSUSED*/

nl_catd
catopen(const char *name, int mode)
{
  nl_catd catd;
  char path[NL_MAXPATHLEN];

#if _SGIAPI
  __catopen_error = 0;
#endif

  /*
   * Allocate space to hold the necessary data
   */
  if ((catd = (nl_catd)malloc(sizeof (nl_catd_t))) == NULL) {
    __catopen_error = NL_ERR_MALLOC;
    return (nl_catd)-1L;
  }  

  /*
   * Get actual file name and open file
   */
  if ((name = _cat_name((char *)name,path, mode, NL_MAXPATHLEN)) != NULL){
  
    /*
     * Save full path for __cat_path_name
     */
#if _SGIAPI
    if ((((sginl_catd)catd)->path_name =
	 (char *)malloc(strlen(name)+sizeof(char))) != NULL) {
      (void) strcpy(((sginl_catd)catd)->path_name, name);
#endif
      /*
       * init internal data structures
       */
      if ((__catopen_error = _cat_init(name, catd)) == 0)
	return catd;
      else {
	free(catd);
#if _SGIAPI
	free(((sginl_catd)catd)->path_name);
#endif
	return (nl_catd)-1L;
      }
#if _SGIAPI
    } else {
      __catopen_error = NL_ERR_MALLOC;
      free(catd);
      return (nl_catd)-1L;
    }    
#endif
  } else {
    __catopen_error = errno;
  }

  free(catd);
  return (nl_catd)-1L;
}

#if _SGIAPI
/*
 *      __catopen_error_code - returns the error status from the last
 *              failed (returned -1) catopen() call.
 *
 *              returns  < 0    Internal error code
 *                       > 0    System error code
 */
int
__catopen_error_code(void)
{
  return(__catopen_error);
}


/*
 *      __cat_path_name - return actual name of opened catalog
 *
 *              catd   - message catalog descriptor
 *
 *              returns  NULL if catalog not open, or
 *                       pointer to actual name of catalog
 */
char *
__cat_path_name(nl_catd catd)
{
  
  return(((sginl_catd)catd)->path_name);
}
#endif
