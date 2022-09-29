/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/catgets.c	1.6.1.5"

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
#endif

#include <dirent.h>
#include <locale.h>
#include <stdio.h>
#define __NLS_INTERNALS 1
#include <nl_types.h>
#undef __NLS_INTERNALS
#include <string.h>
#include <pfmt.h>

#if defined(__STDC__) && !defined(_LIBU)
	#pragma weak catgets = _catgets
#if _SGIAPI
	#pragma weak __catgets_error_code = ___catgets_error_code
#endif
#endif

#define min(a,b)  (a>b?b:a)

typedef nl_catd_t *sginl_catd;
#if _SGIAPI
#ifdef _LIBU
#define __catgets_error ___catgets_error
#endif
static int __catgets_error = 0;
#endif

#ifdef __STDC__
char *
catgets(nl_catd catd, int set_num, int msg_num, const char *s)
#else
char *
catgets(catd, set_num, msg_num, s)
  nl_catd catd;
  int set_num, msg_num;
  char *s;
#endif
{
  register int i;
  int first_msg;
  int message_no;
  int msg_ptr;

  char *data;
  char *msg;
  char *p;

  struct _cat_set_hdr *sets;
  struct _cat_msg_hdr *msgs;
  struct _m_cat_set *set;
  sginl_catd sgicatd;

#if _SGIAPI
  __catgets_error = 0;
#endif
  
  if ( catd == (nl_catd)-1L || catd == (nl_catd)NULL) {
    __catgets_error = NL_ERR_ARGERR;
    return (char *)s;
  }

  sgicatd = (sginl_catd)catd;

  switch (sgicatd->type) {
    case MALLOC :
      /*
       * Locate set
       */
      sets = sgicatd->info.m.sets;
      msgs = sgicatd->info.m.msgs;
      data = sgicatd->info.m.data;
      for (i = min(set_num, sgicatd->set_nr) - 1 ; i >= 0 ; i--){
        if (set_num == sets[i].shdr_set_nr){
          first_msg = sets[i].shdr_msg;
  
          /*
           * Locate message in set
           */
          for (i = min(msg_num, sets[i].shdr_msg_nr) - 1 ; i >= 0 ; i--){
            if (msg_num == msgs[first_msg + i].msg_nr){
              i += first_msg;
              msg = data + msgs[i].msg_ptr;
              return msg;
            }
            if (msg_num > msgs[first_msg + i].msg_nr) {
	      __catgets_error = NL_ERR_NOMSG;
              return (char *)s;
	    }
          }
          return (char *)s;
        }
        if (set_num > sets[i].shdr_set_nr) {
	  __catgets_error = NL_ERR_BADSET;
          return (char *)s;
	}
      }
      return (char *)s;
  
  case MKMSGS :
      /*
       * get it from
       * a mkmsgs catalog
       */
      if (set_num > sgicatd->set_nr) {
	__catgets_error = NL_ERR_BADSET;
        return (char *)s;
      }
      set = &(sgicatd->info.g.sets->sn[set_num -1]);
      if (msg_num > set->last_msg) {
	__catgets_error = NL_ERR_NOMSG;
        return (char *)s;
      }
      message_no = set->first_msg + msg_num -1;

      /*  Extract the message from the second file  */
      msg_ptr = *((int *)(sgicatd->info.g.msgs + (message_no * sizeof(int))));
      p = (char *)(sgicatd->info.g.msgs + msg_ptr);

      if (strcmp(p,DFLT_MSG) && strcmp(p,"Message not found!!\n")) {
        return p;
      } else {
	__catgets_error = NL_ERR_NOMSG;
	return (char *)s;
      }
  
  default :
      __catgets_error = NL_ERR_BADTYPE;
      return (char *)s;

  }
}


#if _SGIAPI
/*
 *      __catgets_error_code - returns the error status from the last
 *              failed catgets() call.
 *
 *              returns  < 0    Internal error code
 *                       > 0    System error code
 */
int
__catgets_error_code(void)
{
        return(__catgets_error);
}
#endif
