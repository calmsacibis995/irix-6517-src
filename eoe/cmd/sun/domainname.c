/* @(#)domainname.c	1.2 87/07/17 3.2/4.3NFSSRC */
/* @(#)domainname.c	1.2 86/11/11 NFSSRC */
#ifndef lint
static	char sccsid[] = "@(#)domainname.c 1.1 86/09/24 Copyr 1984 Sun Micro";
#endif
/*
 * domainname -- get (or set domainname)
 */
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/capability.h>

static char domainname[MAXHOSTNAMELEN];

int
main(argc,argv)
	char *argv[];
{
	int	myerrno;
	cap_t	ocap;
	cap_value_t cap_sysinfo_mgt = CAP_SYSINFO_MGT;

	argc--;
	argv++;
	errno = 0;
	if (argc) {
		ocap = cap_acquire(1, &cap_sysinfo_mgt);
		if (setdomainname(*argv,strlen(*argv)))
			perror("setdomainname");
		cap_surrender(ocap);
		myerrno = errno;
	} else {
		getdomainname(domainname,sizeof(domainname));
		myerrno = errno;
		printf("%s\n",domainname);
	}
	exit(myerrno);
	return(myerrno);
}
