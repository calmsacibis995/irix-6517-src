/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/strings/RCS/strings.c,v 1.17 1999/05/11 21:26:20 olson Exp $"

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980 Regents of the University of California.\n\
 All rights reserved.\n";
#endif 


#include <sys/types.h>
#include <sys/file.h>
#include <a.out.h>
#include <stdio.h>
#include <ctype.h>
#include <locale.h>
#include <widec.h>
#include <wctype.h>
#include <stdarg.h>
#include <fmtmsg.h>
#include <unistd.h>
#include <sgi_nl.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <msgs/uxsgicore.h>
#include <elf_abi.h>

#define DEF_LEN		4		/* default minimum string length */
#define EOS		(char)NULL	/* end of string */
#define ERR		-1		/* general error */
#define ERREXIT		1		/* error exit */
#define NO		0		/* false/no */
#define OK		0		/* ok exit */
#define YES		1		/* true/yes */

#define DEBUG if (debug) printf

int debug = 0;
int _xpg = 0;	/* in xpg compliant mode (strings only if end in
	\0, \n, or EOF), if set.  See bug 642929 */

typedef struct exec	EXEC;		/* struct exec cast */

extern const char * setcat(const char *);
extern int setlabel(const char *);

static wchar_t	getch(void);
static long	foff;			/* offset in the file */
static int	head_len,		/* length of header */
		read_len;		/* length to read */

void process(void);


static u_char	hbfr[FILHSZ+AOUTHSZ];	/* buffer for filehdr, aouthdr */
FILHDR		*filhdr = (FILHDR *) hbfr;
AOUTHDR		*aouthdr = (AOUTHDR *) &hbfr[FILHSZ]; 

int	minlen = DEF_LEN;		/* minimum string length */
short oflg;			        /* print location in decimal*/
				        /* with padding*/
/* Flags for xpg4 */
short decflg;                                /* print decimal location*/  
short octflg;                                /* print octal location */
short hexflg;                                /* print hex location */
wchar_t ch;					/* character */
int	cnt;					/* general counter */
wchar_t *C;					/* bfr pointer */
wchar_t	*bfr;				/* collection buffer */

static int end;
static char cmd_label[] = "UX:strings";


/*
 * some error prints
 */
static void
err_opt(int c)
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_illoption, "illegal option -- %c"),
	    c);
	_sgi_nl_usage(SGINL_USAGE, cmd_label,
	    gettxt(_SGI_DMMX_usage_strings, "strings [-ao] [-#] [file ... ]"));
}

static void
err_usage(void)
{
    _sgi_nl_usage(SGINL_USAGE, cmd_label,
         gettxt(_SGI_DMMX_usage_strings, "strings [-ao] [-#] [file ... ]"));
}

static void
err_nomem(void)
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_outofmem, "Out of memory"));
}

static void
err_open(char *s)
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_CannotOpen, "Cannot open %s"),
	    s);
}

static void
err_read(char *s)
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_CannotRead, "Cannot read from %s"),
	    s);
}

static void
err_seek(char *s)
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_CannotSeek, "%s: Cannot seek"),
	    s);
}

/*
 * main entry
 */
main(int argc, char **argv)
{
	short	asdata = NO;		/* look in everything */
	char	*file;			/* file name for error */
	char *strtab;			/* variable for ELF Formats */
	int s_num;
	off_t foff;		/* variable to hold shdr file offset */
	int need_num = NO;              /* set if next arg needs to be a num */
	int need_letter = NO;
	char *xpg;
        /*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	xpg = getenv("_XPG");
	_xpg = xpg && atoi(xpg) > 0;

	/*
	 * for backward compatibility, allow '-' to specify 'a' flag; no
	 * longer documented in the man page or usage string.
	 */
	
        for (++argv; (*argv && (*argv)[0] ==  '-') || (need_num == YES) || 
			 (need_letter == YES); ++argv) {
            if (need_letter == YES) {
		cnt = 0;
		switch ((*argv)[cnt]) {
		case 'd':
		    decflg = YES;
		    break;
		case 'o':
		    octflg = YES;
		    break;
		case 'x':
		    hexflg = YES;
		    break;
		default:
		    err_opt((*argv)[cnt]);
		    exit(ERREXIT);
		    break;
		}
		need_letter = NO;
		continue;
	    }    
	    if (need_num == YES) {
		if (*argv) {
		    cnt = 0;
		    if (!isdigit((*argv)[cnt])) {
			err_opt((*argv)[cnt]);
			exit(ERREXIT);
		    }
		    minlen = atoi(*argv);
		    need_num = NO;
		    continue; 
		} else {
		    err_usage();
		    exit(ERREXIT);
		}
	    } 
	    if (strcmp(*argv, "--") == 0) {
		++argv;
		break;
            }
	    for (cnt = 1;(*argv)[cnt];++cnt)
		switch ((*argv)[cnt]) {
                case 'a':
                    asdata = YES;
                    break;
                case 'o':
                    oflg = YES;
                    break;
		case 'd': 
		    debug = YES;
		    break;
		case 't':
		    need_letter = YES;
		    break;
		case 'n':
                    if (((*argv)[cnt+1]) == 0) {
                        need_num = YES;
                        break;
                    } else if (isdigit((*argv)[cnt+1]) == 0) {
                        err_opt((*argv)[cnt+1]);
                        exit(ERREXIT);
                    } else {
                        DEBUG ("argv +2 = %s\n",*argv + 2);
			minlen = strtol(*argv + 2, argv, 0);
			cnt = -1;
			DEBUG("minlen = %d\nargv = %s\n", minlen, *argv);
			need_num = NO;
		    }
                    break;

		default:        /* getopt message compatible */
		    if (!isdigit((*argv)[cnt])) {
                        err_opt((*argv)[cnt]);
                        exit(ERREXIT);
                    }
                    minlen = atoi(*argv + 1);
                    break;
                }	    
	    if (cnt == 1)
                asdata = YES;
        }

	file = "stdin";
	do {
	    if (*argv) {
		if (!freopen(*argv,"r",stdin)) {
		    err_open(*argv);
		    exit(ERREXIT);
		}
		file = *argv++;
	    }
	    else	asdata = YES;
	    foff = 0;
	    read_len = ERR;
	    if (asdata)
		process();
	    else {
		/* check for good header */
		if ((head_len = read(fileno(stdin),(char *)filhdr, 
				     FILHSZ)) == ERR) {
		    err_read(file);
		    exit(ERREXIT);
		}
		
		if ISCOFF(filhdr->f_magic) {	/* process COFF format */
		    if ((head_len += read(fileno(stdin),(char *)aouthdr,
					  AOUTHSZ)) == ERR) {
			err_read(file);
			exit(ERREXIT);
		    }
		    if (head_len == FILHSZ+AOUTHSZ && !N_BADMAG(*aouthdr)){
			foff = N_TXTOFF(*filhdr, *aouthdr) + aouthdr->tsize;
			if (fseek64(stdin,foff,L_SET) == ERR) {
			    err_seek(file);
			    exit(ERREXIT);
			}
			read_len = aouthdr->dsize;
			head_len = 0;
			process();
		    } else {
			if (fseek64(stdin, 0LL, 0) == ERR) {
			    err_seek(file);
			    perror(file);
			    exit(ERREXIT);
			}
			process();
		    }
		} else {
				/* 
				   Read header and then check to see if the 
				   file is a 32 or 64bit file.
				   */
		    Elf32_Ehdr	ehdr;
		    
		    fseek64(stdin, 0LL, 0);
		    if ((head_len = read(fileno(stdin), &ehdr,
					 sizeof(ehdr))) == ERR) {
			err_read(file);
			exit(ERREXIT);
		    }
		    
				/* If it is a ELF 32bit format then process */
		    
		    if (head_len == sizeof(ehdr) && IS_ELF(ehdr) && 
			ehdr.e_ident[EI_CLASS] == ELFCLASS32) {
			Elf32_Shdr secthdr;
			
			foff = ehdr.e_shoff; 	/* section hdr tbl offset */
			
				/* get strings table header */
			fseek64(stdin, foff+(ehdr.e_shstrndx*ehdr.e_shentsize),0);
			if (read(fileno(stdin), &secthdr,sizeof(secthdr)) != sizeof(secthdr)) {
			    err_read(file);
			    exit(ERREXIT);
			}
			
				/* read string table section */
			strtab = malloc(secthdr.sh_size);
			fseek64(stdin,secthdr.sh_offset,0);
			if (read(fileno(stdin), strtab,secthdr.sh_size) != secthdr.sh_size) {
			    err_read(file);
			    exit(ERREXIT);
			}
			
				/* for each section in the section header table, 
				 * read the section header and determine the section name.
				 * process .data, and .rodata sections
				 	*/ 
			for (s_num = 0; s_num < ehdr.e_shnum; s_num = ++s_num) {
			    fseek64(stdin, foff, 0);
			    
				/* read section header */
			    if (read(fileno(stdin),&secthdr,sizeof(secthdr)) != 
				sizeof(secthdr)) {
				err_read(file);
				exit(1);
			    }
				/* check name of section */
			    if ((strcmp(strtab+secthdr.sh_name,ELF_DATA) == 0) ||
				(strcmp(strtab+secthdr.sh_name,ELF_RODATA) == 0)) {
				read_len = (int)secthdr.sh_size;
				head_len = 0;
				if (fseek64(stdin, secthdr.sh_offset, L_SET) == ERR) {
				    err_seek(file);
				    exit(ERREXIT);
				} /* if */
				process(); /* process section */
			    } /* if */
				/* update pointer to next header */
			    foff += ehdr.e_shentsize;
			} /* for */
		    } else if (IS_ELF(ehdr) && ehdr.e_ident[EI_CLASS] == ELFCLASS64) {
			
				/* Check if it is a ELF 64bit format */
			
			Elf64_Ehdr ehdr_64;
			Elf64_Shdr secthdr_64;
			
				/* Re-read to completely fill the 64bit ELF header */
			
			fseek64(stdin, 0LL, 0);
			if ((head_len = read(fileno(stdin), &ehdr_64,
				     	     sizeof(ehdr_64))) == ERR) {
			    err_read(file);
			    exit(ERREXIT);
			}
			
			
			foff = ehdr_64.e_shoff; /* section hdr tbl offset */
			
				/* get strings table header */
			fseek64(stdin, foff+(ehdr_64.e_shstrndx*ehdr_64.e_shentsize),0);
			if (read(fileno(stdin), &secthdr_64,sizeof(secthdr_64)) != sizeof(secthdr_64)) {
			    err_read(file);
			    exit(ERREXIT);
			}
			
				/* read string table section */
			strtab = malloc(secthdr_64.sh_size);
			fseek64(stdin,secthdr_64.sh_offset,0);
			if (read(fileno(stdin), strtab,secthdr_64.sh_size) != secthdr_64.sh_size) {
			    err_read(file);
			    exit(ERREXIT);
			}
			
				/* for each section in the section header table, 
				 * read the section header and determine the section name.
				 * process .data, and .rodata sections
				 */ 
			for (s_num = 0; s_num < ehdr_64.e_shnum; s_num = ++s_num) {
			    fseek64(stdin, foff, 0);
			    
				/* read section header */
			    if (read(fileno(stdin),&secthdr_64,sizeof(secthdr_64)) != 
				sizeof(secthdr_64)) {
				err_read(file);
				exit(1);
			    }
				/* check name of section */
			    if ((strcmp(strtab+secthdr_64.sh_name,ELF_DATA) == 0) ||
				(strcmp(strtab+secthdr_64.sh_name,ELF_RODATA) == 0)) {
				read_len = secthdr_64.sh_size;
				head_len = 0;
				if (fseek64(stdin, secthdr_64.sh_offset, L_SET) == ERR) {
				    err_seek(file);
				    exit(ERREXIT);
				} /* if */
				process(); /* process section */
			    } /* if */
				/* update pointer to next header */
			    foff += ehdr_64.e_shentsize;
			} /* for */
		    } else {
				/* If not COFF or ELF, do whole file */
			if (fseek64(stdin, 0LL, 0) == ERR) {
			    err_seek(file);
			    exit(ERREXIT);
			}
			process();
		    }  /* if (IS_ELF(ehdr)) */
		}
		
	    }
	} while (*argv);
	return(OK);
}


/* 
 * process --
 *	find strings in section
 */
void
process(void)
{
	register long cnt;
	size_t buf_cnt;
	long foff_sv;

	buf_cnt = ((LINE_MAX > (minlen+1)) ? LINE_MAX : minlen+1);
	if (!(bfr = (wchar_t *)malloc((size_t)((buf_cnt)*sizeof(wchar_t))))) {
	    err_nomem();
	    exit(ERREXIT);
	}
	for (cnt = 0, end = 0; !end; ) {
		ch = getch(); 
		if (end)
			break;
		if (iswprint(ch) || ch == L'\t' || (_xpg && ch == L'\n')) {
			if (!cnt) {
				C = bfr;
				foff_sv = foff-1;
			}
			if( cnt < minlen && (_xpg && (ch == L'\n' || ch == 0))){
				cnt = 0;
				continue;
			}
			*C++ = ch;
			if (++cnt < minlen)
				continue;

			/* Satisfied min cnt */
			for(;;) {
				ch = getch();
				if(end)		/* Allow end of file to terminate minlen string */
					goto output;
				if (!iswprint(ch) && ch != L'\t') {
					if(_xpg && ch != L'\n' && ch != 0)
						break;
					else
						goto output;
				}
				if ((cnt+1) > buf_cnt){
					if (!(realloc(bfr,(size_t)((buf_cnt+LINE_MAX)*
							sizeof(wchar_t))))) {
					    err_nomem();
					    exit(ERREXIT);
					}
					buf_cnt += LINE_MAX;
				}
				if( ch != L'\n' ){
					*C++ = ch;
					++cnt;
				}
				if( ch == 0 || ch == L'\n' ) {
output:
					*C = 0;
					if (decflg)
					    printf("%d %S",foff_sv,bfr);
					else if (hexflg)
					    printf("%x %S",foff_sv,bfr);
					else if (octflg)
					    printf("%o %S",foff_sv,bfr);
					else if (oflg)
					    printf("%07ld %S",foff_sv,bfr);
					else
					    fputws(bfr,stdout);
					putchar(L'\n');
					break;
				}
			}
		}
		cnt = 0;
	}
}


/*
 * getch --
 *	get next character from wherever
 */
static wchar_t
getch(void)
{
	wchar_t wc;

	if (read_len != ERR && read_len-- == 0) {
	    end = 1;
	    return -1;
	}
	wc = getwchar();
	if (feof(stdin) || ferror(stdin)) {
	    end = 1;
	    return -1;
	}
	foff = ftell(stdin);
	return(wc);
}

