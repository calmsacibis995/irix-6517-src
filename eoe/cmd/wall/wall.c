/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.11 $"

#define	FAILURE	(-1)
#define	TRUE	1
#define	FALSE	0

#include <signal.h>
#include <malloc.h>

char	mesg[3000];

#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utmp.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <pwd.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <locale.h>
#include <fmtmsg.h>
#include <unistd.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>
#include <sys/capability.h>

int	entries;
char	*infile;
int	group;
struct	group *pgrp;
char	*grpname;
char	line[MAXNAMLEN+1] = "???";
char	*systm;
char	*timstr;
long	tloc;
extern	char *ttyname();
unsigned int usize;
struct	utmp *utmp;
struct	utsname utsn;
char	who[32]	= "???";
char	utmp_file[] = UTMP_FILE;

char	cmd_label[] = "UX:wall";

#define equal(a,b)		(!strcmp( (a), (b) ))

static void readargs(int, char **);
static void sendmes(struct utmp *);
static int  chkgrp(char *);

/*
 * some error prints
 */
static void
err_open(s)
char *s;
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_CannotOpen, "Cannot open %s"), s);
	exit(1);
}

static void
err_read(s)
char *s;
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_CannotRead, "Cannot read from %s"), s);
}

static void
err_stat(s)
char *s;
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_cannotstat, "cannot stat %s"), s);
}

static void
err_nomem()
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_outofmem, "Out of memory"));
	exit(1);
}

/*
 * main entry
 */
int
main(argc, argv)
int	argc;
char	*argv[];
{
	int	i=0, fd;
	register	struct utmp *p;
	FILE	*f;
	struct	stat statbuf;
	struct	dirent *direntry;
	DIR *devp = NULL;
	register	char *ptr;
	struct	passwd *pwd;

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	if(uname(&utsn) == -1) {
		_sgi_nl_error(SGINL_SYSERR, cmd_label,
		    gettxt(_SGI_DMMX_cannot_uname, "Cannot uname()"));
		exit(2);
	}
	systm = utsn.nodename;

	if(stat(utmp_file, &statbuf) < 0) {
		err_stat(utmp_file);
		exit(1);
	}
	usize = statbuf.st_size;
	entries = usize / sizeof(struct utmp);
	if((utmp=(struct utmp *)malloc(usize)) == NULL)
		err_nomem();

	if((fd=open(utmp_file, O_RDONLY)) < 0)
		err_open(utmp_file);
	if(read(fd, (char *) utmp, usize) != usize) {
		err_read(utmp_file);
		exit(1);
	}
	close(fd);
	readargs(argc, argv);

	/*
		Get the name of the terminal wall is running from.
	*/

	if (isatty(fileno(stderr)) && fstat(fileno(stderr),&statbuf) !=
		FAILURE && ((devp = opendir("/dev")) != NULL)) {


			/*
				Read in directory entries for /dev and look for
				ones with an inode number equal to the inode 
				number of our standard error output.
			*/
			while ((direntry = readdir(devp)) != NULL) {
				if (direntry->d_ino != statbuf.st_ino) 
					direntry->d_ino = 0;
				else { 
					strcpy(line, direntry->d_name);
					break;
				}
			}

			/*
				If we've reached the end of the dev directory 
				and can't find another name for this terminal, 
				finally give up
			*/

	} 
	if (who[0] == '?') {
		if (pwd = getpwuid((int) getuid()))
			strncpy(&who[0],pwd->pw_name,sizeof(who));
	}
	if (devp != NULL) closedir(devp);

	f = stdin;
	if(infile) {
		f = fopen(infile, "r");
		if( !f)
			err_open(infile);
	}
	for(ptr= &mesg[0]; fgets(ptr,&mesg[sizeof(mesg)]-ptr, f) != NULL
		; ptr += strlen(ptr));
	fclose(f);
	time(&tloc);
	timstr = ctime(&tloc);
	for(i=0;i<entries;i++) {
		if ( (p=(&utmp[i]))->ut_type == USER_PROCESS
		  /*
		   * Don't bother trying to send to a ftp user because
		   * there is no pty.
		   */
		   && strncmp(p->ut_line, "ftp", 3) )
			sendmes(p);
	}
	alarm(60);
	do {
		i = wait((int *)0);
	} while(i != -1 || errno != ECHILD);
	exit(0); /* NOTREACHED */
}

static void
sendmes(struct utmp *p)
{
	register i;
	register char *s;
	static char *device = NULL;
	struct stat dummystat;
	FILE *f = NULL;

	if(group)
		if(!chkgrp(p->ut_user))
			return;
	while((i=fork()) == -1) {
		alarm(60);
		wait((int *)0);
		alarm(0);
	}

	if(i)
		return;

	if( !device) {
		device = (char *)malloc(sizeof("/dev/")+sizeof(p->ut_line)+1);
		if( !device)
			err_nomem();
	}
	signal(SIGHUP, SIG_IGN);
	alarm(60);
	s = &device[0];
	sprintf(s,"/dev/%.*s", sizeof(p->ut_line), &p->ut_line[0]);
	if (strchr(p->ut_line, '/') != NULL) {
		/* don't allow someone to send to other than /dev devices */
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		    gettxt(_SGI_DMMX_wall_illdev,
			"Illegal device for %.-8s on %s"),
		    &p->ut_user[0], s);
		exit(1);
	}
#ifdef DEBUG
	f = fopen("wall.debug", "a");
#else
	/* don't create a file */
	if( stat(s,&dummystat) == 0 ) {
		cap_value_t cap_write_override[] = {CAP_DAC_WRITE,
						    CAP_MAC_WRITE};
		cap_t ocap = cap_acquire(2, cap_write_override);
		f = fopen(s, "w");
		cap_surrender(ocap);
	}
#endif
	if(f == NULL) {
		_sgi_nl_error(SGINL_SYSERR2, cmd_label,
		    gettxt(_SGI_DMMX_wall_cnsnd, "Cannot send to %.-8s on %s"),
		    &p->ut_user[0], s);
		exit(1);
	}
	_sgi_ffmtmsg(f, 0, cmd_label, MM_INFO,
	    gettxt(_SGI_DMMX_wall_mesg,
		"Broadcast Message from %s (%s) on %s %s"),
	    who, line, systm, timstr);
	if(group)
	    _sgi_ffmtmsg(f, 0, cmd_label, MM_INFO,
		gettxt(_SGI_DMMX_wall_mesggrp, "To group %s"),
		grpname);
#ifdef DEBUG
	fprintf(f,"DEBUG: To %.8s on %s\n", p->ut_user, s);
#endif
	fprintf(f, "%s\n", mesg);
	fclose(f);
	exit(0);
}

static void
readargs(int ac, char **av)
{
	register int i;

	for(i = 1; i < ac; i++) {
		if(equal(av[i], "-g")) {
			if(group) {
			    _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				gettxt(_SGI_DMMX_wall_only1grp,
				    "Only one group allowed"));
			    exit(1);
			}
			i++;
			if (i >= ac) {
			    _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				gettxt(_SGI_DMMX_wall_neddgrp,
				    "-g needs a group name"));
			    exit(1);
			}
			if((pgrp=getgrnam(grpname= av[i])) == NULL) {
			    _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				gettxt(_SGI_DMMX_unkngroupid,
				    "unknown group id: %s"),
				grpname);
			    exit(1);
			}
			endgrent();
			group++;
		}
		else
			infile = av[i];
	}
}
#define BLANK		' '
static int
chkgrp(char *name)
{
	register int i;
	register char *p;

	for(i = 0; pgrp->gr_mem[i] && pgrp->gr_mem[i][0]; i++) {
		for(p=name; *p && *p != BLANK; p++);
		*p = 0;
		if(equal(name, pgrp->gr_mem[i]))
			return(1);
	}

	return(0);
}

