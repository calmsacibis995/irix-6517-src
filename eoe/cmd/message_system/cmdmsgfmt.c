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

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/message_system/RCS/cmdmsgfmt.c,v 1.3 1997/05/09 21:26:38 bforney Exp $"

#define __NLS_INTERNALS 1
#include <nl_types.h>
#undef __NLS_INTERNALS
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

/* --- Environment variable defaults --- */
#define	CMDMSG_FORMAT	"CMDMSG_FORMAT"
#define	D_CMDMSG_FORMAT	"%G-%N %C: %M\n"

#define EQUAL_CHAR "="
#define MESSAGE_CAT_EXT ".cat"

/*
 *	cmdmsgfmt - format an error message
 *
 *	char *
 *	cmdmsgfmt(
 *		char	*cmdname,
 *		char	*groupcode,
 *		int	msg_num,
 *		char	*severity,
 *		char	*msgtext,
 *		char	*buf,
 *		int	buflen)
 *
 *	cmdmsgfmt() formats up to "buflen" characters a message containing
 *	the command name "cmdname", group code "groupcode", message number
 *	"msg_num", severity level "severity", the text of the message
 *	"msgtext".  The formatted essage is placed in the user-supplied 
 *      buffer "buf".
 *
 *	The msg_num parameter is an integer which is converted to an ASCII
 *	string of digits.  The cmdname, groupcode, severity, and msgtext
 *	parameters are all null-terminated character strings.
 *
 *	The CMDMSG_FORMAT environment variable controls how the message
 *	is formatted.  The MSG_FORMAT environment variable also controls
 *      how the message is formatted if the CMDMSG_FORMAT is not defined. 
 *      If either variable is defined, a default formatting value is used.
 *
 */

char *
cmdmsgfmt(const char *cmdname, const char *groupcode, int msg_num,
	const char *severity, const char *msgtext, char *buf, int buflen)
{
  char *cmdmsg_format_env;
  char *msg_format_env,
    *save_msg_format,
    *temp = NULL;
  char *return_val,
    groupcode_stripped[NL_MAXPATHLEN];

  cmdmsg_format_env = getenv(CMDMSG_FORMAT);

  /* check if the MSG_FORMAT evironment variable needs to be changed.
   * two cases where a change is needed:
   * - CMDMSG_FORMAT environment variable is defined.
   * - neither CMDMSG_FORMAT nor MSG_FORMAT environment variables are defined.
   */


  if (cmdmsg_format_env != NULL ||
      ((temp = getenv(MSG_FORMAT)) == NULL
       /* && cmdmsg_format_env == NULL
	  (implicit in the evaluation of the if) */
       )) {
    /* save MSG_FORMAT environment variable */
    if (temp == NULL) {
      temp = getenv(MSG_FORMAT);
    }
    if (temp != NULL) {
      save_msg_format = (char *)malloc(strlen(temp));
      strcpy(save_msg_format, temp);
    } else {
      save_msg_format = NULL;
    }

    /* set MSG_FORMAT environment variable to new value */
    msg_format_env = (char *)malloc(strlen(MSG_FORMAT)+
				    strlen(cmdmsg_format_env ?
					   cmdmsg_format_env :
					   D_CMDMSG_FORMAT)+2);
    if (msg_format_env == NULL) {
      return NULL;
    }
    strcpy(msg_format_env, MSG_FORMAT);
    strcat(msg_format_env, EQUAL_CHAR);
    strcat(msg_format_env,
	   cmdmsg_format_env ?
	   cmdmsg_format_env : D_CMDMSG_FORMAT);
    if (putenv(msg_format_env) != 0) {
      free(msg_format_env);
      return NULL;
    }
    free(msg_format_env);
  } else {
    save_msg_format = NULL;
  }

  /* strip off MESSAGE_CAT_EXT from groupcode if it exists */
  if ((temp = strstr(groupcode, MESSAGE_CAT_EXT)) != NULL) {
    strncpy(groupcode_stripped, groupcode, temp-groupcode);
    groupcode_stripped[temp-groupcode] = '\0';
  } else {
    strcpy(groupcode_stripped, groupcode);
  }

  return_val =
    catmsgfmt(cmdname, groupcode_stripped, msg_num, severity, msgtext,
	      buf, buflen, NULL, NULL);

  /* restore MSG_FORMAT environment variable if needed */
  if (save_msg_format != NULL) {
    msg_format_env = (char *)malloc(strlen(MSG_FORMAT)+
				    strlen(save_msg_format)+2);
    if (msg_format_env == NULL) {
      return NULL;
    }
    strcpy(msg_format_env, MSG_FORMAT);
    strcat(msg_format_env, EQUAL_CHAR);
    strcat(msg_format_env, save_msg_format);
    putenv(save_msg_format);
  }
  
  return return_val;
}



