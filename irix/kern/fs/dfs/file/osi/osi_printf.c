/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: osi_printf.c,v 65.3 1998/03/23 16:26:12 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE
 * for the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_printf.c,v $
 * Revision 65.3  1998/03/23 16:26:12  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 19:58:16  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:44  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.136.1  1996/10/02  18:11:46  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:07  damon]
 *
 * Revision 1.1.131.2  1994/06/09  14:17:14  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:29:25  annie]
 * 
 * Revision 1.1.131.1  1994/02/04  20:26:45  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:16:32  devsrc]
 * 
 * Revision 1.1.129.1  1993/12/07  17:31:24  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  16:13:16  jaffe]
 * 
 * Revision 1.1.5.6  1993/01/21  14:52:58  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:56:36  cjd]
 * 
 * Revision 1.1.5.5  1992/11/24  18:23:56  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:15:42  bolinger]
 * 
 * Revision 1.1.5.4  1992/10/02  21:25:02  toml
 * 	Don't call fprf() on HPUX.
 * 	[1992/10/02  19:35:38  toml]
 * 
 * Revision 1.1.5.3  1992/09/15  13:16:14  jaffe
 * 	Transarc delta: jaffe-ot3935-dont-require-DFSLog-esp-for-memcache 1.3
 * 	  Selected comments:
 * 	    we no longer require that a memcache dfs have a cachedir.  Any messages
 * 	    will be sent to the console.  If there is no cachedir, then the user will
 * 	    be unable to turn on cm debugging.  The user may specify a cachedir for
 * 	    memcache if they want, in which case a DFSLog file will be created in that
 * 	    dir.
 * 	    keep track of whether we have been given a logfile.   If there is none,
 * 	    then don't fail on StartLog, just return and let the messages go to the
 * 	    console.
 * 	    fixed bugs found in testing
 * 	[1992/09/14  20:20:31  jaffe]
 * 
 * Revision 1.1.5.2  1992/08/31  20:48:59  jaffe
 * 	Transarc delta: cburnett-ot4621-add-vfscache-support-for-aix 1.1
 * 	  Selected comments:
 * 	    Add AFS_VFSCACHE support for the AIX platform
 * 	    VFSCACHE AIX support
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    Remove HIGHC pragmas.
 * 	    add prototype for fprf.
 * 	    make osi_SetLogDev, osi_StartLogFile, and osi_EndLogFile return a void.
 * 	    make the first argument to osi_dp be a char *.
 * 	    return a value (0) from osi_dp.
 * 	    declare fprint to return an int, and return a value (0).
 * 	    Add a variable declaration for "b" in the function fprintn.
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    make osi_StartLogFile, and osi_EndLogFile return an int.
 * 	    cleanup for OSF1 compilation
 * 	[1992/08/30  03:37:57  jaffe]
 * 
 * Revision 1.1.3.7  1992/07/03  01:54:05  mason
 * 	Transarc delta: tu-ot3584-cm-debug-panic-pmax 1.1
 * 	  Selected comments:
 * 	    Turning on CM debug should not cause the system to panic.
 * 	    When the CM debug is on, direct all I/Os to DFSLog rahter than the system's
 * 	    message log.
 * 	[1992/07/02  19:31:16  mason]
 * 
 * Revision 1.1.3.6  1992/07/02  21:52:36  mason
 * 	Transarc delta: tu-ot3584-cm-debug-panic-pmax 1.1
 * 	  Selected comments:
 * 	    Turning on CM debug should not cause the system to panic.
 * 	    When the CM debug is on, direct all I/Os to DFSLog rahter than the system's
 * 	    message log.
 * 	[1992/07/02  19:31:16  mason]
 * 
 * Revision 1.1.3.5  1992/05/28  20:47:51  toml
 * 	Add #ifdef AFS_VFSCACHE
 * 	[1992/05/28  20:44:55  toml]
 * 
 * Revision 1.1.3.4  1992/04/15  20:03:32  garyf
 * 	fix compilation error
 * 	[1992/04/15  20:03:07  garyf]
 * 
 * Revision 1.1.3.3  1992/01/24  03:47:20  devsrc
 * 	Merging dfs6.3
 * 	[1992/01/24  00:17:28  devsrc]
 * 
 * Revision 1.1.3.2  1992/01/23  20:20:37  zeliff
 * 	Moving files onto the branch for dfs6.3 merge
 * 	[1992/01/23  18:37:08  zeliff]
 * 	Revision 1.1.1.3  1992/01/23  22:19:56  devsrc
 * 	Fixed logs
 * 
 * $EndLog$
 */
/* Copyright (C) 1990, 1989 Transarc Corporation - All rights reserved */

#include <dcedfs/osi.h>
#include <dcedfs/osi_uio.h>
#include <dcedfs/lock.h>

/*
 * osi_fprintn prints a number n in base b.
 * We don't use recursion to avoid deep kernel stacks.
 * The magic constant "12" is really 1 + howmany(bits in a long, 3),
 * i.e., the maximum number of octal digits in a long, plus a null at the end.
 * This magic number will be "23" on 64-bit machine.
 */
#define MAX_OCTAL_STRING 23
static char *
osi_fprintn(u_long n, int b, char *bufp)
{
    char prbuf[MAX_OCTAL_STRING];
    char *cp;

    if (b == 10 && (long)n < 0) {
	*bufp++ = '-';
	n = (u_long)(-(long)n);
    }
    cp = prbuf;
    do {
	*cp++ = "0123456789abcdef"[n % b];
	n /= b;
    } while (n);
    do
        *bufp++ = *--cp;
    while (cp > prbuf);
    return bufp;
}

/*
 * osi_fprf is ...
 */

int
osi_fprf(char *bufp, char *fmt, va_list ap)
{
    int c = 1;
    char *s, *bufp0 = bufp;

    while (c != '\0') {
	while ((c = *fmt++) != '%' && c != '\0')
	    *bufp++ = c;

	if (c == '\0')
	    break;

	while ((c = *fmt++) == 'l')
	    ;

	switch (c) {
	  case 'x': case 'X':
	    bufp = osi_fprintn(va_arg(ap, u_long), 16, bufp);
	    break;
	  case 'd': case 'D': case 'u':
	    bufp = osi_fprintn(va_arg(ap, u_long), 10, bufp);
	    break;
	  case 'o': case 'O':
	    bufp = osi_fprintn(va_arg(ap, u_long), 8, bufp);
	    break;
	  case 'c':
	    *bufp++ = va_arg(ap, int);
	    break;
	  case 's':
	    s = va_arg(ap, char *);
	    while (*s != 0)
		*bufp++ = *s++;
	    break;
	  case '%':
	    *bufp++ = c;
	    break;
	  default:
	    break;
	}	
    }

   *bufp = 0;
   return (bufp - bufp0);
}
