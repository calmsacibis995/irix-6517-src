/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/catclose.c	1.4.1.3"

#if defined(__STDC__) && !defined(_LIBU)
	#pragma weak catclose = _catclose
#endif

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
#define _mmp_opened __mmp_opened
#endif

#include <dirent.h>
#include <stdio.h>
#include <nl_types.h>
#include <locale.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/mman.h>	/* for munmap prototype */
#include "_locale.h"

typedef nl_catd_t *sginl_catd;

int
catclose(nl_catd catd)
{
  sginl_catd sgicatd;

  sgicatd = catd;

  if (sgicatd != (sginl_catd)-1L || sgicatd == (sginl_catd)NULL) {
#if _SGIAPI
    free(sgicatd->path_name);
#endif
    
    if (sgicatd->type == MALLOC) {
        free(sgicatd->info.m.sets);
	sgicatd->type = (char) -1;
	free(sgicatd);
	return 0;
    } else
    if (sgicatd->type == MKMSGS) {
        munmap(sgicatd->info.g.sets, (size_t)sgicatd->info.g.set_size);
        munmap(sgicatd->info.g.msgs, (size_t)sgicatd->info.g.msg_size);
	sgicatd->type = (char) -1;
	_mmp_opened--;
	free(sgicatd);
	return 0;
    }
  }
  /*
   * was a bad catd
   */
  return -1;
}

