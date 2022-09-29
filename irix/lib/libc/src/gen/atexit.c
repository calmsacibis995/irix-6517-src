/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/atexit.c	1.4"
/*LINTLIBRARY*/

#include "synonyms.h"
#include <stdlib.h>
#include "mplib.h"
#include "gen_extern.h"

#define MAXEXITFNS	37

static void	(*exitfns[MAXEXITFNS])(void);
static void	(*eachexitfns[MAXEXITFNS])(void);
static int	numeachexitfns _INITBSS;
static int	numexitfns _INITBSS;


/* Note: changes to this routine should be mirrored in "__ateachexit" below */
int
atexit(void (*func)())
{
	LOCKDECLINIT(l, LOCKMISC);

	if ((func == (void(*)(void))0) || numexitfns >= MAXEXITFNS) {
		UNLOCKMISC(l);
		return(-1);
	}

	exitfns[numexitfns++] = func;
	UNLOCKMISC(l);
	return(0);
}

void
_exithandle(void)
{
	/* Don't decrement & dirty numexitfns unless work to do */
	while (numexitfns > 0)
		(*exitfns[--numexitfns])();
}

/*
 * IRIX 4.0 compatibility
 */

void
__call_exitfns(void)
{
	_exithandle();
}





/* Similar functionality to "atexit," but called by *each* thread
** in a share group (not just the last one like atexit is).
*/


int
__ateachexit(void (*func)())
{
        LOCKDECLINIT(l, LOCKMISC);

        if ((func == (void(*)(void))0) || numeachexitfns >= MAXEXITFNS) {
                UNLOCKMISC(l);
                return(-1);
        }

        eachexitfns[numeachexitfns++] = func;
        UNLOCKMISC(l);
        return(0);
}

void
__eachexithandle(void)
{
	/* Since this can be called by several threads, we need to
	** make a local copy of numeachexitfns.
	*/
	int count = numeachexitfns;

        while (--count >= 0)
                (*eachexitfns[count])();
}

