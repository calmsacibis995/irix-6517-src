/*****************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Function:
 *
 *	BSD gethostbyname call for Xenix
 *
 * Original Author:  Paul Mattes		Created on: 04/12/88
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/hostname.c,v 1.3 1996/10/04 12:08:16 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/hostname.c,v $
 *
 * Revision History:
 *
 * $Log: hostname.c,v $
 * Revision 1.3  1996/10/04 12:08:16  cwilson
 * latest rev
 *
 * Revision 1.10  1993/12/30  13:13:41  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 1.9  1993/06/28  08:39:20  carlson
 * Refixed xylo_rhost's fopen() call.
 *
 * Revision 1.8  93/06/24  21:50:43  reeve
 * Fixed up xylo_rhost's call to fopen().
 * 
 * Revision 1.7  1993/06/21  19:51:56  reeve
 * Added include for netdb.h.
 *
 * Revision 1.6  1991/06/21  09:48:17  barnes
 * changing _xxxx function names to xylo_xxxx
 *
 * Revision 1.5  91/04/08  14:26:14  emond
 * ifndef'ed include of socket.h since we can now work with TLI instead.
 * 
 * Revision 1.4  89/04/11  00:05:00  loverso
 * fix extern declarations; inet_addr is u_long; remove extern htonl et al
 * 
 * Revision 1.3  89/04/05  12:40:15  loverso
 * Changed copyright notice
 * 
 * Revision 1.2  88/05/24  18:35:36  parker
 * Changes for new install-annex script
 * 
 * Revision 1.1  88/04/15  11:56:47  mattes
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:08:16 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/hostname.c,v 1.3 1996/10/04 12:08:16 cwilson Exp $"

#ifndef lint
static char rcsid[] = RCSID;
#endif

/*****************************************************************************
 *									     *
 * Include files							     *
 *									     *
 *****************************************************************************/

#include "../inc/config.h"

#include "port/port.h"
#include "port/install_dir.h"
#include <sys/types.h>
#include "netdb.h"
#ifdef SLIP
#undef open
#undef close
#undef read
#endif

#ifndef TLI
#include <sys/socket.h>
#endif
#include <netinet/in.h>

#include <ctype.h>
#include <stdio.h>


/*****************************************************************************
 *									     *
 * Local defines and macros						     *
 *									     *
 *****************************************************************************/

#define LB_SIZE 256


/*****************************************************************************
 *									     *
 * Structure and union definitions					     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * External data							     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * Global data								     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * Static data								     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * Forward definitions							     *
 *									     *
 *****************************************************************************/



/*
 * This gethostbyname() call is based on the EXOS rhost() call.  For SL/IP,
 * first define the rhost() call...
 */

INT32
xylo_rhost(n)
char **n;	/* name to work on */
{
    FILE *f;
    char linebuf[LB_SIZE];
    char hname[LB_SIZE];
    char *p;
    INT32 a;
    int i;
    extern UINT32 inet_addr();

#ifdef INSTALL_DIR
    sprintf(linebuf,"%s/hosts",INSTALL_DIR);
    if ((f = fopen(linebuf,"r")) == NULL)
	return(-1L);
#else
    if ((f = fopen("/etc/hosts","r")) == NULL)
	return(-1L);
#endif

    while(fgets(linebuf, LB_SIZE, f)) {

	/* handle comments */
	for(p = linebuf; *p; ++p)
	    if(*p == '#') {
		*p = '\0';
		break;
		}

	/* skip leading white */
	p = linebuf;
	while(isspace(*p)) ++p;
	if(!*p)
	    continue;

	/* hope for an Inet address */
	a = inet_addr(p);
	if(a == -1L)
	    continue;

	/* skip inet address */
	while(*p && !isspace(*p)) ++p;

	while(*p) {

	    while(isspace(*p)) ++p;
	    if(!*p)
		continue;

	    /* copy out hostname */
	    i = 0;
	    while(*p && !isspace(*p))
		hname[i++] = *p++;
	    hname[i] = '\0';
	    
	    /* compare */
	    if(!strcasecmp(*n, hname)) {
		fclose(f);
		return(a);
		}
	    }
	}

    fclose(f);
    return(-1L);
    }

#ifdef SLIP

/*
 * Case-insensitive string compare -- not very fast
 */
static int
strcasecmp(s1, s2)
char *s1, *s2;
{
    char c1, c2;

    while(*s1 && *s2) {
	c1 = *s1++;
	if(islower(c1)) c1 = toupper(c1);
	c2 = *s2++;
	if(islower(c2)) c2 = toupper(c2);
	if(c1 != c2)
	    return(c2 - c1);
    }

    return(*s2 - *s1);
}

#endif /*SLIP*/


/*
 * Xenix/V version of BSD gethostbyname()
 */

struct hostent *
xylo_gethostbyname(name)
char *name;
{
    INT32 rhost();
    static struct hostent h;
    static struct in_addr a;
    char *nname = name;

    a.s_addr = rhost(&nname);
    if(a.s_addr == -1L)
	return((struct hostent *)0);

    h.h_name = name;
    h.h_aliases = (char **)0;
    h.h_addrtype = AF_INET;
    h.h_length = sizeof(struct in_addr);
    h.h_addr = (char *)&a;
    return(&h);
    }


/*
 * Xenix/V version of the BSD gethostname() call
 * The alleged host name is kept in /etc/systemid
 */
int
xylo_gethostname(buf, len)
char *buf;
int len;
{
    int fd, len_got;

    *buf = '\0';

    fd = open("/etc/systemid", 0);
    if(fd < 0)
	return(-1);

    len_got = read(fd, buf, len - 1);
    if(len_got < 0) {
	close(fd);
	return(-1);
	}

    close(fd);
    buf[len - 1] = '\0';

    for(len = 0; len < len_got && buf[len]; ++len)
	if(buf[len] == '\n')
	    break;

    buf[len] = '\0';
    return(0);
    }
