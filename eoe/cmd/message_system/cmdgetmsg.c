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

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/message_system/RCS/cmdgetmsg.c,v 1.3 1997/05/09 21:24:35 bforney Exp $"

#include <nl_types.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <malloc.h>

#define MAXMLN		1024	/* Maximum message length		  */
char *_docmdgetmsg();
nl_catd _open_catalog(const char *);

/*
 *	_cmdgetmsg() -	Get a message from the Unicos utility command 
 *			and/or system message catalogs
 *
 *	"cmdgetmsg()" returns up to "buflen" characters of the requested
 *	message string identified by the utility message number 'msgno'
 *	and/or system error number 'erno'. The message string is placed
 *	in the user-supplied buffer 'buf'. If the message is longer than
 *	'buflen' characters, it is truncated with a null-byte.
 *
 *	If an invalid buffer length (bufle <1), a pointer to a null string
 *	("") is returned, otherwise, _cmdgetmsg() returns a pointer to the 
 *	message string in 'buf'.
 *
 *	Formal parameters:						
 *		msgno - message catalog message number			
 *		erno - system error number				
 *		buf - user supplied buffer 
 *		buflen - length of user supplied buffer		
 */

char *
_cmdgetmsg(int msgno,
	   int erno,
	   char *buf,
	   int buflen,
	   const char *cmdname,
	   const char *cmdcat,
	   const char *syscat,
	   ...)
{
	va_list	args;
	char	*bufp;

	va_start(args, syscat);
	bufp = _docmdgetmsg(msgno, erno, buf, buflen, cmdname, 
			    cmdcat, syscat, args);
	va_end(args);

	return(bufp);
}

/*
 *	_docmdgetmsg() - Get a message from the IRIX utility command 
 *			 and/or system message catalogs
 *
 *	"docmdgetmsg()" returns up to "buflen" characters of the requested
 *	message string identified by the utility message number 'msgno'
 *	and/or system error number 'erno'. The message string is placed
 *	in the user-supplied buffer 'buf'. If the message is longer than
 *	'buflen' characters, it is truncated with a null-byte.
 *
 *	If an invalid buffer length (bufle <1), a pointer to a null string
 *	("") is returned, otherwise, _docmdgetmsg() returns a pointer to the 
 *	message string in 'buf'.
 *
 *	Formal parameters:						
 *		msgno - message catalog message number			
 *		erno - system error number				
 *		buf - user supplied buffer 
 *		buflen - length of user supplied buffer		
 *		args - argument list		
 */

char *
_docmdgetmsg(int msgno,
	     int erno,
	     char *buf,
	     int buflen,
	     const char *cmdname,
	     const char *cmdcat,
	     const char *syscat,
	     va_list args)
{
	nl_catd	cfd;			/* Message catalog descriptor	*/
	char	msgbuf[MAXMLN *2 +1];	/* Message buffer		*/
	char	*msgptr;		/* Pointer to message		*/
	char	tmpbuf[MAXMLN+1];	/* Temporary message buffer	*/
	char	*tptr;			/* Pointer to tmp message buffer*/
	char	*nptr;			/* Null message pointer		*/
	static	char n;			/* Null message string		*/
	int	msglen;			/* Message length		*/

	/* check valid buflen */
	n	='\0';
	nptr	=&n;
	if (buflen < 1) return(nptr);

	msgptr = &msgbuf[0];
	*msgptr = '\0';

	if (msgno) {
		/* open command utility message catalog */
		if ((cfd = _open_catalog(cmdcat)) == (nl_catd)-1) {
			sprintf(msgptr, "Cannot open the message catalog for %s.", cmdname);
		}
		else {
			/* put command message into msgbuf */
			if ((tptr = catgetmsg(cfd, NL_MSGSET, msgno,
				tmpbuf, MAXMLN)) == NULL) {
				(void) sprintf(msgptr, "Cannot retrieve message for message number '%d' from the message catalog for %s.",
					msgno, cmdname);
			}
			else {
				(void) vsprintf(msgptr, tmpbuf, args);
			}
			(void) catclose (cfd);
		} 
	
		if (erno) {
			/* back over ending period */
			msgptr += strlen(msgptr) -1;
			/* modify message to perror() format */
			strcpy(msgptr, ": ");
			msgptr += strlen(msgptr);
		}
	}

	if (erno) {
	  tptr = strerror(erno);
	  if (tptr != NULL) {
	    strcpy(msgptr, tptr);
	  } else {
	    sprintf(msgptr, "Cannot retrieve message for errno '%d' from system message catalog.", erno);
	  }
	}
	else if (!msgno) {
		(void) sprintf(msgptr, 
		   "%s(INTERNAL): Message number and errno are both zero.\n", 
		   cmdname);
	}

	/* check message length */
	msglen = strlen(msgbuf);
	if (msglen > buflen) {
		msglen = buflen -1;
	}

	/* transfer message to user supplied buffer, terminate with Null */
	(void) strncpy(buf, msgbuf, msglen);
	buf[msglen] = '\0';

	return(buf);
}

/*
 *  _open_catalog - open explanation catalog
 *
 *     Tries to open the catalog with cmdcat. If this fails, the .cat
 *     is stripped off. This is done incase the NLSPATH is set to
 *     %N.cat instead of %N, as is commonly done by module files for
 *     Cray products.
 */
nl_catd
_open_catalog(const char *cmdcat)
{
  char *new_str, *temp;
  nl_catd cfd;
  
  if ((cfd = catopen(cmdcat, 0)) == (nl_catd)-1) {
    /* try stripping off ".cat" suffix and call catopen again */
    if (((temp = strstr(cmdcat, ".cat")) != NULL) &&
	((new_str = (char *)malloc(temp-cmdcat+sizeof(char)))
	 != NULL)) {
      strncpy(new_str, cmdcat, temp-cmdcat);
      new_str[temp-cmdcat] = '\0';
      cfd = catopen(new_str, 0);
    }
  }
  return(cfd);
}







