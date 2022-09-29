/* @[$] mknod.c 1.0 frank@ceres.esd.sgi.com Oct 27 1992
 * mknod - make special file
 *
 * mknod name [ b ] [ c ] major minor
 *       name m   ( shared data )
 *       name p   ( named pipe )
 *       name s   ( semaphore )
 *
 * internationalized !
 */

#include	<limits.h>
#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/mkdev.h>
#include	<locale.h>
#include	<fmtmsg.h>
#include	<unistd.h>
#include	<errno.h>
#include	<sys/attributes.h>
#include	<sys/iograph.h>

#include	"../messages/uxsgicore/uxsgicore.h"

/* local defines */

#define	ACC	0666		/* permission bits */
#define	F_USER	0
#define	F_ROOT	1

/* externals */

extern	void _sgi_nl_usage(int, char *, char *, ...);
extern	void _sgi_nl_error(int, char *, char *, ...);
extern	void _sgi_ffmtmsg(FILE *, int, char *, int, char *, ...);

/* locals */

struct moption {
	char	mo_let;
	char	mo_flag;
	mode_t	mo_mode;
	dev_t	mo_arg;
};

static	struct moption mopt3[] = {
	{ 'p',	F_USER,	S_IFIFO,	0	},
	{ 'm',	F_USER,	S_IFNAM,	S_INSHD	},
	{ 's',	F_USER,	S_IFNAM,	S_INSEM	},
	{ 0,	0,	0,		0	}
};
static	struct moption mopt45[] = {
	{ 'b',	F_ROOT,	S_IFBLK,	0	},
	{ 'c',	F_ROOT,	S_IFCHR,	0	},
	{ 0,	0,	0,		0	}
};

static	char cmd_label[] = "UX:mknod";

uid_t the_uid;

/*
 * Usage error message
 */
static void
Usage()
{
	register char *spec;

	spec = gettxt(_SGI_DMMX_specfile, "name");
	_sgi_ffmtmsg(stderr, 0, cmd_label, MM_INFO,
	    "%s %s %s b|c major minor",
	    gettxt(_SGI_DMMX_Usage, "Usage:"),
	    cmd_label + 3,
	    spec);
	_sgi_ffmtmsg(stderr, 0, cmd_label, MM_INFO,
	    "%s %s %s b|c string",
	    gettxt(_SGI_DMMX_Usage, "Usage:"),
	    cmd_label + 3,
	    spec);
	_sgi_ffmtmsg(stderr, 0, cmd_label, MM_INFO,
	    "%s %s %s p|m|s",
	    gettxt(_SGI_DMMX_usagespc, "      "),
	    cmd_label + 3,
	    spec);
	exit(2);
}

/*
 * check for correct args
 */
static
struct moption *chkarg(mop, lp)
register struct moption *mop;
register char *lp;
{
	if(lp[1])
		Usage();
	for(; mop->mo_mode; mop++) {
		if(mop->mo_let != *lp)
			continue;		/* not found */
		if(mop->mo_flag && the_uid) {
			static chkeuid;	/* only do this if setuid, and need to */
				/* ugly, but want to preserve old chown semantics */
			if(!chkeuid) {
				the_uid = geteuid();
				chkeuid = 1;
			}
			if(the_uid) {
				_sgi_nl_error(0, cmd_label,
					gettxt(_SGI_DMMX_MustBeSU, "Must be super-user"));
				exit(2);
			}
		}
		return(mop);
	}
	Usage();
}

/*
 * check major or minor number
 * only decimal or octal input (see ABI test)
 */
long chkmajmin(s, max)
char *s;
int max;
{
	long ret = 0;
	char *final;

	ret = strtol(s, &final, 0);
	if((ret > max) || *final)
		return(-1);
	return(ret);
}

/*
 * Illegal number
 */
static void
IllNum(ts, numstr)
char *ts;
char *numstr;
{
	_sgi_nl_error(0, cmd_label,
	    gettxt(_SGI_DMMX_ILLNum, "%s - Illegal %s number"),
	    numstr,
	    ts);
}

/*
 * main function
 */
int
main(argc, argv)
int argc;
char **argv;
{
	register struct moption *mop;
	register int majno;
	register int minno;
	register int ret;
	int is_string_spec = 0;

	/*
	 * international environment
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	the_uid = getuid();

	/*
	 * check args
	 */
	switch(argc) {
	case 3 :
		mop = chkarg(mopt3, argv[2]);
		break;
	case 4 :
		mop = chkarg(mopt45, argv[2]);
		mop->mo_arg = STRING_SPEC_DEV;
		is_string_spec = 1;
		break;
	case 5 :
		mop = chkarg(mopt45, argv[2]);
		/*
		 * make b/c device file
		 */
		if((majno = chkmajmin(argv[3], MAXMAJ)) < 0) {
			IllNum("major", argv[3]);
			exit(2);
		}
		if((minno = chkmajmin(argv[4], MAXMIN)) < 0) {
			IllNum("minor", argv[4]);
			exit(2);
		}
		mop->mo_arg = makedev((major_t)majno, (minor_t)minno);
		break;
	default :
		_sgi_nl_error(0, cmd_label,
		    gettxt(_SGI_DMMX_bad_argc,
			"Incorrect number of arguments"));
		Usage();
	}

	/*
	 * make special file
	 */
	ret = mknod(argv[1], mop->mo_mode | ACC, mop->mo_arg);

	if ((ret == 0) && is_string_spec) {
		ret = attr_set(argv[1], _DEVNAME_ATTR, argv[3], strlen(argv[3])+1, ATTR_ROOT);
		if (ret < 0)
			unlink(argv[1]);
	}

	if (ret < 0) {
		_sgi_nl_error(1, cmd_label,
		    gettxt(_SGI_DMMX_Cnotmk_spec, "Cannot make spec file %s"),
		    argv[1]);
		exit(2);
	}

	/*
	 * chown() return deliberately ignored
	 * match old chown semantics, for which
	 * we always created devices as uid 0, since we checked only
	 * uid==0; this is apparently here primarily for fifos.
	 */
	chown(argv[1], the_uid, getgid());
	return(0);
}

