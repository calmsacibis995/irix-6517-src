#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <proj.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <locale.h>
#include <unistd.h>
#include <fmtmsg.h>
#include <stdlib.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

/*
 * chproj - change project id associated with a given file.
 */
static void 	usage(void);
static prid_t	getprojid(struct passwd *pwent, char *s);
static void	err(char *s);
static void	chprojerr(char *file);
static void	change(char *file, prid_t prjid);

/* locals */

char	cmd_label[] = "UX:chproj";

static uid_t	uid;
static int 	fflag, hflag, rflag;
static int 	retval;

/*
 * print the usage message
 */
static void
usage()
{
	(void)_sgi_nl_usage(SGINL_USAGE, cmd_label,
			    "chproj [-Rfh] project  file ...");
	exit(-1);
}

/*
 * Get numeric project id, given the name.
 * Also do the validity checks.
 */
static prid_t
getprojid(struct passwd *pwent, char *s)
{
	prid_t		prjid;
	char 		*name;
	char 		prjname[MAXPROJNAMELEN];

	if( !*s) {
	    return (prid_t) -1;
	}
	for (name = s; *s && isdigit(*s); ++s);
	if (!*s) {
		prjid = atoi(name);
		if (pwent->pw_uid == 0)
			return (prjid);
		if (prjid >= 0) {
			(void) projname(prjid, prjname, MAXPROJNAMELEN);
			if (prjname[0]) {
				if (validateproj(pwent->pw_name, 
						 prjname) != prjid) {
					_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
						      "prid %s - Authorization "
						      "failure", name);
					prjid = -1;
			        }
				return (prjid);
			}
		}
		prjid = -1;

	} else if (pwent->pw_uid == 0) {
		prjid = projid(name);
	} else {
		prjid = validateproj(pwent->pw_name, name);
	}

	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		      "unknown project id: %s",
		      name);
	return (prjid);
	
}

/*
 * error handling
 */
static void
err(char *s)
{
	if(fflag)
	    return;
	_sgi_nl_error(SGINL_SYSERR2, cmd_label, "%s", s);
	retval = 2;
}

static void
chprojerr(char *file)
{
	if (fflag)
		return;
        err(file);
}

/*
 * set owner/group/project
 */
static void
change(char *file, prid_t prjid)
{
	register DIR *dirp;
	register dirent64_t *dp;
	struct stat64 buf;
	char savedir[PATH_MAX];
	
	if (hflag ? lstat64(file, &buf) : stat64(file, &buf)) {
		err(file);
		return;
	}
	
	if (rflag && S_ISDIR(buf.st_mode)) {
		if (getcwd(savedir, PATH_MAX) == (char *)0) {
			err(file);
			return;
		}
		if (chdir(file) < 0 || !(dirp = opendir("."))) {
			err(file);
			return;
		}
		for (dp = readdir64(dirp); dp; dp = readdir64(dirp)) {
			if (dp->d_name[0] == '.' && (!dp->d_name[1] ||
						     dp->d_name[1] == '.' && 
						     !dp->d_name[2]))
				continue;
			change(dp->d_name, prjid);
		}
		closedir(dirp);

		/* 
		 * Now go back to where we started and chown that
		 * as well.  This should work for ".", "..", and
		 * anything else.
		 */
		if(chdir(savedir) < 0) {
			err(savedir);
			exit(fflag? 0 : 1);
		}
		if (chproj(file, prjid))
		    chprojerr(file);
	} else {
		if (hflag) {
			if (lchproj(file, prjid))
				chprojerr(file);
		} else {
			if(chproj(file, prjid)) 
				chprojerr(file);
		}	
		return;
	}
}

/*
 * main entry
 */
int
main(int argc, char **argv)
{
	int 		ch;
	struct passwd   *pwent;
	prid_t		prjid;

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);


	rflag = fflag = hflag = retval = 0;
	while((ch = getopt(argc, argv, "Rfh")) != EOF)
	    switch((char)ch) {
		case 'R':
		    rflag++;
		    break;
		case 'f':
		    fflag++;
		    break;
		case 'h':
		    hflag++;
		    break;
		case '?':
		default:
		    usage();
	    }
	argv += optind;
	argc -= optind;

	if(argc < 2)
	    usage();

	uid = geteuid();

	/* Extract our passwd entry */
	if ((pwent = getpwuid(uid)) == NULL) {
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			      "unable to find passwd entry");
		exit(1);
	}
	endpwent();

	/* 
	 * Convert the project name to a numeric project ID. If the user
	 * is anyone other than root, make sure they are authorized. 
	 */
	prjid = getprojid(pwent, *argv);
	if (prjid < 0) 
		exit(1);
	
	if ((prjid & 0xffff) != prjid) {
		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			      "project id %lld is too large (> 16 bits)",
			      prjid);
		exit(1);
	}

	while (*++argv) 
		change(*argv, prjid);
	
	exit(retval);
}

