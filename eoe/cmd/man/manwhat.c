/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1989, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/* build the man whatis database the same way that man -W does, but
 * *much* faster (about 1/4 - 1/6 the time, because it only runs
 * children on unformatted or compressed (rather than packed) man 
 * pages.  Heavily hacked from man.c, 3/95.  Dave Olson
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <libgen.h>
#include <locale.h>
#include <setjmp.h>
#include "path.h"

/*
//  Routines
*/
extern int	main(int argc, char * argv[]);
static int	parsePaths(char * pathstr, char * patharray[]);
static void	lookdirs(char * manpaths[]);
static int	findman(char * rootpath);
static void	launch(char * name);
static int filetype(char * file);
static int unpack(char *src, int srccnt, char *dest, int destcnt);
extern int ungzip(char *src, int srccnt, char *dest, int destcnt);
extern int uncompress(char *src, int srccnt, char *dest, int destcnt);
static void getcatNAME(char *buf, int len);
static void gethtmlNAME(char *buf, int len);

#define	DEFAULTMANPATHS "/usr/share/catman:/usr/share/man:/usr/catman:/usr/man"


#define DEFAULTNROFFCMD "nroff"

#define	FBSZ		(4096)

int fsize;	/* current size of filebuf */
unsigned fmem;	/* current direct io mem requirement; can be
	different for different filesystems */
int pagesize;	/* system page size; that's our mininum alignment */
char *filebuf; /* have to allocate dynamically because xfs block
		sizes can be large */

static char *tbuf;
static int fbsz;
static struct stat statbuf;

/* variables associated with getcatNAME */
static char *usrc, *usend, *udest, *udend;
static jmp_buf packjmp;
static int inleft;

/* variables associated with unpack */
#define US	037
#define RS	036
static char	*uninp;
/* the dictionary */
static long	origsize;
static short	maxlev;
static short	intnodes[25];
static char	*tree[25];
static char	characters[256];
static char	*eof;
static int	Tree[1024];
static void getdict(void);
static unsigned char ugetch(void);
static unsigned short getword(void);
static void expand(void);
static void decode(void);


static char * macropackage = "/usr/lib/tmac/tmac.an";
static char * filtercmd;
static char * nroffcmd;
static char * manfmtcmd;
static char * awfcmd = "awf -man";

#define	GUESS	0
#define	CAT	1
#define	MAN	2

#define	BUFSIZE 4096
#define MANDIRCNT 200	/* 1/2 this is max dirs in manpath */

#define	USAGE "Usage:  manwhatis [-W] [-M manpath]\n"

#define	PACKMAGIC	017436
#define	COMPRESSMAGIC	0x1f9d
#define	GZIPMAGIC	0x1f8b

#define NOTREGULARFILE  0
#define	PACKFILE	1
#define	COMPRESSFILE	2
#define	NROFFFILE	3
#define	CATFILE		4
#define	GZIPFILE	5
#define	GZIPHTML	6

#define GZIPFILEOFFSET	10

extern int
main(int argc, char * argv[])
{
    char *		manpaths[MANDIRCNT];
    char *	envmp;
    int 	o;
    extern char *	optarg;

    (void)setlocale(LC_ALL, "");

    /* Set up defaults search paths */
    (void)parsePaths(DEFAULTMANPATHS, manpaths);

    if ((envmp = getenv("MANPATH")) != NULL) {
	    if (parsePaths(envmp, manpaths) == 0)
		    (void)parsePaths(DEFAULTMANPATHS, manpaths);
    }

    nroffcmd = getenv("NROFF");
    if (nroffcmd == 0) nroffcmd = DEFAULTNROFFCMD;

	/* if set, this is the *full* command we us to format unformatted
	 * man pages.  Otherwise we have to add tbl, eqn, and a way to
	 * specify arguments to nroff, etc.  For exampe, this allows
	 * us to use things Henry Spencer's "awf" simply.
	*/
    manfmtcmd = getenv("MANFMTCMD");

	/* only used for compressed man pages, and unformatted man pages */
	    filtercmd = "/usr/lib/getcatNAME";

	while ((o = getopt(argc, argv, "M:W")) != -1) {
		switch (o) {
		case 'W':	/* accept and ignore the man -W flag */
			break;
		case 'M':	/* man path */
			if (parsePaths(optarg, manpaths) == 0)
			    (void)parsePaths(DEFAULTMANPATHS, manpaths);
			break;
		case '?':
			(void) fprintf(stderr, USAGE); 
			exit(2);
		}
	}
    
	/* align on a page boundary, for the last tweak of i/o
	 * performance, but allocate only 4K regardless of page size
	 * since we should never need to read more than that to get
	 * the useful info.  O_DIRECT requirements may need more than
	 * the pagesize, because xfs blocksizes can be large, but we
	 * will always operate on no more than FBSZ in any case.
	*/
	pagesize = getpagesize();
	tbuf = (char *)memalign(pagesize, FBSZ);
	if(!tbuf) {
		perror("manwhat: Can't allocate memory for buffers");
		exit(1);
	}

	lookdirs(manpaths);

	fflush(stdout);

	return(0);	/* exit */
}

static void
lookdirs(char * manpaths[])
{
    struct dirent *	direntptr;
    DIR *	dirptr;
	char **mp;
	char dname[PATH_MAX];

    for (mp=manpaths; *mp; mp++) {
		if ((dirptr = opendir(*mp)) == NULL)
			continue;
		while ((direntptr = readdir(dirptr)) != NULL) {
			char *nm = direntptr->d_name;
			switch(*nm) {
			case 'u':
			case 'a':
			case 'p':
			case 'g':
				if(strcmp(nm+1, "_man"))
					continue;
				break;
			case 'l':
				if(strcmp(nm+1, "ocal"))
					continue;
				break;
			case 'm':
				/* manX, but not manXy* */
				if(strncmp(nm+1, "an", 2) || nm[4])
					continue;
				break;
			case 'c':
				/* catX, but not catXy* */
				if(strncmp(nm+1, "at", 2) || nm[4])
					continue;
				break;
			default:
				continue;
			}
			sprintf(dname,"%s/%s", *mp, nm);
			(void)findman(dname);
		}
		closedir(dirptr);
	}
}

static int
parsePaths(char * pathstr, char * patharray[])
{
    int	count;
    char *	ps;
    int  		addpath;
    char *		langname = setlocale(LC_MESSAGES, NULL);
   
    if ((langname == NULL) || !strcmp(langname, "C"))
        addpath = 0;
    else
        addpath = 1;
 
    count = 0;
    ps = pathstr;
    while(count < MANDIRCNT/2 && (patharray[count] = strtok(ps, ":"))) {
        if (addpath) {
            patharray[count + 1] = patharray[count];
            patharray[count] = malloc(strlen(patharray[count + 1]) + 
			       strlen(langname) + 2);
            sprintf(patharray[count], "%s/%s", patharray[count + 1], langname);
	    count++;
        }
	count++;
	if(count >= MANDIRCNT/2)
		fprintf(stderr, "only %d dirs handled in MANPATH, rest ignored\n",
			MANDIRCNT/2);
	ps = NULL;
    }
    return(count);
}

static int
findman(char * rootpath)
{
    DIR *	dirptr;
    struct dirent *	direntptr;
	char name[PATH_MAX+NAME_MAX+2], *namep;
    
	if ((dirptr = opendir(rootpath)) == NULL)
		return (-1);

	strcpy(name, rootpath);
	namep = name + strlen(name);
	*namep++ = '/';

	while ((direntptr = readdir(dirptr)) != NULL) {
		char *rcscheck;

		strcpy(namep, direntptr->d_name);
		if(!stat(name, &statbuf)) {
			if(S_ISREG(statbuf.st_mode)) {
				/* Ignore RCS files (...,v) */
				rcscheck = strrchr(namep, ',');
				if(rcscheck && strcmp(rcscheck, ",v"))
					rcscheck = NULL;
				if(*namep != '.' && rcscheck == NULL)
					launch(name);
			}
			else if(S_ISDIR(statbuf.st_mode) &&
				*namep != '.' && strcmp(namep, "RCS")) {
				/* Avoid RCS directories to speed searches against man page
				   source trees.  Also skip all . dirs, not just . and .. */
				(void)findman(name);
			}
		}
	}
    closedir(dirptr);
	return 0;
}


/* fork and exec to see if cmd exists in search path; designed to
 * work only with cmds that work as filters and work with no args
*/
static int
forkandexeclp(char *cmd)
{
    int pid, wpid, i;
    wait_t status;
    char *args[11];

    if((pid=fork()) == -1)
	return 0;
    if(pid) {
	while((wpid = waitpid(pid, &status.w_status, 0)) != pid && wpid != -1)
		;
	if(wpid != pid)
	    return 0;	/* something's weird, act old way */
	if(status.w_termsig)
	     return 0;	/* strange; possibly resources; act old way */
	if((status.w_retcode & 0xff) == 0xff)
	     return -1;	/* it isn't there */
	/* any non-ff code take as success */
	return 1;
    }
    /* close fd 0; all of the cmds we are interested in work as
     * filters, reading from stdin, and work with no args;
     * close 1 also, in case, like *eqn output is generated immediately;
     * close 2 in case it generates usage messages.
     * it would be nice to use popen to handle strings with args,
     * but it is too hard to tell if exec fails; can't just use
     * first part of cmd, becauase cmds like psroff will happily
     * generate empty print jobs...; if more than 10 args, assume
     * they know what they are doing in specifying their old
     * filter and bomb out also */
    args[0] = strtok(cmd, " ");
    for(i=1; i<11 && (args[i]=strtok(NULL, " ")); i++)
	;
    if(i == 11)
	exit(0);

    close(0);
    close(1);
    close(2);
    execvp(args[0], args);
    exit(0xff);
	/*NOTREACHED*/
}


/* check to see if the formatting commands exist; also check for
 * tbl, and the appropriate *eqn command.  Too many customers just
 * won't read the man page...  I'm tired of seeing and answering this
 * question, as are most of support, I suspect.
 * return 1 on success, -1 on failure; 0 if fork fails, so we try
 * again on later man pages, if any; this essentially means that
 * the older style message about formatters "not found" message will
 * be printed.  Better than bogusly not trying and printing incorrect
 * no DWB message.  Normally called only once per roff cmd (i.e.,
 * once for nroff per invocation of the man cmd
*/
static
chkexist(char *roffcmd)
{
    int ret;
    if((ret = forkandexeclp(roffcmd)) <= 0)
	return ret;
    return 1;
}


static void
launch(char * name)
{
    unsigned	ft;
    char cmd[ARG_MAX];
    int skipit = 0, usize;

    if ((ft = filetype(name)) == -1) return;

    switch (ft) {
    case CATFILE:
		getcatNAME(filebuf, fbsz);
		return;
    case PACKFILE:
		usize = unpack(filebuf, fbsz, tbuf, FBSZ);
		getcatNAME(tbuf, usize);
		return;
    case COMPRESSFILE:
		usize = uncompress(filebuf, fbsz, tbuf, FBSZ);
		if (usize > 0) {
			getcatNAME(tbuf, usize);
			return;
		} else {
			/* unknown compress method, corrupt file, etc. */
			(void) sprintf(cmd, "zcat %s ", name);
			strcat(cmd, "| ");
			strcat(cmd, filtercmd);
		}
		break;
    case GZIPFILE:
    case GZIPHTML:
		usize = ungzip(filebuf, fbsz, tbuf, FBSZ);
		if (usize > 0) {
		if (ft == GZIPHTML)
			gethtmlNAME(tbuf, usize);
		else 
			getcatNAME(tbuf, usize);
			return;
		} else {
			/* unknown gzip method, corrupt file, etc. */
			(void) sprintf(cmd, "gzip -dc %s ", name);
			strcat(cmd, "| ");
			strcat(cmd, filtercmd);
		}
		break;
    case NROFFFILE:
	{
	    static chknfmtexist;
awfit:
		if(manfmtcmd) {
			(void)sprintf(cmd, "%s %s", manfmtcmd, name);
			strcat(cmd, "| ");
			strcat(cmd, filtercmd);
		}
		else {
			 if(!chknfmtexist) {
				if((chknfmtexist = chkexist(nroffcmd)) == -1) {
					/* if no nroff, look for awf, and if it's there,
					 * use it for this and the other unformatted man pages.
					 * awf is Henry Spencer's awk based formatter */
					if((chknfmtexist = forkandexeclp(awfcmd)) != -1) {
						fprintf(stderr,"Standard text formatter package not found,"
						 " using the \"awf\" formatter\n");
						manfmtcmd = awfcmd;
						goto awfit;
					}
				}
			}
			if(chknfmtexist == -1)
				skipit = 1;
			(void)sprintf(cmd, "tbl -TX %s | neqn /usr/pub/eqnchar - | %s -i %s ",
				name, nroffcmd, macropackage);
			strcat(cmd, "| ");
			strcat(cmd, filtercmd);
		}
	}
	break;
    case NOTREGULARFILE:
    default:
	fprintf(stderr, "File %s is not a printable man page.\n", name);
	return;	 /* No reason to launch if we can't handle it */
    }

    if (ft == NROFFFILE) {
	static nodwb = 0;
	if(skipit) {
	    if(!nodwb) {
		nodwb = 1;
		fprintf(stderr,
		  "The text formatters necessary to format this man page\n"
		  "do not appear to be installed or in your PATH.  You must\n"
		  "install the \"Documenter's Workbench\" (dwb) product, or the\n"
		  "equivalent in order to display unformatted man pages\n");
	    }
	    fprintf(stderr, "Skipping %s.\n", name);
	    return;
	}
    }
    fflush(stdout);	/* flush our buffer before running pipeline */
    (void) system(cmd);
}


static int
filetype(char * file)
{
    int	ifd;
    unsigned int ft;
    unsigned int i;
    struct dioattr attr;

    /* use O_DIRECT, since we won't ever look at the data more than
     * once, and it saves the data copy by the kernel; if we get EINVAL,
     * try again without O_DIRECT, to handle nfs mounted man pages, before
     * complaining. */
    ifd = open(file, O_RDONLY|O_DIRECT);
    if (ifd == -1) {
	if (errno == EINVAL)
	    ifd = open(file, O_RDONLY);
	if (ifd == -1) {
	    perror(file);
	    return(-1);
	}
	if(!fsize) { /* first manpage, and direct i/o not supported on
	    * this man page (probably over nfs) */
	    fsize = FBSZ;
	    if(!(filebuf = memalign(pagesize, fsize))) {
		perror("manwhat: Can't allocate memory for file buffer");
		exit(1);
	    }
	}
    }
    else if(fcntl(ifd, F_DIOINFO, &attr) == 0) {
	/* for most systems, we'll do this only once... */
	if(attr.d_miniosz > fsize || attr.d_mem > fmem) {
	    if(filebuf)
		free(filebuf);
	    fsize = attr.d_miniosz > FBSZ ? attr.d_miniosz : FBSZ;
	    /* page align, at least */
	    fmem = pagesize > attr.d_mem ? pagesize : attr.d_mem;
	    if(!(filebuf = memalign(fmem, fsize))) {
		perror("manwhat: Can't allocate memory for file buffer");
		exit(1);
	    }
	}
    }

    /* we stat'ed file in findman() just before calling launch(),
     * which immediately calls filetype; saves a stat followed by
     * fstat */
    if (statbuf.st_mode & S_IFREG) {
	ft = NOTREGULARFILE;

	fbsz = read(ifd, filebuf, fsize);
	if (fbsz > 3) {
	    short magic = *(short *)filebuf;
	    if(fbsz > FBSZ)
		fbsz = FBSZ; /* never more than first FBSZ bytes */
	    if (magic == PACKMAGIC) {
		ft = PACKFILE;
	    } else
	    if (magic == COMPRESSMAGIC) {
		ft = COMPRESSFILE;
	    } else
	    if (magic == GZIPMAGIC) {
		if(strstr(filebuf+GZIPFILEOFFSET,".html") != NULL) 
		    ft = GZIPHTML;
		else
		    ft = GZIPFILE;
	    } else {
		/*
		** NROFF filetyping heuristic -- look for a line beginning
		** with what may be an NROFF command (regexp "^[.'][A-Za-z'\\]")
		*/
		for (i=0; i<fbsz; i++) {
		    if ((i == 0 || filebuf[i-1] == '\n')
			&& ((filebuf[i] == '.') || (filebuf[i] == '\''))
			&& (isalpha(filebuf[i+1]) ||
			    (filebuf[i+1] == '\\') ||
			    (filebuf[i+1] == '\'')
			)
		    ) {
			ft = NROFFFILE;
			break;
		    }
		}
	    }
	    if (ft == NOTREGULARFILE) {
		/*
		** If we haven't validated any other file type, try validating
		** an ASCII formatted CATFILE by making sure all of the
		** characters are printable.  This will reduce the chance that
		** things like executables get printed.
		*/
		int printable = 1;
		for (i=0; i<fbsz && printable; i++) {
		    printable = isprint(filebuf[i])
				|| isspace(filebuf[i])
				|| filebuf[i] == '\b';
		}
		if (printable)
		    ft = CATFILE;
	    }
	}
    }
    else
	ft = NOTREGULARFILE;
    (void) close(ifd);
    return(ft);
}

#define	BACKSPACE	0x08
#define TAB	'\t'
#define SPACE	' '
#define NEWLINE	'\n'

#define FMTWIDTH 23

static
match(register char* str1, register char* str2)
{
    for (; *str1 == *str2 && *str1 != '\0'; str1++, str2++) ;
    return (*str1 == '\0');
}
    
/* this is essentially the guts of the getcatNAME program, but
 * avoids recompiling regexp's, and already has the data in a buffer
 * (and of course, avoids the fork and exec!)
*/
static void
getcatNAME(char *buf, int len)
{
    char in[4096];
    char sectionNumberStr[256]; 
    static char *sectionNumberPattern = 0;
    char *strp, *inp, *endp;
    char last;
    unsigned int spaces;
    unsigned int havesection = 0;
    unsigned int gotname = 0;
    unsigned int pastfirstlineofname = 0;
    unsigned int printedsec = 0;

    sectionNumberStr[0] = '\0';

	if(!sectionNumberPattern)
		/* section number is '(' [0-9l][0-9a-zA-Z+]* ')' */
		sectionNumberPattern = regcmp("(\\([0-9lD][+0-9a-zA-Z]*\\))$0", (char*)0);

	endp = &buf[len-1];

	while (buf < endp) {
		strp = buf;
		while (buf < endp && *buf != '\n')
			buf++;
		*buf = '\0';
		buf++;

		/*
		**  Read input stream.  Change tabs, multiple spaces, and newlines
		**  to single character spaces.  Backspaces act as real backspaces
		**  in the "in" buffer.
		*/
		for (inp=in, last=NEWLINE, spaces=0; *strp; strp++) {
			if (*strp == TAB) *strp = SPACE;
			if ((*strp == SPACE) && (last == NEWLINE)) {
			if (pastfirstlineofname) {
				*strp = SPACE;
			} else {
				continue;
			}
			}
			if (*strp == SPACE) spaces++; else spaces = 0;
			if (spaces > 1) continue;

			if (*strp == BACKSPACE && inp != in) {
			inp--;
			continue;
			}

			*inp++ = *strp;
			last = *strp;
		}
		*inp = '\0';


		/*
		**  Parse section number from header.  Looking for:
		**
		** 	'(' [0-9l][0-9a-zA-Z+]* ')'
		**
		**  This pattern handles diverse section numbers such as:
		**
		**	(l)
		**	(3X11)
		**	(1EXP)
		**	(3C++)
		**
		**  If have already gotten the NAME section, don't mistakenly
		**  extract bogus section number information.
		**
		**  Some manual page headers are completely busted and look like:
		**
		**	foo(Silicon Graphics foo(3X)
		** 
		**  or
		**	foo(3X(tentative)(3X)
		**
		**  Since there is little chance we can get all of these fixed, and
		**  since this is a common formatting error, we'll for section numbers
		**  which have a more specific format than just '(' .* ')'.
		*/
		if (!gotname && !havesection) {
			(void) regex(sectionNumberPattern, in, sectionNumberStr);
			if (sectionNumberStr[0] != '\0') {
			havesection = 1;	/* section number is valid */
			continue;		/* done parsing, get next line */
			}
			/* Valid section not gotten so try matching other patterns */
		}


		/*
		**  Begin parsing NAME section of manual page.
		**  Clearcase manual pages use SUBCOMMAND NAME instead.
		*/
		if (match("NAME", in) || match("SUBCOMMAND NAME", in)) {
			gotname = 1;
			continue;	/* done parsing, get next line */
		}


		/*
		**  Terminate parsing of NAME section.
		**
		**  The name section may be multiple lines of text. We stop parsing
		**  NAME section text when we hit a blank line or if we run into any
		**  commonly known section names which include:
		**
		**  	SYNTAX, SYNOPSIS, DESCRIPTION, C SYNOPSIS, FORTRAN SYNOPSIS,
		**	C SPECIFICATION, FORTRAN SPECIFICATION
		*/
		if (in[0] == '\0') {
			if (pastfirstlineofname) {
			break;
			} else {
			continue;  /* name section is multiple lines, get next line */
			}
		}
		if (match("SYNTAX", in)) break;
		if (match("SYNOPSIS", in)) break;
		if (match("DESCRIPTION", in)) break;
		strp = strchr(in, ' ');
		if (strp != NULL && match("SYNOPSIS", strp+1)) break;	
		if (strp != NULL && match("SPECIFICATION", strp+1)) break;	


		/*
		**  Output the NAME section of the manual in whatis format.
		*/
		if (gotname) {
			unsigned int i;
			unsigned int fmtcount = 0;

			for (i = 0; in[i] != '\0'; i++) {
			if (in[i] == ' ' && in[i+1] == '-' &&
				(in[i+2] == ' ' || in[i+2] == '\0')) {
				putc(' ', stdout);
				fputs(sectionNumberStr, stdout);
				printedsec = 1;
				if (!pastfirstlineofname) {
				fmtcount += 1 + strlen(sectionNumberStr);
				for (; fmtcount < FMTWIDTH; fmtcount++) {
					putc(' ', stdout);
				}
				}
				break;
			}

			putc(in[i], stdout);
			fmtcount++;
			}
			fputs(&(in[i]), stdout);
			pastfirstlineofname = 1;
		}
	}

    if (!printedsec) {
	putc(' ', stdout);
	fputs(sectionNumberStr, stdout);
    }
    putc('\n', stdout);
}



/* Output a HTML TITLE string in apropos format */
void outputAproposFmt(char* str)
{
    int i=0;
    int k=0;
 
 /* 
 *  -Should enforce a space before the (1M)?
 *  -HTML entity translation
 */
 
    while(str[i] != NULL){
	
	if((str[i] == '-') && (i <= FMTWIDTH)){
	    k = FMTWIDTH-i+1;
	    while(k > 0){
		putchar(SPACE);
		k--;
	    }    
	}
	putchar(str[i]);
	i++;
    }
    putchar(NEWLINE);
    
}

/* 
 * This is a equivelent to getcatNAME,  but is used specifically with 
 * HTML man pages.  All the information we need is in the TITLE tag.
 */
static void
gethtmlNAME(char *buf, int len)
{
    char in[4096];
    char matchTag[32];
    char matchStr[4096];
    char titleStr[4096];
    char titleLine[4096];
    static char *bTitlePattern = 0;
    static char *eTitlePattern = 0;
    char *strp, *inp, *endp;
    char last;
    unsigned int inhtmltitle = 0;
    unsigned int havehtmltitle = 0;
    unsigned int spaces;
    unsigned int pastfirstlineofname = 0;

    if(!bTitlePattern)
	bTitlePattern = regcmp("(<[Tt][Ii][Tt][Ll][Ee]>)$0([^\n]*)$1", (char*)0);

    if(!eTitlePattern)
	eTitlePattern = regcmp("([^<]*)$0(</[Tt][Ii][Tt][Ll][Ee]>)$1", (char*)0);

    matchStr[0]	    = NULL;
    titleStr[0]	    = NULL;
    titleLine[0]    = NULL;

    endp = &buf[len-1];

    while (buf < endp) {
	strp = buf;
	while (buf < endp && *buf != '\n')
	    buf++;
	*buf = '\0';
	buf++;

	/*
	**  Read input stream.  Change tabs, multiple spaces, and newlines
	**  to single character spaces.  Backspaces act as real backspaces
	**  in the "in" buffer.
	*/
	
	for (inp=in, last=NEWLINE, spaces=0; *strp; strp++) {
	    if (*strp == TAB) *strp = SPACE;
	    if ((*strp == SPACE) && (last == NEWLINE)) {
		if (pastfirstlineofname) {
		    *strp = SPACE;
		} else {
		    continue;
		}
	    }
	    if (*strp == SPACE) spaces++; 
	    else spaces = 0;
	    if (spaces > 1) continue;

	    if (*strp == BACKSPACE && inp != in) {
		inp--;
		continue;
	    }

	    *inp++ = *strp;
	    last = *strp;
	}
	*inp = '\0';


	/* Looking for all the possible cases:
	 * 
	 *	<TITLE>A one line Title</TITLE> (One line)
	 *	
	 *	<Title>
	 *	A one line Title
	 *	</Title>
	 *	
	 *	<Title>This is a 
	 *	two line title</Title>
	 * 
	 */

	if(!havehtmltitle){
	    if(!inhtmltitle){
		matchTag[0] = NULL;
		(void) regex(bTitlePattern, in, matchTag, matchStr );
		if (matchTag[0] != NULL){
		    matchTag[0] = NULL;
		    (void) regex(eTitlePattern, matchStr, titleLine, matchTag); /* memory? */
		    if(matchTag[0] != NULL){
			havehtmltitle = 1;
			outputAproposFmt(titleLine);
			return;
		    } else{
			inhtmltitle = 1;
			strcat(titleStr, matchStr);
		    }
		}
	    } else{
		matchTag[0] = NULL;
		(void) regex(eTitlePattern, in, titleLine, matchTag);
		if(matchTag[0] != NULL){
		    if(titleLine[0] != NULL){
			strncat(titleStr, " ", 1);
			strcat(titleStr, titleLine);
		    }
		    havehtmltitle = 1;
		    outputAproposFmt(titleStr);
		    return;
		} else{ /* Take the whole line */
		    strncat(titleStr, " ", 1);
		    strcat(titleStr, in);
		}
	    }
	}

    }
}


static unsigned char
ugetch(void)
{
	if(usrc < usend)
		return *usrc++;
	longjmp(packjmp, 1);
	/*NOTREACHED*/
}

/* these hacks and globals are to minimize the effort of porting
 * unpack into inline code. */
static unsigned short
getword(void)
{
	unsigned char c;
	unsigned short d;
	c = ugetch ();
	d = ugetch ();
	d <<= 8;
	d |= c & 0377;
	return (d);
}

static void
putch(char c)
{
	if(udest < udend)
		*udest++ = c;
	else
		longjmp(packjmp, 1);
}

/* this is the guts of the unpack command, but it already
 * has the data in the buffer, and of course, it avoids the
 * overhead of fork and exec
*/
static int
unpack(char *src, int srccnt, char *dest, int destcnt)
{
	usrc = src;
	usend = src + srccnt -1;
	udest = dest;
	udend = dest + destcnt -1;
	if(setjmp(packjmp)) {
		*udest = '\0';
		return udest - dest;
	}
	getdict();
	*udest = '\0';
	return udest - dest;
}


/* read in the dictionary portion and build decoding structures */
/* return 1 if successful, 0 otherwise */
static void
getdict(void)
{
	register int c, i, nchildren;

	/*
	 * check two-byte header
	 * get size of original file,
	 * get number of levels in maxlev,
	 * get number of leaves on level i in intnodes[i],
	 * set tree[i] to point to leaves for level i
	 */
	eof = &characters[0];

	inleft = usend - usrc;
	if (usrc[0] != US)
		goto goof;

	if (usrc[1] == US) {		/* oldstyle packing */
		expand ();
		return;
	}
	if (usrc[1] != RS)
		goto goof;

	uninp = &usrc[2];
	origsize = 0;
	for (i=0; i<4; i++)
		origsize = origsize*256 + ((*uninp++) & 0377);
	maxlev = *uninp++ & 0377;
	if (maxlev > 24) {
goof:
		return;
	}
	for (i=1; i<=maxlev; i++)
		intnodes[i] = *uninp++ & 0377;
	for (i=1; i<=maxlev; i++) {
		tree[i] = eof;
		for (c=intnodes[i]; c>0; c--) {
			if (eof >= &characters[255])
				goto goof;
			*eof++ = *uninp++;
		}
	}
	*eof++ = *uninp++;
	intnodes[maxlev] += 2;
	inleft -= uninp - &usrc[0];
	if (inleft < 0)
		goto goof;

	/*
	 * convert intnodes[i] to be number of
	 * internal nodes possessed by level i
	 */

	nchildren = 0;
	for (i=maxlev; i>=1; i--) {
		c = intnodes[i];
		intnodes[i] = nchildren /= 2;
		nchildren += c;
	}
	decode();
}


/* unpack the file */
static void
decode(void)
{
	register int bitsleft, c, i;
	int j, lev;
	char *p;
	char *orig = udest;

	lev = 1;
	i = 0;
	for(;;) {
		if(--inleft < 0) {
uggh:			
			return;
		}
		c = *uninp++;
		bitsleft = 8;
		while (--bitsleft >= 0) {
			i *= 2;
			if (c & 0200)
				i++;
			c <<= 1;
			if ((j = i - intnodes[lev]) >= 0) {
				p = &tree[lev][j];
				if (p == eof) {
					c = udest - orig;
					origsize -= c;
					if (origsize != 0)
						goto uggh;
					return;
				}
				*udest++ = *p;
				if (udest == udend)
					longjmp(packjmp, 1);
				lev = 1;
				i = 0;
			} else
				lev++;
		}
	}
}


/*
 * This code is for unpacking files that
 * were packed using the old-style algorithm.
 */

static void
expand(void)
{
	register tp, bit;
	short word;
	int keysize, i, *t;

	uninp = &usrc[2];
	inleft -= 2;
	origsize = ((long) (unsigned) getword ())*256*256;
	origsize += (unsigned) getword ();
	t = Tree;
	for (keysize = getword (); keysize--; ) {
		if ((i = ugetch ()) == 0377)
			*t++ = getword ();
		else
			*t++ = i & 0377;
	}

	bit = tp = 0;
	for (;;) {
		if (bit <= 0) {
			word = getword ();
			bit = 16;
		}
		tp += Tree[tp + (word<0)];
		word <<= 1;
		bit--;
		if (Tree[tp] == 0) {
			putch (Tree[tp+1]);
			tp = 0;
			if ((origsize -= 1) == 0) {
				return;
			}
		}
	}
}
