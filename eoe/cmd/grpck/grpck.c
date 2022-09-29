/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)grpck:grpck.c	1.3"
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <pwd.h>
#include <limits.h>

#define BADLINE "Too many/few fields"
#define TOOLONG "Line too long"
#define NONAME	"No group name"
#define BADNAME "Bad character(s) in group name"
#define BADGID  "Invalid GID"
#define NULLNAME "Null login name"
#define NOTFOUND "Logname not found in password file"
#define DUPNAME "Duplicate logname entry"
#define NOMEM	"Out of memory"
#define NGROUPS	"Maximum groups exceeded for logname "

int eflag, badchar, baddigit,badlognam,colons,len,i;

#define MYBUFSIZE	512	/* max line length including newline and null */

char buf[MYBUFSIZE];
char tmpbuf[MYBUFSIZE];

char *nptr;
char *cptr;
FILE *fptr;
int delim[MYBUFSIZE];
gid_t gid;
int error();

struct group {
	struct group *nxt;
	int cnt;
	gid_t grp;
};

struct node {
	struct node *next;
	int ngroups;
	struct group *groups;
	char user[1];
};

void *
emalloc(size)
{
	void *vp;
	vp = malloc(size);
	if (vp == NULL) {
		fprintf(stderr, "%s\n", NOMEM);
		exit(1);
	}
	return vp;
}

main (argc,argv)
int argc;
char *argv[];
{
	struct passwd *pwp;
	struct node *root = NULL;
	struct node *t;
	struct group *gp;
	int ngroups_max;

#ifdef NOT_WORKING
	int listlen;	/* See comments below */
#endif /* NOT_WORKING */

	int i;
	char c;		/* SGI */
	int ypflag;	/* SGI */

	ngroups_max = sysconf(_SC_NGROUPS_MAX);

	if ( argc == 1)
		argv[1] = "/etc/group";
	else if ( argc != 2 ) {
		printf ("\nusage: %s filename\n\n",*argv);
		exit(1);
	}

	if ( ( fptr = fopen (argv[1],"r" ) ) == NULL ) {
		printf ("\ncannot open file %s\n\n",argv[1]);
		exit(1);
	}

	while ((pwp = getpwent()) != NULL) {
		t = (struct node *)emalloc(sizeof(*t) + strlen(pwp->pw_name));
		t->next = root;
		root = t;
		strcpy(t->user, pwp->pw_name);
		t->ngroups = 1;
		if (!ngroups_max)
			t->groups = NULL;
		else {
			t->groups = (struct group*)
			  emalloc(sizeof(struct group));
			t->groups->grp = pwp->pw_gid;
			t->groups->cnt = 1;
			t->groups->nxt = NULL;
		}
	}


	while(fgets(buf,MYBUFSIZE,fptr) != NULL ) {
		eflag=0;	/* Bug fix to allow printing of TOOLONG error */
		ypflag=0;	/* SGI */

		if ( buf[0] == '\n' )    /* blank lines are ignored */
			continue;

		i = strlen(buf);
		if ( (i == (MYBUFSIZE-1)) && (buf[i-1] != '\n') ) {  
			/* line too long */
			buf[i-1] = '\n';	/* add newline for printing */
			error(TOOLONG);

#ifdef NOT_WORKING

		/* 
		 * If two lines fail the "TOOLONG" test and the lines are 
		 * back to back in the group file - The following code will
		 * not report an error message on the second line that is 
		 * "TOOLONG".  Thus, reason for ifdefing code out.
		 */
			while(fgets(tmpbuf,MYBUFSIZE,fptr) != NULL )  {
				i = strlen(tmpbuf);
				if ( (i == (MYBUFSIZE-1)) 
				  && (tmpbuf[i-1] != '\n') )
					/* another long line */
					continue;
				else
					break;
			}
#endif /* NOT_WORKING */

			/* done reading continuation line(s) */

		while((c = getc(fptr)) != '\n');  /* SGI */

			strcpy(tmpbuf, buf);
		} else {
			/* change newline to comma for strchr */
			strcpy(tmpbuf, buf);
			tmpbuf[i-1] = ',';	
		}

		colons=0;
		badchar=0;
		baddigit=0;
		badlognam=0;
		gid=(gid_t)0;

		/* Check yellow page entry - SGI */

		if (buf[0] == '+' || buf[0] == '-')
			ypflag++;

		/* Check number of fields */

		for (i=0 ; buf[i]!=NULL ; i++) {
			if (buf[i]==':') {
				delim[colons]=i;
				++colons;
			}
		}

		/* Addition of ypflag check - SGI */

		if (!ypflag && colons != 3 ) {
			error(BADLINE);
			continue;
		}

		/* check to see that group name is at least 1 character	*/
		/* and that all characters are printable.		*/

		if ( buf[0] == ':' )
			error(NONAME);
		else {
			for ( i=0; buf[i] != ':' && buf[i] != '\n' && buf[i] != '\0'; i++ ) {
				if (!isprint(buf[i]) && (!ypflag))
					badchar++;
			}

			if ( badchar > 0 )
				error(BADNAME);
		}

		/* 
		 * If a yp entry in group file - ignore the following checks :
		 *	valid gid, null login name, valid login name in passwd
		 *	and duplicate login names. - SGI
		 */

		if (ypflag) 
			continue;

		len = ( delim[2] - delim[1] ) - 1;

		if ( len <= 5 && len > 0 ) {
			for ( i=(delim[1]+1); i < delim[2] && !baddigit; i++ ) {
				if (!isdigit(buf[i]))
					baddigit++;
				else 			/* converts ascii GID to decimal */
					gid=gid * 10 + (gid_t)(buf[i] - '0');
			}

			if ( !baddigit && (gid > UID_MAX || gid < (gid_t)0) )
				baddigit++;
		}
		else 
			baddigit++;

		if (baddigit > 0) {
			error(BADGID);
			continue;
		}

		nptr = &tmpbuf[delim[2]];
		nptr++;

#ifdef NOT_WORKING
		listlen = strlen(nptr) - 1;  /* See comments below */
#endif /* NOT_WORKING */

		while ( ( cptr = strchr(nptr,',') ) != NULL ) {

			*cptr=NULL;

			/* Checking for groups with null login name */

			if (*nptr == NULL) {
#ifdef NOT_WORKING
		/*
		 * If this "if" statement is kept then group names
		 * that are null would not have been caught as errors.
		 */ 
				if (listlen)
					error(NULLNAME);
#endif /* NOT_WORKING */
				error(NULLNAME);
				nptr++;
				continue;
			}

			/*  check that logname appears in the passwd file  */

			for (t = root; ; t = t->next) {
				if (t == NULL){
					badlognam++;
					error(NOTFOUND);
					goto getnext;
				}
				if (strcmp(t->user, nptr) == 0)
					break;
			}
			
			if (!ngroups_max)
				goto getnext;

			t->ngroups++;

			/* check for duplicate logname in group */

			for (gp = t->groups; gp != NULL; gp = gp->nxt) {
				if (gid == gp->grp) {
					if (gp->cnt++ == 1) {
						badlognam++;
						error(DUPNAME);
					}
					goto getnext;
				}
			}

			gp = (struct group*)emalloc(sizeof(struct group));
			gp->grp = gid;
			gp->cnt = 1;
			gp->nxt = t->groups;
			t->groups = gp;
getnext:
			nptr = ++cptr;
		}
	}

	if (ngroups_max) {
		for (t = root; t != NULL; t = t->next) {
			if (t->ngroups > ngroups_max)
				fprintf(stderr,"\n\n%s%s (%d)\n",
			  	  NGROUPS, t->user, t->ngroups);
		}
	}
}

/*	Error printing routine	*/

error(msg)
char *msg;
{
	if ( eflag==0 ) {
		fprintf(stderr,"\n\n%s",buf);
		eflag=1;
	}

	if ( badchar != 0 ) {
		fprintf (stderr,"\t%d %s\n",badchar,msg);
		badchar=0;
		return;
	}
	else if ( baddigit != 0 ) {
		fprintf (stderr,"\t%s\n",msg);
		baddigit=0;
		return;
	}
	else if ( badlognam != 0 ) {
		fprintf (stderr,"\t%s - %s\n",nptr,msg);
		badlognam=0;
		return;
	}
	else {
		fprintf (stderr,"\t%s\n",msg);
		return;
	}
}
