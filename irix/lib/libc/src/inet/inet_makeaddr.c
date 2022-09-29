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

#ifdef __STDC__
	#pragma weak inet_makeaddr = _inet_makeaddr
#endif
#include "synonyms.h"

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)inet_makeaddr.c	5.3 (Berkeley) 6/27/88";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <netinet/in.h>

/*
 * Formulate an Internet address from network + host.  Used in
 * building addresses stored in the ifnet structure.
 */
struct in_addr
inet_makeaddr(int net, int host)
{
	struct in_addr a;

	if ((unsigned)net < 128)
		a.s_addr = ((net << IN_CLASSA_NSHIFT) | (host & IN_CLASSA_HOST));
	else if ((unsigned)net < 65536)
		a.s_addr = ((net << IN_CLASSB_NSHIFT) | (host & IN_CLASSB_HOST));
	else
		a.s_addr = ((net << IN_CLASSC_NSHIFT) | (host & IN_CLASSC_HOST));
	HTONL(a.s_addr);
	return a;
}
