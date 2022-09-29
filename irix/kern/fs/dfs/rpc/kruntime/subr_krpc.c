/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: subr_krpc.c,v 65.5 1998/03/24 16:21:58 lmc Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif

/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: subr_krpc.c,v $
 * Revision 65.5  1998/03/24 16:21:58  lmc
 * Fixed missing comment characters lost when the ident lines were
 * ifdef'd for the kernel.  Also now calls rpc_icrash_init() for
 * initializing symbols, and the makefile changed to build rpc_icrash.c
 * instead of dfs_icrash.c.
 *
 * Revision 65.4  1998/03/23  17:26:52  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1997/11/06 19:56:51  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:16:29  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.95.2  1996/02/18  00:00:54  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:53:56  marty]
 *
 * Revision 1.1.95.1  1995/12/08  00:15:35  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/dat_xidl2/jrr_1.2_mothra/1  1995/11/27  15:08 UTC  jrr
 * 	Change return type of abort() to void.
 * 
 * 	HP revision /main/dat_xidl2/1  1995/11/17  17:09 UTC  dat
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/12/07  23:56:35  root]
 * 
 * Revision 1.1.91.4  1994/02/02  21:49:05  cbrooks
 * 	OT9855 code cleanup breaks KRPC
 * 	[1994/02/02  21:00:29  cbrooks]
 * 
 * Revision 1.1.91.3  1994/01/23  21:37:12  cbrooks
 * 	RPC Code Cleanup - CR 9797
 * 	[1994/01/23  21:05:45  cbrooks]
 * 
 * Revision 1.1.91.2  1994/01/22  22:50:34  cbrooks
 * 	RPC Code Cleanup - CR9797
 * 	[1994/01/22  21:18:57  cbrooks]
 * 
 * Revision 1.1.91.1  1994/01/21  22:32:36  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:43  cbrooks]
 * 
 * Revision 1.1.8.2  1993/06/10  19:26:39  sommerfeld
 * 	   Portability fix for HPUX - do not #include endian.h.
 * 	   [1992/11/30  21:27:59  toml]
 * 	[1993/06/08  22:00:49  sommerfeld]
 * 
 * Revision 1.1.4.3  1993/01/03  22:37:05  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:53:49  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  19:40:43  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:54:38  zeliff]
 * 
 * Revision 1.1.2.2  1992/06/26  06:11:18  jim
 * 	added include of sys/domain.h and sys/protosw.h for AIX 3.2.
 * 	[1992/06/02  15:53:12  jim]
 * 
 * Revision 1.1  1992/01/19  03:16:33  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**
**
**  NAME:
**
**      subr_xxx.x  (in the true UNIX kernel tradition ;-)
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  This file contains several hacks to enable the NCK to reside in
**  the OSC kernel with minimum modifications to NCK.
**  
**  Many of these functions (particularly sprintf, sscanf and str*)
**  exist to support API operations that arguably should not be present
**  in a kernel (e.g. rpc_bindind_{to,from}_string_binding()).
**  However these API operations are certainly useful during debugging
**  and since it's not really not that difficult to make these
**  operations work in a kernel environment we include them.
**  In the future, perhaps they will be enclosed by "#ifdef DEBUG"
**  so that production kernels wouldn't need to carry this baggage.
**
**
*/

#include <dce/dce.h>
#include <commonp.h>

int sscann _DCE_PROTOTYPE_(( unsigned32 *, int, int, char *));
/*
 * Used by:
 *      DataGram protocol service boot time initialization (dg/dginit.c)
 *      RPC timer services (common/rpctimer.c)
 */
#ifndef USE_ALT_NCK_GETTIMEOFDAY
nck_gettimeofday
#ifdef _DCE_PROTO_
(
   struct timeval *tvp,
   void * tzp
)
#else
(tvp, tzp)
struct timeval *tvp;
void *tzp;
#endif
{
#if ! (defined (_AIX) && defined (_KERNEL))
    MICROTIME(tvp);
#else
    int pri;

    pin(tvp, sizeof (*tvp));
    pri = i_disable(INTTIMER);
    tvp->tv_sec = tod.tv_sec;
    tvp->tv_usec = tod.tv_nsec/1000;
    i_enable(pri);
    unpin(tvp, sizeof(*tvp));
#endif /* Not (_AIX and _KERNEL) */
    return 0; /* make the 6.4 mongoose compiler stop complaining */
}
#endif /* USER_ALT_NCK_GETTIMEOFDAY  */

/*
 * Used by common/rpcdbg.c as well as internally to this file.
 */
#ifdef NEED_ISDIGIT
int isdigit(c )
int c ;
{
    return ('0' <= c && c <= '9');
}
#endif /* NEED_ISDIGIT */

/*
 * Used by com/uuid.c.
 * (at least it's portable).
 */

#ifdef NEED_TOUPPER
int toupper(c)
int c;
{
    char *lower = "abcdefghijklmnopqrstuvwxyz";
    char *upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char *cp;

    for (cp=lower; *cp != '\0' && *cp != c; cp++)
        ;
    if (*cp == '\0')
        return (c);

    return (upper[cp-lower]);
}
#endif /* NEED_TOUPPER */

void
abort( void )
{
    panic("NCK runtime abort");
}


/*  
 * The following copyright applies to the BSD4.4 rand() function below
 * (from BSD4.4 lib/libc/stdlib/rand.c). 
 */
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

static unsigned32 next = 1;

int
rand( void )
{
	return ((next = next * 1103515245 + 12345) % 0x7fffffff);
}


/*
 * ========== A set of routines for dealing with sprintfs ==========
 */

/*
 * This is a rip off of the kernel printn() modified to
 * put its "output" to a buffer.
 */

PRIVATE void sprintn
#ifdef _DCE_PROTO_
(
 unsigned32 n,
 int b,
 int zf,
 int fld_size,
 char *buf
)
#else
(n, b, zf, fld_size, buf)
unsigned32 n;
int b;
int zf;
int fld_size;
char *buf;
#endif
{
    char prbuf[11];
    register char *cp;

    if (b == 10 && (signed32)n < 0) {
        *buf++ = '-';
        n = (unsigned32)(-(signed32)n);
    }

    cp = prbuf;
    do {
        *cp++ = "0123456789abcdef"[n%b];
        n /= b;
    } while (n);
    if (fld_size) {
        for (fld_size -= cp - prbuf; fld_size > 0; fld_size--)
            if (zf)
                *buf++ = '0';
            else
                *buf++ = ' ';
    }
    do
        *buf++ = *--cp;
    while (cp > prbuf);
    *buf = '\0';
}


/*
 * handle the format:
 *      "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x"
 * Used by uuid_to_string()
 */

PRIVATE void uuid__sprintf
#ifdef _DCE_PROTO_
(
  char * buf, 
  char * fmt, 
 unsigned32 time_low, unsigned32 time_mid, unsigned32 time_hi_and_version,
 unsigned32 clock_seq_hi_and_reserved, unsigned32 clock_seq_low,
 unsigned32 node_0, unsigned32 node_1, unsigned32 node_2, 
 unsigned32 node_3, unsigned32 node_4, unsigned32 node_5
)  
#else 
(buf, fmt, time_low, time_mid, time_hi_and_version,
            clock_seq_hi_and_reserved, clock_seq_low,
            node_0, node_1, node_2, node_3, node_4, node_5
char *buf;
char *fmt;
unsigned32 time_low;
#endif 
{
    sprintn((unsigned32)time_low, 16, 1, 8, buf); buf += 8;
    *buf++ = '-';
    sprintn((unsigned32)time_mid, 16, 1, 4, buf); buf += 4;
    *buf++ = '-';
    sprintn((unsigned32)time_hi_and_version, 16, 1, 4, buf); buf += 4;
    *buf++ = '-';
    sprintn((unsigned32)clock_seq_hi_and_reserved, 16, 1, 2, buf); buf += 2;
    sprintn((unsigned32)clock_seq_low, 16, 1, 2, buf); buf += 2;
    *buf++ = '-';
    sprintn((unsigned32)node_0, 16, 1, 2, buf); buf += 2;
    sprintn((unsigned32)node_1, 16, 1, 2, buf); buf += 2;
    sprintn((unsigned32)node_2, 16, 1, 2, buf); buf += 2;
    sprintn((unsigned32)node_3, 16, 1, 2, buf); buf += 2;
    sprintn((unsigned32)node_4, 16, 1, 2, buf); buf += 2;
    sprintn((unsigned32)node_5, 16, 1, 2, buf); buf += 2;
}


/*
 * handle the format:
 *      "%08x%04x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x"
 * Used by uuid_to_string()
 */

PRIVATE void uuid__old_sprintf
#ifdef _DCE_PROTO_
(
 char * buf,
 char * fmt,
 unsigned32 time_high, unsigned32 time_low, unsigned32 family ,
 unsigned32 host_0, unsigned32 host_1, unsigned32 host_2, 
 unsigned32 host_3, unsigned32 host_4, unsigned32 host_5, unsigned32 host_6
)
#else 
(buf, fmt, time_high, time_low, family,
            host_0, host_1, host_2, host_3, host_4, host_5, host_6)
char *buf;
char *fmt;
unsigned32 time_high;
#endif
{
    sprintn((unsigned32)time_high, 16, 1, 8, buf); buf += 8;
    sprintn((unsigned32)time_low, 16, 1, 4, buf); buf += 4;
    *buf++ = '.';
    sprintn((unsigned32)family, 16, 1, 2, buf); buf += 2;
    *buf++ = '.';
    sprintn((unsigned32)host_0, 16, 1, 2, buf); buf += 2;
    *buf++ = '.';
    sprintn((unsigned32)host_1, 16, 1, 2, buf); buf += 2;
    *buf++ = '.';
    sprintn((unsigned32)host_2, 16, 1, 2, buf); buf += 2;
    *buf++ = '.';
    sprintn((unsigned32)host_3, 16, 1, 2, buf); buf += 2;
    *buf++ = '.';
    sprintn((unsigned32)host_4, 16, 1, 2, buf); buf += 2;
    *buf++ = '.';
    sprintn((unsigned32)host_5, 16, 1, 2, buf); buf += 2;
    *buf++ = '.';
    sprintn((unsigned32)host_6, 16, 1, 2, buf); buf += 2;
}


/*
 * handle the format:
 *      "%u"
 * Used by rpc__ip_addr_inq_endpoint()
 */

PRIVATE void rpc__ip_endpoint_sprintf
#ifdef _DCE_PROTO_
(
  char *buf,
  char *fmt,
  int u
)
#else 
(buf, fmt, u)
char *buf;
char *fmt;
#endif
{
    if (u < 0)
        u = -u;
    sprintn((unsigned32)u, 10, 0, 0, buf);
}


/*
 * handle the format:
 *      "%d.%d.%d.%d"
 * Used by rpc__ip_addr_inq_netaddr()
 */

PRIVATE void rpc__ip_network_sprintf
#ifdef _DCE_PROTO_
(
  char * buf,
  char * fmt,
  int b1, int b2, int b3, int b4
)
#else 
(buf, fmt, b1, b2, b3, b4)
char *buf;
char *fmt;
#endif
{
    sprintn((unsigned32)b1, 10, 0, 0, buf); buf += strlen(buf);
    *buf++ = '.';
    sprintn((unsigned32)b2, 10, 0, 0, buf); buf += strlen(buf);
    *buf++ = '.';
    sprintn((unsigned32)b3, 10, 0, 0, buf); buf += strlen(buf);
    *buf++ = '.';
    sprintn((unsigned32)b4, 10, 0, 0, buf); buf += strlen(buf);
}

/*
 * handle the format:
 *      "%s, %lu.%u"
 * Used by rpc__dg_act_seq_string()
 */

PRIVATE void rpc__dg_act_seq_sprintf
#ifdef _DCE_PROTO_
(
  char *buf,
  char *fmt,
  char *s,
  unsigned32 ul,
  unsigned32 u 
)
#else 
(buf, fmt, s, ul, u)
char *buf;
char *fmt;
char *s;
unsigned32 ul;
#endif
{
    strcpy(buf, s); buf += strlen(buf);
    *buf++ = ',';
    *buf++ = ' ';
    sprintn((unsigned32)ul, 10, 0, 0, buf); buf += strlen(buf);
    *buf++ = '.';
    sprintn((unsigned32)u, 10, 0, 0, buf);
}



/*
 * ========== A set of routines for dealing with sscanfs ==========
 */

/*
 * Don't let the name fool you, sscann really only does what
 * we need..
 */

INTERNAL int sscann
#ifdef _DCE_PROTO_
(
  unsigned32 *n,
  int b,
  int fld_size,
  char *buf
)
#else 
(n, b, fld_size, buf)
unsigned32 *n;
int b;
int fld_size;
char *buf;
#endif
{
    register c;
    int nfound = 0;
    unsigned32 lcval;
    extern int isdigit(int);

    lcval = 0;
    if (fld_size == 0)
        fld_size = 11;
    for (c = *buf++; --fld_size>=0; c = *buf++) {
        if (isdigit(c)
         || b==16 && ('a'<=c && c<='f' || 'A'<=c && c<='F')) {
            nfound++;
            if (b==8)
                lcval <<=3;
            else if (b==10)
                lcval = ((lcval<<2) + lcval)<<1;
            else
                lcval <<= 4;
            if (isdigit(c))
                c -= '0';
            else if ('a'<=c && c<='f')
                c -= 'a'-10;
            else
                c -= 'A'-10;
            lcval += c;
            continue;
        } else
            break;
    }
    if (nfound == 0)
        return(0);

    *n = lcval;
    return(1);
}

/*
 * atoi (ncstest uses it)
 */

#if ! defined (_AIX) && ! defined (SGIMIPS)

int atoi
#ifdef _DCE_PROTO_
(
  char * s
)
#else 
(s)
char *s;
#endif
{
    int i;

    return ((char *)sscann((unsigned32 *)&i, 10, 0, s) ? i : -1);
}
#endif /*  ! defined _AIX */

/*
 * handle the format:
 *      "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x"
 * Used by uuid_from_string()
 */

PRIVATE int uuid__sscanf 
#ifdef _DCE_PROTO_ 
(
  char * buf,
  char * fmt,
  unsigned32 * time_low,
  int * time_mid, int * time_hi_and_version,
  int *clock_seq_hi_and_reserved, 
  int *clock_seq_low,
  int *node_0, int *node_1, int *node_2, int *node_3, 
  int *node_4, int *node_5
)
#else 
(buf, fmt, time_low, time_mid, time_hi_and_version,
            clock_seq_hi_and_reserved, clock_seq_low,
            node_0, node_1, node_2, node_3, node_4, node_5)
char *buf;
char *fmt;
unsigned32 *time_low;
int *time_mid, *time_hi_and_version;
int *clock_seq_hi_and_reserved, *clock_seq_low;
int *node_0, *node_1, *node_2, *node_3, *node_4, *node_5;
#endif
{
    unsigned32 n;
    int found = 0;

    found += sscann(&n, 16, 8, buf); buf += 8; *time_low = n;
    buf++;
    found += sscann(&n, 16, 4, buf); buf += 4; *time_mid = n;
    buf++;
    found += sscann(&n, 16, 4, buf); buf += 4; *time_hi_and_version = n;
    buf++;
    found += sscann(&n, 16, 2, buf); buf += 2; *clock_seq_hi_and_reserved = n;
    found += sscann(&n, 16, 2, buf); buf += 2; *clock_seq_low = n;
    buf++;
    found += sscann(&n, 16, 2, buf); buf += 2; *node_0 = n;
    found += sscann(&n, 16, 2, buf); buf += 2; *node_1 = n;
    found += sscann(&n, 16, 2, buf); buf += 2; *node_2 = n;
    found += sscann(&n, 16, 2, buf); buf += 2; *node_3 = n;
    found += sscann(&n, 16, 2, buf); buf += 2; *node_4 = n;
    found += sscann(&n, 16, 2, buf); buf += 2; *node_5 = n;
    return (found);
}


/*
 * handle the format:
 *      "%08x%04x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x"
 * Used by uuid_from_string()
 */

PRIVATE int uuid__old_sscanf
#ifdef _DCE_PROTO_
(
  char *buf,
  char *fmt,
  unsigned32 *time_high,
  int *time_low,
  int *family,
  int *host_0, 
  int *host_1, 
  int *host_2, 
  int *host_3, 
  int *host_4, 
  int *host_5, 
  int *host_6
)
#else 
(buf, fmt, time_high, time_low, family,
            host_0, host_1, host_2, host_3, host_4, host_5, host_6)
char *buf;
char *fmt;
unsigned32 *time_high;
int *time_low, *family;
int *host_0, *host_1, *host_2, *host_3, *host_4, *host_5, *host_6;
#endif 
{
    unsigned32 n;
    int found = 0;

    found += sscann(&n, 16, 8, buf); buf += 8; *time_high = n;
    found += sscann(&n, 16, 4, buf); buf += 4; *time_low = n;
    buf++;
    found += sscann(&n, 16, 2, buf); buf += 2; *family = n;
    buf++;
    found += sscann(&n, 16, 2, buf); buf += 2; *host_0 = n;
    buf++;
    found += sscann(&n, 16, 2, buf); buf += 2; *host_1 = n;
    buf++;
    found += sscann(&n, 16, 2, buf); buf += 2; *host_2 = n;
    buf++;
    found += sscann(&n, 16, 2, buf); buf += 2; *host_3 = n;
    buf++;
    found += sscann(&n, 16, 2, buf); buf += 2; *host_4 = n;
    buf++;
    found += sscann(&n, 16, 2, buf); buf += 2; *host_5 = n;
    buf++;
    found += sscann(&n, 16, 2, buf); buf += 2; *host_6 = n;
    return (found);
}


/*
 * handle the format:
 *      "%u"
 * Used by rpc__ip_addr_set_endpoint
 */

PRIVATE int rpc__ip_endpoint_sscanf(buf, fmt, ul)
char *buf;
char *fmt;
unsigned32 *ul;
{
    unsigned32 n;
    int found = 0;

    found += sscann(&n, 10, 0, buf); *ul = n;
    return (found);
}


/*
 * ========== The following are all of the functions in "string.h" ==========
 *
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifdef NEED_STRCAT

char *
strcat
#ifdef _DCE_PROTO_ 
(
  char * sa, const char * appenda
)
#else 
(sa, appenda)
char * sa;
char * apa;
#endif
{
    register char * s = sa;
    const char * append = appenda;
	char *save = s;

	for (; *s; ++s);
	while (*s++ = *append++);
	return(save);
}
#endif /* NEED_STRCAT */

#ifdef NEED_STRCMP
int strcmp
#ifdef _DCE_PROTO_
(
 register char * s1, register char * s2
)
#else
(s1, s2)
	register char *s1, *s2;
#endif
{
	for (; *s1 == *s2; ++s1, ++s2)
		if (!*s1)
			return(0);
	return(*s1 - *s2);
}
#endif

#ifdef NEED_STRCPY
char *
strcpy
#ifdef _DCE_PROTO_
(
  register char * to, register char * from
)
#else 
(to, from)
	register char *to, *from;
#endif
{
	char *save = to;

	for (; *to = *from; ++from, ++to);
	return(save);
}
#endif /* NEED_STRCPY */

#ifdef NEED_STRNCAT
char *
strncat
#ifdef _DCE_PROTO_
(
 register char * s,
 register const char * append,
 register size_t cnt
)
#else 
(s, append, cnt)
	register char *sa;
        register const char  *appenda;
	register int cnta;
#endif
{

	char *save = s;

	for (; *s; ++s);
	for (; cnt; --cnt)
		*s++ = *append++;
	*s = '\0';
	return(save);
}
#endif /* NEED_STRNCAT */

#ifdef NEED_STRNCMP
strncmp(s1, s2, cnt)
	register char *s1, *s2;
	register int cnt;
{
	for (; cnt && *s1 == *s2; --cnt, ++s1, ++s2)
		if (!*s1)
			return(0);
	return(cnt ? *s1 - *s2 : 0);
}
#endif /* NEED_STRNCMP */

#ifdef NEED_STRSPN
size_t strspn 
#ifdef _DCE_PROTO_ 
(
    register const char *s1,
    register const char *s2
)
#else
(s1, s2)
    register const char *s1, *s2;
#endif 
{
    register int l = 0;
    register const char *ts2;

    for (; *s1; ++s1)
        for (ts2 = s2; *ts2; ++ts2)
            if (*ts2 == *s1)
            {
                l++;
                break;
            }

    return l;
}
#endif /* NEED_STRSPN */

#ifdef NEED_STRNCPY
char *
strncpy
#ifdef _DCE_PROTO_
(
 register char *to, 
 register char  *from,
 register int cnt
)
#else 
(to, from, cnt)
	register char *to, *from;
	register int cnt;
#endif
{
	char *save = to;

	for (; cnt && (*to = *from); --cnt, ++from, ++to);
	for (; cnt; --cnt, ++to)
		*to = '\0';
	return(save);
}
#endif /* NEED_STRNCPY */

#ifdef NEED_STRLEN
strlen
#ifdef _DCE_PROTO_
(
	register char *str
)
#else 
(str)
	register char *str;
#endif
{
	register int cnt;

	for (cnt = 0; *str; ++cnt, ++str);
	return(cnt);
}
#endif /* NEED_STRLEN */

#ifdef NEED_INDEX
#ifndef AIX32
char *
index
#ifdef _DCE_PROTO_
( const char *p, int ch) 
#else 
index
(p, ch)
	const char *p;
	int ch;
#endif
{
	for (;; ++p) {
		if (*p == ch)
			return(p);
		if (!*p)
			return((char *)NULL);
	}
	/* NOTREACHED */
}
#endif /* _AIX32 */
#endif /* NEED_INDEX */

#ifdef NEED_RINDEX
#ifndef AIX32
char *
rindex
#ifdef _DCE_PROTO_
(
 const char *p,
 int ch
)
#else 
(p, ch)
	register const char *p;
	register int ch;
#endif
{
	register const char *save;

	for (save = NULL;; ++p) {
		if (*p == ch)
			save = p;
		if (!*p)
			return((char *)save);
	}
	/* NOTREACHED */
}
#endif /* _AIX32 */
#endif /* NEED_RINDEX */

/*
 * This is only here to support rpc_die().
 */
#ifdef NEED_STRRCHR
char *
strrchr
#ifdef _DCE_PROTO_
(
 register const char *p,
#ifdef SGIMIPS
 register int ch 
#else 
 const register int ch 
#endif
)
#else 
(p, ch)
	register char *p;
	register int  ch;
#endif
{
	register char *save;

	for (save = NULL;; ++p) {
		if (*p == ch)
			save = (char *)p;
		if (!*p)
			return(save);
	}
	/* NOTREACHED */
}
#endif /* NEED_STRRCHR */

#ifdef NEED_MEMCPY
void *
memcpy 
#ifdef _DCE_PROTO_
(
 register void *src,
 register const void *dst,
 register size_t len
)
#else 
(src, dst, len)
         register void *src;
         register const void *dst;
         register size_t len;
#endif
{
    bcopy(dst, src, len);
    return 0;  /* silence the 6.4 mogoose compiler */
}
#endif /* NEED_MEMCPY */

#ifdef NEED_MEMCMP
int
memcmp 
#ifdef _DCE_PROTO_
(
 register const void *src,
 register const void *dst,
 register size_t len
)
#else 
(src, dst, len)
         register const void *src;
         register const void *dst;
         register size_t len;
#endif
{
    return(bcmp(dst, src, len));
}
#endif /* NEED_MEMCMP */

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */


#ifdef NEED_MEMSET
void *
memset
#ifdef _DCE_PROTO_
(
 void *dst,
 register int c,
 register size_t n
)
#else 
(dst, c, n)
	void *dst;
	register int c;
	register size_t n;
#endif
{

	if (n != 0) {
		register char *d = dst;

		do
			*d++ = c;
		while (--n != 0);
	}
	return ((char *) dst);
}
#endif /* NEED_MEMSET */

#ifdef NEED_MEMMOVE 

void * 
memmove
#ifdef _DCE_PROTO_
(
  register void * dst,
  register const void * src,
  register size_t n 
)
#else
(dst, src, n)
register void * dst;
register const void * src;
register size_t n ;
#endif
{
    bcopy( src, dst, n ) ;
    return dst ;
}
#endif /* NEED_MEMMOVE */

/*
 * Again... more support for string conversions
 */
/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifdef NEED_ISXDIGIT
#define isxdigit(c)   (isdigit(c) || \
                            ('a' <= (c) && (c) <= 'f') || \
                            ('A' <= (c) && (c) <= 'F'))
#define isspace(c)    ((c) == ' ' || (c) == '\t')
#define islower(c)    ('a' <= (c) && (c) <= 'z')
#endif /* NEED_ISXDIGIT */

#ifndef	SGIMIPS
#ifdef	__OSF__
#include <machine/endian.h>
#endif
#ifdef	_AIX
#include <sys/machine.h>
#endif

#if defined(_AIX) && defined(_KERNEL)
#include <sys/domain.h>
#include <sys/protosw.h>
#endif

#include <netinet/in.h>

#ifndef INADDR_NONE
#define	INADDR_NONE		0xffffffff		/* -1 return */
#endif /* INADDR_NONE */

/*
 * Internet address interpretation routine.
 * All the network library routines call this
 * routine to interpret entries in the data bases
 * which are expected to be an address.
 * The value returned is in network order.
 */
u_long
inet_addr
#ifdef _DCE_PROTO_
(
 register char *cp
)
#else 
(cp)
register char *cp;
#endif
{
	register u_long val, base, n;
	register char c;
	u_long parts[4], *pp = parts;

again:
	/*
	 * Collect number up to ``.''.
	 * Values are specified as for C:
	 * 0x=hex, 0=octal, other=decimal.
	 */
	val = 0; base = 10;
	if (*cp == '0') {
		if (*++cp == 'x' || *cp == 'X')
			base = 16, cp++;
		else
			base = 8;
	}
	while (c = *cp) {
		if (isdigit(c)) {
			val = (val * base) + (c - '0');
			cp++;
			continue;
		}
		if (base == 16 && isxdigit(c)) {
			val = (val << 4) + (c + 10 - (islower(c) ? 'a' : 'A'));
			cp++;
			continue;
		}
		break;
	}
	if (*cp == '.') {
		/*
		 * Internet format:
		 *	a.b.c.d
		 *	a.b.c	(with c treated as 16-bits)
		 *	a.b	(with b treated as 24 bits)
		 */
		if (pp >= parts + 4)
			return (INADDR_NONE);
		*pp++ = val, cp++;
		goto again;
	}
	/*
	 * Check for trailing characters.
	 */
	if (*cp && !isspace(*cp))
		return (INADDR_NONE);
	*pp++ = val;
	/*
	 * Concoct the address according to
	 * the number of parts specified.
	 */
	n = pp - parts;
	switch (n) {

	case 1:				/* a -- 32 bits */
		val = parts[0];
		break;

	case 2:				/* a.b -- 8.24 bits */
		val = (parts[0] << 24) | (parts[1] & 0xffffff);
		break;

	case 3:				/* a.b.c -- 8.8.16 bits */
		val = (parts[0] << 24) | ((parts[1] & 0xff) << 16) |
			(parts[2] & 0xffff);
		break;

	case 4:				/* a.b.c.d -- 8.8.8.8 bits */
		val = (parts[0] << 24) | ((parts[1] & 0xff) << 16) |
		      ((parts[2] & 0xff) << 8) | (parts[3] & 0xff);
		break;

	default:
		return (INADDR_NONE);
	}
	val = htonl(val);
	return (val);
}
#endif
