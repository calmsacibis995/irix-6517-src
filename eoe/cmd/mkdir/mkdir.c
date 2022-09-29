/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)mkdir:mkdir.c	1.7.8.14"

/***************************************************************************
 * Command: mkdir
 * Notes: mkdir makes a directory.
 * 	If -m is used with a valid mode, directories will be created in
 *	that mode.  Otherwise, the default mode will be 777 possibly
 *	altered by the process's file mode creation mask.
 *
 * 	If -p is used, make the directory as well as its non-existing
 *	parent directories. 
 ***************************************************************************/

#include	<signal.h>
#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<errno.h>
#include	<string.h>
#include        <deflt.h>
#include	<stdlib.h>
#include	<pwd.h>
#include	<unistd.h>
#include	<locale.h>
#include	<pfmt.h>

#define USER    0700   /* user's bits */
#define GROUP   0070   /* group's bits */
#define OTHER   0007   /* other's bits */
#define ALL     0777   /* all */

#define READ    0444   /* read permit */
#define WRITE   0222   /* write permit */
#define EXEC    0111   /* exec permit */

#define ERR_PERMS  9999

static mode_t parse_mode(), abs_val(), who(), perms();
static int what();

char *ms;

extern int opterr, optind;
extern char *optarg;

static const char
	badmkdir[] = ":328:Cannot create directory \"%s\": %s\n";

main(argc, argv)
char *argv[];
int argc;
{
	int 	pflag, mflag,  errflg;
	int 	c, local_errno, saverr = 0;
	mode_t	mode;
	int 	um_orig;
	char 	*p, *m;
	char 	*endptr;
	char *d;	/* the (path)name of the directory to be created */
	int err;
	void cleandir();

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore.abi");
	(void)setlabel("UX:mkdir");
	errno = 0; /* temporary soln., reset errno in case setlocale fails*/

	mode = S_IRWXU | S_IRWXG | S_IRWXO;
	pflag = mflag = errflg = 0;
	local_errno = 0;

	um_orig = umask(0);   /* save original umask and restore later */
	umask(um_orig);

	while ((c=getopt(argc, argv, "m:p")) != EOF) {
		switch (c) {
		case 'm':
			mflag++;
			ms = optarg;
			mode = parse_mode();
			break;
		case 'p':
			pflag++;
			break;
		case '?':
			errflg++;
			break;
		}
	}

	argc -= optind;
	if(argc < 1 || errflg) {
		if (!errflg)
			pfmt(stderr, MM_ERROR, ":8:Incorrect usage\n");
		pfmt(stderr, MM_ACTION, 
			"uxsgicore:87:Usage: mkdir [-m mode] [-p] dirname ...\n");
		exit(2);
	}
	argv = &argv[optind];

        while(argc--) {
		d = *argv++;
		/* Skip extra slashes at the end of path */
		while ((endptr=strrchr(d, '/')) != NULL){
			p=endptr;
			p++;
			if (*p == '\0')
				*endptr='\0';
			else
				break;
		}


		/* When -p is set, invokes mkdirp library routine.
		 * Although successfully invoked, mkdirp sets errno to ENOENT 
		 * if one of the directory in the pathname does not exist,
		 * thus creates a confusion on success/failure status 
		 * possibly checked by the calling routine or shell. 
		 * Therefore, errno is reset only when
		 * mkdirp has executed successfully, otherwise save
		 * in local_errno.
		 */ 
		if (pflag) { 
			mode_t  um_temp, fin_mode;
			struct stat64 stb;

			um_orig = umask(0);
			um_temp = um_orig & 0477;  /* u+wx */
			fin_mode = ALL & ~um_orig;
			umask(um_temp);
			err = stat64(d, &stb);
			if (err == 0 && S_ISDIR(stb.st_mode))
				;
			else if (err == 0) {
				local_errno = ENOTDIR;
				pfmt(stderr, MM_ERROR, badmkdir, d,
					strerror(ENOTDIR));
			} else {
				err = mkdirp(d,mode);
				if (err < 0) {
					pfmt(stderr, MM_ERROR, badmkdir, d,
						strerror(errno));
					local_errno = errno;
				} 
				if (mflag)
					err = chmod(d, mode);
				else	
					err = chmod(d, fin_mode);
				if (err < 0) {
					pfmt(stderr, MM_ERROR, badmkdir, d,
						strerror(errno));
					local_errno = errno;
				}
			}
			errno = 0;

		/* No -p. Make only one directory 
		 */

		} else {
			err = mkdir(d, mode);
			if (err) 
				pfmt(stderr, MM_ERROR, badmkdir, d,
					strerror(errno));
		}

		if (err) {
			saverr = 2;
			errno = 0;
		}

	} /* end while */

	if (saverr)
		errno = saverr;

	/* When pflag is set, the errno is saved in local_errno */
	if (errno == 0 && local_errno)
		errno = local_errno;

	umask(um_orig);  /* restore original umask value */

        exit(errno ? 2: 0);
	/* NOTREACHED */
} /* main() */


static mode_t
parse_mode() {

  mode_t m,b, mask, mode_r = 0777;
  register int o;
  int um;

  um = umask(0);
  if(*ms >= '0' && *ms <= '7')
        return abs_val(ms);

  mode_r &= ~um;

  do {
        if (! (m = who())) {
		pfmt(stderr, MM_ERROR, ":14:Invalid mode\n");
		exit(2);
	} 
        if (! (o = what())) {
                pfmt(stderr, MM_ERROR, ":14:Invalid mode\n");
                exit(2);
        }
        if ( (b = perms()) == ERR_PERMS) {
                pfmt(stderr, MM_ERROR, ":14:Invalid mode\n");
                exit(2);
        }
 
        mask = m & b;

        if(o == '=')
          mode_r = mask | (mode_r & ~m);
        else if(o == '+')
          mode_r |= mask;
        else if(o == '-')
          mode_r &= ~mask;
  } while(*ms++ == ',');

  return mode_r;
}


static mode_t
who() {
  register mode_t m;

  m=0;
  for(;; ms++)
    switch(*ms) {
        case 'u':
          m |= USER;
          break;
        case 'g':
          m |= GROUP;
          break;
        case 'o':
          m |= OTHER;
          break;
	case 'a':
	  m = ALL;
	  break;
        case '+':
        case '=':
        case '-':
          if(m == 0)
              m = ALL;
          return m;
        default:
          return 0;  /* error */
      }
}


static int
what()
{
        switch (*ms) {
        case '+':
        case '-':
        case '=':
                return *ms++;
        }
        return(0);
}

static mode_t
perms() {
  register mode_t m;

  m=0;
  for(;; ms++)
    switch(*ms) {
        case 'r':
          m |= READ;
          break;
        case 'w':
          m |= WRITE;
          break;
        case 'x':
          m |= EXEC;
          break;
        default:
        /*  if (m == 0)
             return 0;   dont change anything */
          if ((*ms == '\0') || (*ms == ','))
             return m;
          return ERR_PERMS;
      }
}


static mode_t
abs_val()
{
        register c;
        mode_t i;

        for ( i = 0; (c = *ms) >= '0' && c <= '7'; ms++)
                i = (mode_t)((i << 3) + (c - '0'));
        if (*ms) {
                pfmt(stderr, MM_ERROR, ":14:Invalid mode\n");
                exit(2);
	}
        return i;
}

