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

#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/locale/RCS/catgetmsg.c,v 1.6 1997/05/15 14:21:40 bforney Exp $"

#if defined(__STDC__) && !defined(_LIBU)
        #pragma weak catgetmsg = _catgetmsg
        #pragma weak __catgetmsg_error_code = ___catgetmsg_error_code
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
#endif

#define __NLS_INTERNALS 1
#include <nl_types.h>
#undef __NLS_INTERNALS
#include <string.h>

/*
 * catgetmsg -- retrieves a message to a user supplied buffer from a catalog
 *
 */
char *
catgetmsg(
	  nl_catd catd,
	  int set_num,
	  int msg_num,
	  char *buf,
	  int buflen
	  )
{
  size_t len;
  char *str;
  
  if ((str = catgets(catd, set_num, msg_num, NULL)) != NULL) {
    
    /* find the proper string length to copy */
    len = strlen(str); 
    if (len >= buflen)
      len = buflen - 1;
    
    (void) strncpy(buf, str, len);
    buf[len] = '\0';
    
    return buf;
  } else {
    return "\0";
  }
}


/*
 *      __catgetmsg_error_code - returns the error status from the last
 *              failed catgetmsg() call.
 *
 *              returns  < 0    Internal error code
 *                       > 0    System error code
 */
int
__catgetmsg_error_code(void)
{
        return(__catgets_error_code());
}



