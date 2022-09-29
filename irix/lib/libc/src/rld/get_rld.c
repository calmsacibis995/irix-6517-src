/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991 MIPS Computer Systems, Inc.            |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 52.227-7013.   |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Drive                                |
 * |         Sunnyvale, CA 94086                               |
 * |-----------------------------------------------------------|
 */
#ident "$Id: get_rld.c,v 1.13 1998/06/02 17:09:43 davea Exp $"

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * _ _ g e t _ r l d
 * returns the "rld" to use
 *
 *	Returns:
 *		name of rld object
 *	Arguments: 
 *		none
 */
#include "synonyms.h"
#include <stdio.h>
#include <sgidefs.h>
#include <elf.h>
#include "rld_elf.h"
#include "rld_defs.h"
#include <sys/types.h>
#include <sys/syssgi.h>
#include "elfmap.h"
#include <unistd.h>		/* prototype for WRITE (aka. write(2)) */
#include <string.h>		/* prototype for strlen(2) */
#include <stdlib.h>		/* prototype for exit(2) */

#if (_MIPS_SIM == _MIPS_SIM_ABI64)
#define DEFAULT_RLD     "/lib64/rld"
#define	RLD_PATH	"_RLD64_PATH"
#elif (_MIPS_SIM == _MIPS_SIM_ABI32)
#define	DEFAULT_RLD	"/lib/rld"
#define	RLD_PATH	"_RLD_PATH"
#else	/* _MIPS_SIM == MIPS_SIM_NABI32 */
#define	DEFAULT_RLD	"/lib32/rld"
#define	RLD_PATH	"_RLDN32_PATH"
#endif

#define	FAILMSG	 	"Could not map rld from file "
#define FAILMSGSZ	28
#define NONELFMSG	"File specified as rld is not an ELF file: "
#define	NONELFMSGSZ	42

ELF_ADDR
__get_rld( char **env  )
{
char	*_rld_getenv(char *, char **);
char	*rldp;

ELF_ADDR rld_addr;
ELF_EHDR *ehdr;

    /*
     * Get rld from the environment unless it's unsafe to do so.
     *
     * Note that we don't use the same fallback that rld uses if the syssgi()
     * fails because libc and the kernel are always shipped together while rld
     * needs to work on multiple OS releases that may not have the new rld
     * environment variable safety check that was introduced in IRIX 6.5.
     *
     * Also note that we are currently disallowing even root to set _RLD_PATH.
     * We may want to relax this at a later time by passing 0 to the
     * syssgi(SGI_RXEV_GET, ...) instead of the more restrictive 1.
     */
    if (((rldp = _rld_getenv( RLD_PATH,env )) == NULL ) || 
	_syssgi(SGI_RXEV_GET, 1)) {
	/* environment variable not set or is unsafe to use: use default */
	rldp = DEFAULT_RLD;
    }

    /* map in rld */
    if ( (rld_addr = __elfmap( rldp, 0,  _EM_SILENT)) == (ELF_ADDR)_EM_ERROR) {
	WRITE(2, FAILMSG, FAILMSGSZ);
	WRITE(2, rldp, STRLEN(rldp));
	WRITE(2, "\n", 1);
	_exit( 0xdeadbeef  );
    }
    /* get the entry to rld */
    ehdr = (ELF_EHDR *)rld_addr;
    if (IS_ELF(*ehdr))
        return ehdr->e_entry;
    else {
	WRITE(2, NONELFMSG, NONELFMSGSZ);
	WRITE(2, rldp, STRLEN(rldp));
	WRITE(2, "\n", 1);
	_exit( 0xdeadbeef  );
    }
    /* NOTREACHED */
}


/*LINTLIBRARY*/
/*
 *	_rld_getenv(name,env)
 *	returns ptr to value associated with name, if any, else NULL
 */
static char *nvmatch(char *, char *);

char *
_rld_getenv(register char *name, register char **environ)
{
	register char *v, **p;

	p=environ;

	if(p == NULL)
		return(NULL);
	while(*p != NULL)
		if((v = nvmatch(name, *p++)) != NULL)
			return(v);
	return(NULL);
}

/*
 *	s1 is either name, or name=value
 *	s2 is name=value
 *	if names match, return value of s2, else NULL
 *	used for environment searching: see _rld_getenv
 */

static char *
nvmatch(register char *s1, char *s2)
{
	while(*s1 == *s2++)
		if(*s1++ == '=')
			return(s2);
	if(*s1 == '\0' && *(s2-1) == '=')
		return(s2);
	return(NULL);
}
