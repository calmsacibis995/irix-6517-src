/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1995 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include "error.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/param.h>

extern int errno;
extern int debug;

/* error table */
static ioc_err_tab_entry_t	ioc_err_tab[] = 
{
	{ FILE_CREATE_ERR	, _PANIC_	, "%s : Cannot create the file : %s\n"		},
	{ FILE_OPEN_ERR		, _PERROR_RETURN_, "%s : Cannot open the file : %s\n"		},
	{ FILE_WRITE_ERR	, _PANIC_	, "%s :	Cannot write to  the file : %s\n"	},
	{ INVALID_ARG_ERR	, _PANIC_	, "%s : Invalid number of arguments : %d\n%s\n"	},
	{ ATTR_GET_ERR		, _PANIC_	, "%s : Attr get failed for %s on %s\n"		},
	{ ATTR_SET_ERR		, _PERROR_RETURN_, "%s : Attr set failed for %s on %s\n"		},
	{ CTLR_NUM_MISMATCH_ERR	, _RETURN_	, "%s : Persistent ctlr num (%d) different from hwgraph ctlr num(%d) for %s\nMaybe the system was reconfigured.\n"		},
	{ CTLR_NUM_ABSENT_ERR	, _RETURN_	, "%s : Persistent ctlr num absent for hwgraph ctlr num(%d) for %s\n"		},
	{ FORK_FAIL_ERR		, _PANIC_	, "%s : Unable to fork a new process\n"		},
	{ FTW_ERR		, _PANIC_	, "ioconfig : File tree walk error\n"		},
	{ IOCTL_ERR		, _PERROR_RETURN_, "%s : Ioctl error : %s\n"			},
	{ UNAME_ERR		, _PANIC_	, "%s : Uname error \n"				},
	{ SCHEME_ERR		, _PANIC_	, "%s : Sysgi SGI_CONTROL_NUM_SCHEME error \n"	},
	{ GETPWNAM_ERR		, _RETURN_	, "%s : Getpwnam error: owner = %s\n"	}, 
	{ CHOWN_ERR		, _RETURN_	, "%s : Chown error: device = %s owner = %s group = %s \n"	},
	{ CHMOD_ERR		, _RETURN_	, "%s : Chmod error: device = %s mode = %d\n"	}, 
	{ OPENDIR_ERR		, _RETURN_	, "%s : Opendir error: directory = %s\n"	}, 
	{ READLINK_ERR		, _RETURN_	, "%s : Readlink error: symbolic link = %s\n"	}, 
	{ ERR_TAB_END										}

};

/* prints out the error message and takes the appropriate 
 * action encoded in the error table
 */
void
ioc_error(ioc_error_t	error,...)
{
	va_list 				ap;
	register ioc_err_tab_entry_t		*err_p;
	char					msg[200 + MAXPATHLEN];
	va_start(ap, error);
     
	if ((errno == EBUSY) && !debug)
	  return;

	for (err_p = ioc_err_tab ; err_p->err_tab_error != ERR_TAB_END ; err_p++) {
		if (err_p->err_tab_error == error) {
			vsprintf(msg,err_p->err_tab_format,ap);
			syslog(LOG_INFO,"IOCONFIG: %s : %m",msg);
			va_end(ap);
			va_start(ap,error);
			switch(err_p->err_tab_action) {
			case _PANIC_:
				fprintf(stderr,"ioconfig: ERROR:");
				vfprintf(stderr,err_p->err_tab_format,ap);
				va_end(ap);
				exit(1);
			case _PERROR_EXIT_:
				fprintf(stderr,"ioconfig: ERROR:");
				perror(va_arg(ap,char *));
				va_end(ap);
				exit(2);
			case _PERROR_RETURN_:
				fprintf(stderr,"ioconfig: ERROR:");
				vfprintf(stderr,err_p->err_tab_format,ap);
				perror("	error is");
				va_end(ap);
				return;
			case _RETURN_:
				fprintf(stderr,"ioconfig: WARNING:");
				vfprintf(stderr,err_p->err_tab_format,ap);
				va_end(ap);
				return;
			default:
				fprintf(stderr,"ioconfig : error action not yet implemented %d\n",
					err_p->err_tab_action);
				va_end(ap);
				break;
			}
		}
	}
}

