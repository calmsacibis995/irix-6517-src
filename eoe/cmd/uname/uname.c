/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)uname:uname.c	1.29.1.5"

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

/***************************************************************************
 * Command: uname
 * Inheritable Privileges: P_SYSOPS,P_DACREAD,P_DACWRITE
 *       Fixed Privileges: None
 *
 ***************************************************************************/

#include	<stdio.h>
#ifndef __sgi
#include	<priv.h>
#endif /* __sgi */
#include	<locale.h>
#include	<pfmt.h>
#include	<errno.h>
#include	<tzfile.h>
#include	"sys/utsname.h"
#include	"sys/systeminfo.h"

#if u3b2 || i386 || defined(__sgi)
#include        <string.h>
#include        "sys/types.h"
#include        "sys/fcntl.h"
#include        "sys/stat.h"

#endif

#ifdef i386
#include        "sys/sysi86.h"
#else
#ifndef __sgi
#include        "sys/sys3b.h"
#endif /* __sgi */
#endif

#ifdef __sgi
#include	<ctype.h>
#include	<sys/syssgi.h>
#endif


#if defined(_STYPES_LATER)

/*
 * This define is here solely to enable correct execution
 * until such time as we can compile this in ABI-compliant mode.
 */

static int
uname(buf)
struct utsname *buf;
{
	return nuname(buf);
}

#endif /* defined(_STYPES_LATER) */


#ifdef __sgi
/*
 * When DINA is defined, the output of 'uname -a' matches that of 
 * 'uname -snrvmpd'.  When it ISN'T defined, 'uname -a' matches
 * 'uname -snrvmp'.  '-d' in '-a'....ayep.
 */ 
/* #define DINA */

#define ONLY_DFLG	(dflg && !(sflg||nflg||rflg||vflg||mflg||pflg))
#define LINELEN		80

int dflg=0, aflg=0, Vflg=0;	/* alphaversion routine need invoking opts */
char outstr[(3*LINELEN)+1];	/* shouldn't need more than 1 line, really */

extern int version_info(long, char *, int);
static int old_anums(long, char *, char *, int);
static int do_alphaversion(char *);
#endif /* __sgi */

struct utsname  unstr, *un;

extern void exit();
extern int uname();
extern int optind;
extern char *optarg;


/*
 * Procedure:     main
 *
 * Restrictions:
 *                setlocale: none
 *                pfmt: none
 *                fopen: none
 *                strerror: none
 *                sys3b(2): none
*/

main(argc, argv)
char **argv;
int argc;
{
#if u3b2 || i386 || defined(__sgi)
        char *nodename;
        int Sflg=0;
#if defined(__sgi)	/* -d (decode alpha versionnum) and -V VERSIONNUM */
        char *optstring="asnrpvdmRS:V:";
	char *alphaversion;
	long vnum_in;	/* 10-digit version number to decode */
#else
        char *optstring="asnrpvmS:";
#endif /* __sgi */
#else
        char *optstring="asnrpvm";
#endif /* u3b2 || i386 || defined(__sgi) */
	int sflg=0, nflg=0, rflg=0, vflg=0, mflg=0, pflg=0, Rflg=0, errflg=0, optlet;
	char releasename[256];	/* -R */
	char procbuf[256];

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore.abi");
	(void)setlabel("UX:uname");

	umask(~(S_IRWXU|S_IRGRP|S_IROTH) & S_IAMB);
        un = &unstr;
        uname(un);

        while((optlet=getopt(argc, argv, optstring)) != EOF)
                switch(optlet) {
                case 'a':
                        sflg++; nflg++; rflg++; vflg++; mflg++; 
#if defined(__sgi) && defined(DINA)
			dflg++;	    /* display alphanum info in 'all' opt */
			/* if an error is encountered during alphanum
			 * interpretation portion of -a opt, suppress
			 * error msg (because -d wasn't explictly invoked)
			 */
			aflg++;	
#endif /* __sgi && DINA */
                        break;
                case 's':
                        sflg++;
                        break;
                case 'n':
                        nflg++;
                        break;
                case 'r':
                        rflg++;
                        break;
                case 'v':
                        vflg++;
                        break;
                case 'm':
                        mflg++;
                        break;
		case 'p':
			pflg++;
			break;
#if defined(__sgi)
		case 'd':
			dflg++;
			break;
		case 'R':
			Rflg++;	/* *not* set by -a */
			rflg++;	/* -R implies -r */
			break;
		case 'V':
			Vflg++;
			vnum_in = atol(optarg);
			break;
#endif /* __sgi */

#if u3b2 || i386 || defined(__sgi)
                case 'S':
                        Sflg++;
                        nodename = optarg;

#ifdef i386
			Createrc2file(nodename);
#endif

                        break;
#endif

                case '?':
                        errflg++;
                }

        if(errflg || (optind != argc))
                usage(errflg);

#if u3b2 || u3b15 || i386 || defined(__sgi)
        if((Sflg > 1) || 
#if defined(__sgi)
           (Sflg && (sflg || nflg || rflg || vflg || mflg || pflg || dflg)) ||
           (Vflg && (Sflg||sflg||nflg||rflg||vflg||mflg||pflg||dflg))) {
#else
           (Sflg && (sflg || nflg || rflg || vflg || mflg || pflg))) {
#endif
                usage(errflg);
        }

#if defined(__sgi)
	/* 
	 * if Vflg, user just wants us to decode the specified
	 * longint version number.  Check for this first.
	 */
	if (Vflg) {
		int outstrlen;

		outstrlen = version_info(vnum_in, outstr, sizeof(outstr)-1);
		if (outstrlen > 0) {
			(void) fprintf(stdout, "%s\n", outstr);
			exit(0);
		} else {
			(void) fprintf(stderr,
				"`%ld': invalid inst version-number\n",vnum_in);
			exit(1);
		}
	}
	if (Rflg) {
		if(syssgi(SGI_RELEASE_NAME, sizeof(releasename), releasename)) {
			perror("Unable to determined extended release name");
			*releasename = '\0';
		}
	}
#endif /* Vflg */

        /* If we're changing the system name */
        if(Sflg) {
		FILE *file;
		char curname[SYS_NMLN];
		int len = strlen(nodename);
		int curlen, i;
		
                /*
                 * The size of the node name must be less than SYS_NMLN.
                 */
                if(len > SYS_NMLN - 1) {
                        (void) pfmt(stderr, MM_ERROR,
			    ":730:Name must be <= %d letters\n",SYS_NMLN-1);
                        exit(1);
                }

		/*
		 * NOTE:
		 * The name of the system is stored in a file for use
		 * when booting because the non-volatile RAM on the
		 * porting base will not allow storage of the full
		 * internet standard nodename.
		 * If sufficient non-volatile RAM is available on
		 * the hardware, however, storing the name there would
		 * be preferable to storing it in a file.
		 */

		/* 
		 * Only modify the file if the name requested is
		 * different than the name currently stored.
		 * This will mainly be useful at boot time
		 * when 'uname -S' is called with the name stored 
		 * in the file as an argument, to change the
		 * name of the machine from the default to the
		 * stored name.  In this case only the string
		 * in the global utsname structure must be changed.
		 */

		if ((file = fopen("/etc/nodename", "r")) != NULL) {
			curlen = fread(curname, sizeof(char), SYS_NMLN, file);
			for (i = 0; i < curlen; i++) {
				if (curname[i] == '\n') {
					curname[i] = '\0';
					break;
				}
			}
			if (i == curlen) {
				curname[curlen] = '\0';
			}
			fclose(file);
		} else {
			curname[0] = '\0';
		}

		if (strcmp(curname, nodename) != 0) {
			if ((file = fopen("/etc/nodename", "w")) == NULL) {
				(void) pfmt(stderr, MM_ERROR,
					":148:Cannot create %s: %s\n",
					"/etc/nodename", strerror(errno));
				exit(1);
			} 

			if (fprintf(file, "%s\n", nodename) < 0) {
				(void) pfmt(stderr, MM_ERROR,
					":333:Write error in %s: %s\n",
					"/etc/nodename", strerror(errno));
				exit(1);
			}
			fclose(file);
		}		
		
                /* replace name in kernel data section */
#ifdef i386
                sysi86(SETNAME, nodename, 0);
#else
#ifdef __sgi
		if (sysinfo(SI_SET_HOSTNAME, nodename, SYS_NMLN) == -1) {
			pfmt(stderr, MM_ERROR, ":731:Sysinfo failed: %s\n",
				strerror(errno));
			exit(1);
		}
#else /* !__sgi */
                sys3b(SETNAME, nodename, 0);
#endif /* __sgi */
#endif
                exit(0);
        }
#endif
        /* "uname -s" is the default */
#if defined(__sgi)
        if( !(sflg || nflg || rflg || vflg || mflg || pflg || dflg))
#else
        if( !(sflg || nflg || rflg || vflg || mflg || pflg))
#endif
                sflg++;
        if(sflg)
                (void) fprintf(stdout, "%.*s", sizeof(un->sysname), un->sysname);
        if(nflg) {
                if(sflg) (void) putchar(' ');
                (void) fprintf(stdout, "%.*s", sizeof(un->nodename), un->nodename);
        }
        if(rflg) {
                if(sflg || nflg) (void) putchar(' ');
                (void) fprintf(stdout, "%.*s", sizeof(un->release), un->release);
        }
        if(Rflg && *releasename) {
                if(sflg || nflg || rflg) (void) putchar(' ');
                (void) fprintf(stdout, "%.*s", sizeof(releasename), releasename);
        }
        if(vflg) {
                if(sflg || nflg || rflg) (void) putchar(' ');
                (void) fprintf(stdout, "%.*s", sizeof(un->version), un->version);
        }
        if(mflg) {
                if(sflg || nflg || rflg || vflg) (void) putchar(' ');
                (void) fprintf(stdout, "%.*s", sizeof(un->machine), un->machine);
        }
	if (pflg) {
		if (sysinfo(SI_ARCHITECTURE, procbuf, sizeof(procbuf)) == -1) {
			pfmt(stderr, MM_ERROR, ":731:Sysinfo failed: %s\n",
				strerror(errno));
			exit(1);
		}
                if(sflg || nflg || rflg || vflg || mflg) (void) putchar(' ');
		(void) fprintf(stdout, "%.*s", strlen(procbuf), procbuf);
	}
#if defined(__sgi)
	if (!ONLY_DFLG)		/* suppress if invoked with -d opt alone */
	    (void) putchar('\n');
        if(dflg) {
		do_alphaversion(un->release);
        }
#endif
        exit(0);
}


/*
 * Procedure:     usage
 *
 * Restrictions:
 *                pfmt: none
*/

#if defined(__sgi)
static char usage_str[] =	\
    "uname [-snrvmpadR]\n\tuname [-S system name]\n\tuname [-V INSTVERSIONNUM]";
#else
#if u3b2 || i386
static char usage_str[] = "uname [-snrvmpa]\n\tuname [-S system name]";
#else	/* !__sgi and !(u3b2 or i386) */
static char usage_str[] = "uname [-snrvmpa]";
#endif /* u3b2 || i386 */
#endif /* __sgi */

usage(err)
int err;
{
	if (!err)
		pfmt(stderr, MM_ERROR, ":8:Incorrect usage\n");

        (void) pfmt(stderr, MM_ACTION, ":732:Usage:\n\t%s\n", usage_str);

        exit(1);
}

#ifdef i386
Createrc2file(nodename)
char *nodename;
{
	FILE *fp;
	
        /*
         * The size of the node name must be less than SYS_NMLN.
         */
	if (strlen(nodename) > (size_t) SYS_NMLN - 1  ) {
                (void) pfmt(stderr, MM_ERROR,
                      	":730:Name must be <= %d letters\n",SYS_NMLN-1);
               	exit(1);
	}

	if ((fp = fopen("/etc/rc2.d/S11uname", "w")) == NULL) {
		(void) pfmt(stderr, MM_ERROR,
			":148:Cannot create %s: %s\n",
			"/etc/rc2.d/S11uname", strerror(errno));
       		exit(1);
	} 

	fprintf(fp, "uname -S %s", nodename);
}
#endif /* i386 */



#if defined(__sgi)	/* alpha version # decoding and display routines */
/*
 * legitimate alpha versionnums 1) consist of exactly 10 digits, and
 * 2) if present, are always the last chars in the utsname.release string.
 * Use these two tests to determine whether this release has one
 * (alphas, betas, and patch releases do; MR'd releases don't).
 */
#define VSTR_LEN	10

/*
 * do_alphaversion provides the -d opt.  
 * 'release_str' is the string from the running system's utsname.release
 * field.  If present, the 10 digits comprising the release's alphaversion
 * are at the end of it. 
 */
static int
do_alphaversion(char *release_str)
{
	char *cp = release_str;
	int rstrlen, outstrlen, digitcnt=0;
	long version_num;	/* 10 digit release str, converted to binary */

	if (!release_str) {
		fprintf(stderr, "!!??do_alphaversion: NULL release_str!\n");
		exit(1);
	}
	if ((rstrlen = strlen(release_str)) <= 0) {
		fprintf(stderr, "!!??release_str len %d\n", rstrlen);
		exit(1);
	}
	
	/*
	 * extract trailing 10-digit alpha versionnum if found
	 */
	cp += rstrlen-1;	/* last char before the NULL */
	while (cp >= release_str && isdigit((int)*cp)) {
		digitcnt++;
		cp--;	
	}
	if (digitcnt == VSTR_LEN) {
		version_num = atol(++cp);
#if _VDEBUG
		fprintf(stdout,"\tdo_alphaversion: vnum %ld (r_str \"%s\")\n",
				version_num, release_str);
#endif
		outstrlen = version_info(version_num, outstr, sizeof(outstr)-1);
		if (outstrlen > 0) {
			(void) fprintf(stdout, "%s\n", outstr);
			return(outstrlen);
		}
		/* if < 0, outstrlen == neg of error msg len in outstr */
		if ( (outstrlen < 0) && (dflg && !aflg) ) {
			(void) fprintf(stderr, "%s\n", outstr);
			return(-2);
		}
	}
	/*
	 * if we're still here, 'release_str' didn't contain an 
	 * alphanumber, or it was invalid.
	 * Return an error, but only display an error message if the
	 * -d option was explicitly requested. 
	 */
	if (dflg && !aflg) {
		(void)fprintf(stderr,
		"uname:  missing or invalid inst version-number in \"%s\"\n",
			release_str);
	}
	return(-1);

} /* do_alphaversion */
#endif /* (__sgi) */



#if defined(__sgi)
/*
 * version_info() and old_anums() should move to a library,
 * along with version_to_date(), currently in version.c.
 * version_info() and version_to_date() decode and display
 * the 10-digit alpha version numbers used to identify and
 * track SGI software releases, and therefore need to keep
 * their display formats in sync.  version_info should match
 * showversionnum's output exactly, and the assumptions made
 * by version_to_date() in decoding and displaying its alphadate
 * information must be compatible with showversionnum's (e.g.
 * both should use the same timezone (currently PST) when
 * displaying Alpha creation-times.
 */

/*
 * All alpha version numbers are longints consisting of exactly
 * 10 characters when converted to their string representation.
 *   + normal release versionnum format is "MMMAAAAABT", where
 *	"MMM" is the major number, where 100 < MMM < 214; 
 *	"AAAAA" is the alpha number, where 00000 <= AAAAA <= 99999;
 *	"B" identifies the builder of the release:
 *		0 == developer, 1 == project, 2 == the release group.
 *	"T" is a source-tree identifier, and will only be nonzero if
 *		parallel development of the release is occurring.
 *   + The format of patch release version numbers is "RRRRRRRRPP":
 *	"RRRRRRRR" are the major and alpha numbers of an MR'd
 *		release (so "RRRRRRRR" == "MMMAAAAA", and
 *	"PP" is the patch number, where 30 <= PP <= 99.
 * 
 *	--> So the final 2 digits determine whether it is a patch
 *	    or an MR'd release.
 */

/*
 * all legal version numbers contain 10 digits when in their
 * string format
 */
#define VSTR_LEN	10

/*
 * patch #1 --> pfield value 30, patch 2 --> 31, etc., so 30 is
 * the smallest patchfield value possible for patch releases.
 */
#define MIN_PFIELD_VAL	(30)
#define ITSAPATCH(_p_field)	(_p_field >= MIN_PFIELD_VAL)
/*
 * convert patchfield values into patch numbers, adjusting
 * by (MINPATCH-1) to make patch numbering begin at 1
 */
#define PFIELD_TO_PNUM(_pfield,_pnum)	\
			(_pnum = ((_pfield) - (MIN_PFIELD_VAL-1)))
#define SECS_IN_8HRS	(60 * 60 * 8)

char *builders[] =	{ "developer", "project", "build group" };
char *daynames[] = 	{ "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
char *monames[] = 	{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
			  "Aug", "Sep", "Oct", "Nov", "Dec" };
int
version_info(long version_num, char *outbuf, int max_outbuf_len)
{
    static char version_str[(3*VSTR_LEN)+1];  /* big enough for VERY bad vnum */
    static char vstr_cpy[VSTR_LEN+1];
    static char date_str[80];
    char bname[32];
    const char* date_prefix = "101";
    char *obp = outbuf;
    const int   dprefix_len  = strlen(date_prefix);
    const int   date_length = 5;
    const int   second_delta  = 725846400;
    int vstr_len, ob_len;
    int patrelnum, patfieldval, patchnum=0;
    int major, alpha, builder, tree;
    int matchcnt;	/* matchcnt holds # of sprintf fields that matched */
    long  create_date;
    time_t new_date;
    struct tm* tm_new_date;

    if (!outbuf) {
	fprintf(stderr, "!!??version_info: NULL outbuf ptr\n");
	fflush(stderr);
	exit(-3);
    }
    outbuf[0] = '\0';

    /* convert from binary representation to string of digits. */
    vstr_len = sprintf(version_str, "%ld", version_num);
    if (vstr_len != VSTR_LEN) {
	ob_len = sprintf(obp, "invalid version # %ld (%d digits long)\n",
				version_num, vstr_len);
    	return(-ob_len);
    }
#if VADEBUG
    printf("version_str \"%s\" (len %d), version_num %ld\n", version_str,
	vstr_len, version_num);
#endif

    /*
     * versionnums below '101xxxxxxx' use the previous alphanumbering
     * scheme; check for this first
     */
    if (strncmp(version_str,date_prefix,dprefix_len) < 0) {
	ob_len = old_anums(version_num,version_str,outbuf,max_outbuf_len);
#if VADEBUG
	if (ob_len > 0)
	    printf("\tversion_info, vnum %ld: outstr \"%s\" (len %d)\n",
		version_num, outbuf, ob_len);
#endif
	return(ob_len);
    }

    /*
     * version string format is MMMAAAAABT or RRRRRRRRPP; same digits
     * and algos are used in deriving creation-date info.  Keep a good
     * copy of version number in string format (vstr_cpy gets scrogged).
     */
    strncpy(vstr_cpy, version_str, VSTR_LEN);	/* vstr_cpy = "MMMAAAAABT" */
    vstr_cpy[dprefix_len+date_length] = '\0';	/* = "MMMAAAAA" */
    create_date = atol(vstr_cpy+dprefix_len);	/* create_date <-- "AAAAA" */
    new_date = create_date*3600 + second_delta;
    /* 
     * display times in PST; convert from GMT by subtracting 8 hours
     * of seconds from the total in new_date before calling gmtime
     */
    new_date -= SECS_IN_8HRS;
    tm_new_date = gmtime(&new_date);

    /* date format: "Fri Oct 15 1500 PST 1993" */
    sprintf(date_str,"%s %s %.2d %.2d00 PST %4d",
	daynames[tm_new_date->tm_wday], monames[tm_new_date->tm_mon],
	tm_new_date->tm_mday, tm_new_date->tm_hour, tm_new_date->tm_year+TM_YEAR_BASE);
    /*
     * splitting the string into patch-format fields, extract the 8-digit
     * release number and 2-digit patchfield value.  Range-check the 
     * latter before converting it to the actual patch number.
     * > If the patchfield's value is >= 30, it's a patch release whose
     * number is (patchfield-29); else it's an MR'd release version number.
     * Both formats use the top 8 chars for major + alpha numbers (MMMAAAAA);
     * the bottom two digits are the patchfield value, or the release's 
     * builder (B) and the source tree (T) from which it was built.
     */
    if ((matchcnt=sscanf(version_str,"%8d%2d",&patrelnum,&patfieldval)) != 2) {
	ob_len = sprintf(obp,
		"!!patrelnum/patfieldval scanf matched %d fields, not 2\n",
		matchcnt);
	return(-ob_len);
    }
    if ((matchcnt = sscanf(version_str, "%3d%5d%1d%1d", &major, &alpha,
		&builder, &tree)) != 4) {
	ob_len = sprintf(obp,
		"!!??major/alpha/bld/tree scanf matched %d fields not 4\n",
		matchcnt);
	return(-ob_len);
    }

    if (ITSAPATCH(patfieldval)) {
	PFIELD_TO_PNUM(patfieldval,patchnum);
#if VADEBUG
	printf("\t`%ld', patch release:  patrel %d, patch %d (pfield %d)\n", 
	    version_num, patrelnum, patchnum, patfieldval);
#endif
	obp += sprintf(obp, "Patch %d based on release %d, %s (%ld)", 
		patchnum, major, date_str, version_num);
    } else {	/* it's a full-fledged release */
#if VADEBUG
	printf("\t`%ld', MR'd alpha: major %d, alpha %0.4d, bldr %d, tree %d\n",
		version_num, major, alpha, builder, tree);
#endif
	if (builder < 0 || builder >= 3)
	    sprintf(bname, "<invalid builder `%d'>", builder);
	else
	    sprintf(bname, "%s", builders[builder]);

	obp += sprintf(obp, "Release %d, %s, by %s ", major, date_str, bname);
	if (tree)
	    obp += sprintf(obp, "in tree %d (%ld)", tree, version_num);
	else
	    obp += sprintf(obp, "(%ld)", version_num);
    }

    return(strlen(outbuf));

} /* version_info */

/* 
 * old_anums interprets the specified versionnum using SGI's
 * previous alphanumbering method.  A number of recent releases
 * are special-cased in order to provide more info.
 * 'vnum' and 'versions_strp' are binary and character representations
 * of the same value; 'bufp' points to the array in which to return
 * a maximum of 'max_bstr_len' characters of alphanum information.
 */
static int
old_anums(long vnum, char *version_strp, char *bufp, int max_bstr_len)
{
    int matchcnt, bstrlen=0;
    int major, minor, maint, alpha;
    char *cp = bufp;

    /*
     * old alphanumbering scheme (beginning with most significant digit):
     *   2 digits for major number,
     *   2 digits for minor number, 
     *   2 digits for maintenance number,
     *   4 digits for alpha number.
     *   == 10 total.
     */

    if (!bufp) {
	fprintf(stderr, "!!ERROR, old_anums: NULL bufp (vnum %ld)\n", vnum);
	exit(1);
    }
    bufp[0] = '\0';

    if ((matchcnt = sscanf(version_strp, "%2d%2d%2d%4d", &major, &minor,
		&maint, &alpha)) != 4) {
	bstrlen = sprintf(bufp,
		"!!ERROR, old_anums: scanf matched %d fields not 4\n",matchcnt);
	return(-bstrlen);
    }
#if ADEBUG
    printf("\t\t>>> old_anums: major %d, minor %0.2d, maint %d, alpha %0.4d\n",
		major, minor, maint, alpha);
#endif

    switch(vnum) {
	case 1006000182:
	case 1006000219:
		cp += sprintf(bufp, "%.*s", max_bstr_len, "Release 4.0.1T, ");
		break;
	case 1006000406:
		cp += sprintf(bufp, "%.*s", max_bstr_len, "Release 4.0.1, ");
		break;
	case 1006000730:
	case 1006000731:
		cp += sprintf(bufp,"%.*s", max_bstr_len,
				"Release 4.0.5 (or 4.0.5A), ");
		break;
	case 1006000733:
		cp += sprintf(bufp,"%.*s", max_bstr_len, "Release 4.0.5C, ");
		break;
	case 1006002902: 
		cp += sprintf(bufp,"%.*s", max_bstr_len, "Release 4.0.5F, ");
		break;
	case 1006003073:
	case 1006003075: 
		cp += sprintf(bufp,"%.*s", max_bstr_len, "Release 4.0.5G, ");
		break;
	case 1006003209: 
		cp += sprintf(bufp,"%.*s", max_bstr_len,
				"Release 4.0.5IPR, ");
		break;
	case 1006003353:
	case 1006003360: 
		cp += sprintf(bufp,"%.*s", max_bstr_len, "Release 4.0.5H, ");
		break;
	case 1006003370: 
		cp += sprintf(bufp,"%.*s", max_bstr_len,
				"Release 4.0.5 Indigo Patch (4.0.5IOP), ");
		break;
	case 1006003375: 
		cp += sprintf(bufp,"%.*s", max_bstr_len, "Release 4.0.5J, ");
		break;
	case 1007000290: 
		cp += sprintf(bufp,"%.*s", max_bstr_len, "Release 5.0, ");
		break;
	case 1007000305: 
		cp += sprintf(bufp,"%.*s", max_bstr_len, "Release 5.0SNI, ");
		break;
	case 1007000450:
	case 1007000451: 
		cp += sprintf(bufp,"%.*s", max_bstr_len, "Release 5.0.1, ");
		break;
	case 1008000149:
	case 1008000150: 
		cp += sprintf(bufp,"%.*s", max_bstr_len, 
				"Release 5.1 (or 5.1_Indy), ");
		break;
	case 1008000205: 
		cp += sprintf(bufp,"%.*s", max_bstr_len, 
				"Release 5.1.1 (or 5.1.1_Indy), ");
		break;
	default:
#if VADEBUG
		printf("\t\t< old_anums, default (vnum %ld) >\n", vnum);
#endif
		break;
	}
	
	cp += sprintf(cp, "Major %d, minor %0.2d, ", major, minor);

	/* if 'maint' is nonzero this is a maintenance release */
	if (maint)
	    cp += sprintf(cp, "maintenance release %0.2d, ", maint);

	cp += sprintf(cp, "alpha %0.4d (%ld)", alpha, vnum);

	bstrlen = strlen(bufp);

#if ADEBUG
	printf("\t\told_anums; outstr \"%s\" (len %d)\n", bufp, bstrlen);
	fflush(stdout);
#endif

	return(bstrlen);

} /* old_anums */
#endif /* __sgi */

