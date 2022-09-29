/*	Copyright (c) 1990, 1991, 1992 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1985, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)acct:common/cmd/acct/acctdusg.c	1.9.2.4"
#ident "$Revision: 1.6 $"
/*
 *	acctdusg [-u file] [-p file] > dtmp-file
 *	-u	file for names of files not charged to anyone
 *	-p	get password info from file
 *	reads std input (normally from find / -print)
 *	and computes disk resource consumption by login
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include "acctdef.h"

struct	disk{
	char	dsk_name[MAXNAME+1];	/* login name */
	uid_t	dsk_uid;		/* user id of login name */
	long long dsk_du;		/* disk usage */
};

char	*pfile = NULL;
 
struct disk	*usglist;  /* holds data on disk usg by uid */
int 	maxusers;

FILE	*pwf, *nchrg = NULL;
FILE	*names ={stdin};

extern int _pw_stayopen;

main(argc, argv)
char	**argv;
int 	argc;
{
	char	fbuf[BUFSIZ], *fb, *strchr();
	struct passwd	*(*getpw)();
	void	(*endpw)();
	void	hashinit(), openerr(), output(), makdlst();
	void	charge(), end_pwent();
	int	stpwent();
	struct passwd	*pw;
	unsigned	hash();
#ifdef DEBUG
	void	pdisk();
#endif
	char *str;

	/* allocate memory for usglist */
	str = getenv(ACCT_MAXUSERS);
	if (str == NULL) 
		maxusers = MAXUSERS;
	else {
		maxusers = strtol(str, (char **)0, 0);
		if (errno == ERANGE || maxusers < MAXUSERS)
			maxusers = MAXUSERS;
	}
	usglist = calloc(maxusers, sizeof(struct disk));
	if (usglist == NULL) {
		fprintf(stderr, "%s: Cannot allocate memory\n", argv[0]);
		exit(5);
	}
	while(--argc > 0){
		++argv;
		if(**argv == '-') switch((*argv)[1]) {
		case 'u':
			if (--argc <= 0)
				break;
			if ((nchrg = fopen(*(++argv),"w")) == NULL)
				openerr(*argv);
			chmod(*argv, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
			continue;
		case 'p':
			if (--argc <= 0)
				break;
			pfile = *(++argv);
			continue;
		}
		fprintf(stderr,"Invalid argument: %s\n", *argv);
		exit(1);
	}

	_pw_stayopen = 1;
	if (pfile) {
		if (!stpwent(pfile)) {
			openerr(pfile);
		}
		getpw = fgetpwent;
		endpw = end_pwent;
	} else {
		setpwent();
		getpw = getpwent;
		endpw = endpwent;
	}

	hashinit();

	while ((pw = getpw(pwf)) != NULL) {
		makdlst(pw);	/* fill usglist with the user's in the
					password file */
	}
	endpw();

	/* charge the files listed in names to users listed in the usglist */

	while( fgets(fbuf, sizeof fbuf, names) != NULL) {
		if (fb = strchr(fbuf, '\n')) {  /* replace the newline char */
			*fb = '\0';		/* at the end of the filename */
		}				/* with a null character */
		charge(fbuf);
	}

	output();

	if (nchrg)
		fclose(nchrg);
#ifdef DEBUG
		pdisk();
#endif

	exit(0);
}

void
hashinit() {
	int	index;

	for(index=0; index < maxusers ; index++)
	{
         	usglist[index].dsk_uid = UNUSED;
		usglist[index].dsk_du = 0;
		usglist[index].dsk_name[0] = '\0';
	}
}

unsigned
hash(j)
uid_t j;
{
 	register unsigned start;
	register unsigned circle;
	circle = start = (unsigned)j % maxusers;
	do
        {
         	if ( usglist[circle].dsk_uid == j
		     || usglist[circle].dsk_uid == UNUSED )
  			return (circle);
		circle = (circle + 1) % maxusers;
	} while ( circle != start);
	return (FAIL);
}

void
openerr(file)
char	*file;
{
	fprintf(stderr, "Cannot open %s\n", file);
	exit(1);
}

void
output()
{
	int	index;

 	for (index=0; index < maxusers ; index++)
		if ( usglist[index].dsk_uid != UNUSED
		     && usglist[index].dsk_du != 0 )
  			printf("%ld	%s	%lld\n",
                            usglist[index].dsk_uid,
                            usglist[index].dsk_name,
                            usglist[index].dsk_du);
}


/*
 *	make a list of all the
 *	users in password file
 */

void
makdlst(p)
register struct passwd	*p;
{

	int    i, index;

	if ((index = hash(p->pw_uid)) == FAIL) {
		fprintf(stderr,"acctdusg: INCREASE THE VALUE OF THE ENVIRONMENT VARIABLE ACCT_MAXUSERS\n");
		return;
	}
	if (usglist[index].dsk_uid == UNUSED) {
		usglist[index].dsk_uid = p->pw_uid;
		strncpy(usglist[index].dsk_name, p->pw_name, MAXNAME);
	}
	/* uncomment this if it appears that users are being missed
	else {
		fprintf(stderr, "acctdusg:  %s not put in usglist\n", p->pw_name);
	}
	*/

	return;
}


void
charge(n)
register char *n;
{
	struct stat64	statb;
	register index;

	if(lstat64(n,&statb) == -1)
		return;

	if (((index = hash(statb.st_uid)) != FAIL)
	    && (usglist[index].dsk_uid == statb.st_uid)) {
		usglist[index].dsk_du += statb.st_blocks;
	} else if (nchrg) {
			fprintf(nchrg, "%9l\t%7llu\t%s\n",
			statb.st_uid, statb.st_blocks, n);
	}
	return;
}

#ifdef DEBUG
void
pdisk()
{
	int	index;

	for(index=0; index < maxusers ; index++)
		fprintf(stderr, "%.8s\t%9l\t%7lld\n",
			usglist[index].dsk_name,
			usglist[index].dsk_uid,
			usglist[index].dsk_du);
}
#endif

stpwent(pfile)
register char *pfile;
{
        if(pwf == NULL)
                pwf = fopen(pfile, "r");
        else
                rewind(pwf);
        return(pwf != NULL);
}

void
end_pwent()
{
        if(pwf != NULL) {
                (void) fclose(pwf);
                pwf = NULL;
        }
}
