/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.54 $"
/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */
#include	<ctype.h>
#include	<fcntl.h>
#include	<signal.h>
#include	<stdio.h>
#include        <stdlib.h>
#include        <limits.h>
#include        <locale.h>
#include        <archives.h>
#include	<sys/param.h>
#include	<sys/types.h>
#include	<sys/attributes.h>
#include	<sys/hwgraph.h>
#include	<sys/mkdev.h>
#include	<sys/stat.h>
#include	<pfmt.h>
#include	<string.h>
#include	<errno.h>
#include	<invent.h>
#include	<paths.h>
#include	<ar.h>
#include	<libelf.h>

/*
**	File Types
*/

#define	C_TEXT		0
#define FORTRAN_TEXT	1
#define AS_TEXT		2
#define NROFF_TEXT	3
#define CMD_TEXT	4
#define ENGLISH_TEXT	5
#define ASCII_TEXT	6
#define TEXT		7
#define PS_TEXT		8

/*
**	Misc
*/

#define BUFSZ	128
#define	FBSZ	4096	/* justify by the fact that standard block size
			 * for UFS is 8K and S5 is 2K bytes
			 * must be >= TBLOCK, and current largest user
			 * is bru at 2228 bytes.
			 */
#define	reg	register
#define NBLOCK  20

/* Assembly language comment char */
#ifdef pdp11
#define ASCOMCHAR '/'
#else
#define ASCOMCHAR '#'
#endif
char	fbuf[FBSZ];
char	*mfile = _PATH_MAGIC;
						/* Fortran */
char	*fort[] = {
	"block","character","common","complex","data",
	"dimension","double","entry","external","format",
	"function","implicit","integer","intrinsic",
	"logical","parameter","program","real","save", 
	"subroutine","assign","close","continue","open",
	"print","read","return","rewind","stop","write",
	0};

char	*asc[] = {
	"addiu","jal","lui","subu",0};
						/* C Language */
char	*c[] = {
	"int","char","float","double","short","long","unsigned","register",
	"static","struct","extern", 0};
						/* Assembly Language */
char	*as[] = {
	"globl","byte","even","text","data","bss","comm",0};

char 	cfunc[] = { '(', ')', '{', 0 };		/* state of C function */


/* start for MB env */
wchar_t wchar;
int     length;
int     IS_ascii;
int     Max;
/* end for MB env */
int     i = 0;
int	fbsz;
int	ifd;
int	tret;
int	hflg = 0;
int	Sflg = 0;

static int check_e_type(ushort);
int is_bru(void);

const	char	badopen[]  = ":92:cannot open %s: %s\n";

#define	pfmte(s,f,c,a,b)	{putchar('\n'); fflush(stdout); pfmt((s),(f),(c),(a),(b));}

static void
prf(char *x)
{
	if (pfmt(stdout, MM_NOSTD, "uxlibc:84:%s:\t", x) < 9)
		printf("\t");
}

main(argc, argv)
int  argc;
char **argv;
{
	reg	char	*p;
	reg	int	ch;
	reg	FILE	*fl;
	reg	int	cflg = 0, eflg = 0, fflg = 0;
	auto	char	ap[BUFSZ];
	extern	int	optind;
	extern	char	*optarg;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore");
	(void)setlabel("UX:file");

	while((ch = getopt(argc, argv, "Schf:m:")) != EOF)
	switch(ch) {
	case 'c':
		cflg++;
		break;

	case 'f':
		fflg++;
		if ((fl = fopen(optarg, "r")) == NULL) {
			pfmte(stderr, MM_ERROR, badopen, optarg, strerror(errno));
			goto use;
		}
		break;

	case 'm':
		mfile = optarg;
		break;

	case 'h':
		hflg++;
		break;

	case 'S':
		Sflg++;
		break;

	case '?':
		eflg++;
		break;
	}
	if(!cflg && !fflg && (eflg || optind == argc)) {
		if (!eflg)
			pfmt(stderr, MM_ERROR, ":194:Incorrect usage:\n");
use:
		pfmt(stderr, MM_INFO, 
			":195:Usage: file [-S] [-c] [-h] [-f ffile] [-m mfile] file...\n");
		exit(2);
	}
	if(mkmtab(mfile, cflg) == -1)
		exit(2);
	if(cflg) {
		prtmtab();
		exit(0);
	}
	for(; fflg || optind < argc; optind += !fflg) {
		reg	int	l;

		if(fflg) {
			if((p = fgets(ap, BUFSZ, fl)) == NULL) {
				fflg = 0;
				optind--;
				continue;
			}
			l = strlen(p);
			if(l > 0)
				p[l - 1] = '\0';
		} else
			p = argv[optind];
		prf(p);				/* print "file_name:<tab>" */

		tret=type(p);
		if(ifd)
			close(ifd);
	}
	if (tret != 0) {
		exit(tret);
	} else {
		exit(0);	/*NOTREACHED*/
	}
}

type(file)
char	*file;
{
	int	j,nl;
        int     cc,tar;
	char	ch;
	char	buf[BUFSIZ];
	struct	stat64	mbuf;
	struct	stat64	statb;
	int	(*statf)() = hflg ? lstat64 : stat64;
        int     notdecl;
	int	cur_type;
	ushort	is_c_flag;

	int skip_line();
	int c_comment();
	int c_define();
	int c_function();
	
	ifd = -1;
	if ((*statf)(file, &mbuf) < 0) {
		if (statf == lstat64 || lstat64(file, &mbuf) < 0) {
			pfmte(stdout, MM_ERROR, badopen, file, strerror(errno));
			return(0);
		}
	}
	switch (mbuf.st_mode & S_IFMT) {
	case S_IFCHR:
					/* major and minor, see sys/mkdev.h */
		pfmt(stdout, MM_NOSTD, ":196:character special (%d/%d)",
			major(mbuf.st_rdev), minor(mbuf.st_rdev));

		if (Sflg) {
			int bufsize = BUFSIZ;
			if (filename_to_devname(file, buf, &bufsize))
				pfmt(stdout, MM_NOSTD|MM_NOGET, " --> %s", buf);
		}

		pfmt(stdout, MM_NOSTD|MM_NOGET,"\n");

		return(0);

	case S_IFDIR:
		if (mbuf.st_mode&S_ISVTX)
		    pfmt(stdout, MM_NOSTD, "uxsgicore:2:append-only ");
		pfmt(stdout, MM_NOSTD, ":197:directory\n");
		return(0);
	case S_IFSOCK:
		pfmt(stdout, MM_NOSTD, "uxsgicore:3:socket\n");
		return(0);

	case S_IFIFO:
		pfmt(stdout, MM_NOSTD, ":198:fifo\n");
		return(0);

 	case S_IFNAM:
			switch (mbuf.st_rdev) {
			case S_INSEM:
       	                 	pfmt(stdout, MM_NOSTD, ":199:Xenix semaphore\n");
       	                 	return(0);
			case S_INSHD:
       	                 	pfmt(stdout, MM_NOSTD, 
       	                 		":200:Xenix shared memory handle\n");
       	                 	return(0);
			default:
       	              	   	pfmt(stdout, MM_NOSTD, 
       	              	   		":201:unknown Xenix name special file\n");
       	               	  	return(0);
			}

	case S_IFLNK:
		if ((cc = readlink(file, buf, BUFSIZ)) < 0) {
			pfmte(stderr, MM_ERROR,
				":202:Cannot read symbolic link %s: %s\n", 
				file, strerror(errno));
			return(1);
		}
		buf[cc] = '\0';
		if (stat64(file, &statb) != 0) {
		    pfmt(stdout, MM_NOSTD,
				    "uxsgicore:9:dangling symbolic link\n");
		    return(1);
		}
		pfmt(stdout, MM_NOSTD, ":203:symbolic link to %s\n", buf);
		return(0); 

	case S_IFBLK:
					/* major and minor, see sys/mkdev.h */
		pfmt(stdout, MM_NOSTD, ":204:block special (%d/%d)",
			major(mbuf.st_rdev), minor(mbuf.st_rdev));

		if (Sflg) {
			int bufsize = BUFSIZ;
			if (filename_to_devname(file, buf, &bufsize))
				pfmt(stdout, MM_NOSTD|MM_NOGET, " --> %s", buf);
		}

		pfmt(stdout, MM_NOSTD|MM_NOGET,"\n");

		return(0);
	}

	/* check for /proc file */
	if (strncmp(mbuf.st_fstype, "proc", 4) ==0) { 
		pfmt(stdout, MM_NOSTD, 
			"uxsgicore:891:/proc file, process image file\n");
		return(0);
	}

	ifd = open(file, O_RDONLY);
	if(ifd < 0) {
		pfmte(stdout, MM_ERROR, badopen, file, strerror(errno));
		return(0);
	}

        if ((fbsz = read(ifd, fbuf, FBSZ)) == -1) {
		pfmte(stderr, MM_ERROR, ":205:Cannot read %s: %s\n",
			file, strerror(errno));
                return(1);
        }
        if(fbsz == 0) {
		pfmt(stdout, MM_NOSTD, ":644:Empty file\n");
		goto out;
	}
	if(fbsz >= TBLOCK) {
	    /* check for (new style) tar header */
	    union   tblock  *tarbuf;
	    tarbuf = (union tblock *)fbuf;
	    if ( tar = (strncmp(tarbuf->tbuf.t_magic, "ustar", 5)) == 0) {
		    pfmt(stdout, MM_NOSTD, ":645:tar \n");
		    goto out;
	    }
	}
	if(is_bru())
		goto out;
	if(mips_elf(ifd))
		goto out;
	if (script())
		goto out;

	/* although archives are in the Magic Table, we want to specifically
	   identify archives holding 32-bit and 64-bit object files */
	if (object_archive())
		goto out;
	if(sccs()) {	/* look for "1hddddd" where d is a digit */
		pfmt(stdout, MM_NOSTD, ":207:sccs\n");
		goto out;
	}
	switch(ckmtab(fbuf,fbsz,0)){ /* Check against Magic Table entries */
		case -1:             /* Error */
			exit(2);
		case 0:              /* Not magic */
			break;
		default:             /* Switch is magic index */
			goto out;
	}

	/* look at each line for either C 
    	 * definition/declaration statement or C function. 
	 * Skip comment, pre-processor and irrelevant lines.
	 * until the buffer is exhausted or C program is found.
	 * Avoid using goto in this C checking section.
	 */
	is_c_flag = i = 0;	/* reset buffer index pointer and is_c_flag */
	while (i < fbsz && !is_c_flag) {
		switch(fbuf[i++]) {
		case '/':
			if(fbuf[i++] != '*')
				skip_line();
			else
				c_comment();	/* C comment style */
			continue;
		case '#':	/* C preprocessor, skip it */
			skip_line();
			continue;
		case ' ':
		case '\t':
			skip_line();
			continue;
		case '\n':
			continue;
		default:
			i--;  	/* back track 1 character for lookup() */
			if (isalpha(fbuf[i])) {
				if (lookup(c) == 1) {
					if (c_define()) 
						is_c_flag = 1;
				} else {
					if (c_function())
						is_c_flag = 1;
				} /* braces here for clarity only */
				continue;
			}

			/* Not start with character, skip the line */
			skip_line();
			continue;
		} /* end switch */
	} 
	/* If is_c_flag is set, then we find C program,
	 * otherwise we already exhausted the buffer. 
         * Next step is to reset an index pointer and check 
	 * for fortran text.
    	 */
	if (is_c_flag) {
		cur_type = C_TEXT;
		goto outa;
	}
notc:
	i = 0;
	while(fbuf[i] == 'c' || fbuf[i] == '#') {
		while(fbuf[i++] != '\n')
			if(i >= fbsz)
				goto notfort;
	}
	if(lookup(fort) == 1){
		cur_type = FORTRAN_TEXT;
		goto outa;
	}
notfort:
	i = 0;
	if(ascom() == 0)
		goto notas;
	j = i-1;
	if(fbuf[i] == '.') {
		i++;
		if(lookup(as) == 1){
			goto isas;
		}
		else if(j != -1 && fbuf[j] == '\n' && isalpha(fbuf[j+2])){
			goto isroff;
		}
	}
	while(lookup(asc) == 0) {
		if(ascom() == 0)
			goto notas;
		while(fbuf[i] != '\n' && fbuf[i++] != ':')
			if(i >= fbsz)
				goto notas;
		while(fbuf[i] == '\n' || fbuf[i] == ' ' || fbuf[i] == '\t')
			if(i++ >= fbsz)
				goto notas;
		j = i - 1;
		if(fbuf[i] == '.'){
			i++;
			if(lookup(as) == 1) {
				goto isas;
			}
			else if(fbuf[j] == '\n' && isalpha(fbuf[j+2])) {
isroff:				cur_type = NROFF_TEXT;
				goto outa;
			}
		}
	}
isas:	cur_type = AS_TEXT;
	goto outa;
notas:
	/* start modification for multibyte env */	
	IS_ascii = 1;
        if (fbsz < FBSZ)
                Max = fbsz;
        else
                Max = FBSZ - MB_LEN_MAX; /* prevent cut of wchar read */
        /* end modification for multibyte env */
	for(i=0; i < Max; )
		if(fbuf[i]&0200) {
			IS_ascii = 0;
			if (fbuf[0]=='\100' && fbuf[1]=='\357') {
				pfmt(stdout, MM_NOSTD, ":209:troff output\n");
				goto out;
			}
		/* start modification for multibyte env */
			if ((length=mbtowc(&wchar, &fbuf[i],MB_LEN_MAX)) <= 0
			    || !wisprint(wchar)){
				pfmt(stdout, MM_NOSTD, ":208:data\n");
				goto out; 
			}
			i += length;
		}
		else
			i++;
	i = fbsz;
		/* end modification for multibyte env */
	if (mbuf.st_mode&(S_IXUSR|S_IXGRP|S_IXOTH))
		cur_type = CMD_TEXT;
	else if(english(fbuf, fbsz))
		cur_type = ENGLISH_TEXT;
	else if(IS_ascii)
		cur_type = ASCII_TEXT;
	else 
		cur_type = TEXT;
		/* for multibyte env */
outa:
	/* 
	 * This code is to make sure that no MB char is cut in half
	 * while still being used.
	 */
	fbsz = (fbsz < FBSZ ? fbsz : fbsz - MB_CUR_MAX + 1);
	while(i < fbsz){
		if (isascii(fbuf[i])){
			i++;
			continue;
		}
		else {
			if ((length=mbtowc(&wchar, &fbuf[i],MB_LEN_MAX)) <= 0
		        	|| !wisprint(wchar)){
		        switch(cur_type){
		        case C_TEXT:
		        	pfmt(stdout, MM_NOSTD,
		        		":210:c program text with garbage\n");
		        	break;
		        case FORTRAN_TEXT:
		        	pfmt(stdout, MM_NOSTD,
		        		":211:fortran program text with garbage\n");
				break;
			case AS_TEXT:
				pfmt(stdout, MM_NOSTD,
					":212:assembler program text with garbage\n");
				break;
			case NROFF_TEXT:
				pfmt(stdout, MM_NOSTD,
					":213:[nt]roff, tbl, or eqn input text with garbage\n");
				break;
			case CMD_TEXT:
				pfmt(stdout, MM_NOSTD,
					":214:commands text with garbage\n");
				break;
			case ENGLISH_TEXT:
				pfmt(stdout, MM_NOSTD,
					":215:English text with garbage\n");
				break;
			case ASCII_TEXT:
				pfmt(stdout, MM_NOSTD,
					":216:ascii text with garbage\n");
				break;
			case TEXT:
				pfmt(stdout, MM_NOSTD,
					":217:text with garbage\n");
				break;
			}
			goto out;
			}
			i = i + length;
		}
	}
	switch(cur_type){
	case C_TEXT:
		pfmt(stdout, MM_NOSTD, ":218:c program text\n");
		break;
	case FORTRAN_TEXT:
        	pfmt(stdout, MM_NOSTD, ":219:fortran program text\n");
        	break;
	case AS_TEXT:
		pfmt(stdout, MM_NOSTD, ":220:assembler program text\n");
		break;
	case NROFF_TEXT:
		pfmt(stdout, MM_NOSTD, ":221:[nt]roff, tbl, or eqn input text\n");
		break;
	case CMD_TEXT:
		pfmt(stdout, MM_NOSTD, ":222:commands text\n");
		break;
	case ENGLISH_TEXT:
		pfmt(stdout, MM_NOSTD, ":223:English text\n");
		break;
	case ASCII_TEXT:
		pfmt(stdout, MM_NOSTD, ":224:ascii text\n");
		break;
	case TEXT:
		pfmt(stdout, MM_NOSTD, ":225:text\n");
		break;
	}
out:
	return(0);
}

lookup(tab)
reg	char **tab;
{
	reg	char	r;
	reg	int	k,j,l;

	while(fbuf[i] == ' ' || fbuf[i] == '\t' || fbuf[i] == '\n')
		i++;
	for(j=0; tab[j] != 0; j++) {
		l = 0;
		for(k=i; ((r=tab[j][l++]) == fbuf[k] && r != '\0');k++);
		if(r == '\0')
			if(fbuf[k] == ' ' || fbuf[k] == '\n' || fbuf[k] == '\t'
			    || fbuf[k] == '{' || fbuf[k] == '/') {
				i=k;
				return(1);
			}
	}
	return(0);
}
/*
 * Non-recursive check routine for C comment
 * If it fails return 0, otherwise return 1
 * Use global variable i and fbuf[]
 */
int
c_comment()
{
	reg	char	cc;

	while(fbuf[i] != '*' || fbuf[i+1] != '/') {
		if(fbuf[i] == '\\')
			i += 2;
		else
			i++;
		if(i >= fbsz)
			return(0);
	}
	if((i += 2) >= fbsz)
		return(0);
	return(1);
}
/* 
 * Skip the whole line in fbuf[], also
 * update global index (i).
 * Return 0 if go over limit, otherwise return 1
 */
int
skip_line()
{
 
	while (fbuf[i++] != '\n') {
		if (i >= fbsz )
			return(0);
	}
	return(1);
}
/*
 * Check if the line is likely to be a
 * C definition/declaration statement
 * If true return 1, otherwise return 0
 */
int
c_define()
{
	reg char  a_char;

	while ((a_char = fbuf[i++]) != ';' && a_char != '{' ) {
		if (i >= fbsz || a_char == '\n')
			return(0);
	}
	return(1);
}
/* 
 * Check if this line and the following lines
 * form a C function, normally in the format of
 * func_name(....) {
 * Also detect a line that can't possible be a
 * C function, i.e. page_t xxx; or STATIC ino_t y();
 * Update global array index (i) 
 * If true return 1, otherwise return 0
 */
int
c_function()
{
	reg int state = 0;

	while (cfunc[state] != 0) {
		if (fbuf[i++] == cfunc[state]) {
			if (state == 1 && (fbuf[i] == ';' || fbuf[i] == ','))
				return(0);
			state++;
		}
		if ((state == 0 && (fbuf[i] == ';' || fbuf[i] == '\n'))
			|| (i >= fbsz))
			return(0);
	}
	return(1);
}


ascom()
{
        while(fbuf[i] == ASCOMCHAR || (fbuf[i] == '/' && fbuf[i+1] == '*')) {/*}*/
		if ( fbuf[i] == '/' ) {
			c_comment();
			continue;
		}
		i++;
		while(fbuf[i++] != '\n')
			if(i >= fbsz)
				return(0);
		while(fbuf[i] == '\n')
			if(i++ >= fbsz)
				return(0);
	}
	return(1);
}

sccs() 
{				/* look for "1hddddd" where d is a digit */
	reg int j;

	if(fbuf[0] == 1 && fbuf[1] == 'h') {
		for(j=2; j<=6; j++) {
			if(isdigit(fbuf[j]))  
				continue;
			else  
				return(0);
		}
	} else {
		return(0);
	}
	return(1);
}

english (bp, n)
char *bp;
int  n;
{
#	define NASC 128		/* number of ascii char ?? */
	reg	int	j, vow, freq, rare;
	reg	int	badpun = 0, punct = 0;
	auto	int	ct[NASC];

	if (n<50)
		return(0); /* no point in statistics on squibs */
	for(j=0; j<NASC; j++)
		ct[j]=0;
	for(j=0; j<n; j++)
	{
		if (bp[j]<NASC)
			ct[bp[j]|040]++;
		switch (bp[j])
		{
		case '.': 
		case ',': 
		case ')': 
		case '%':
		case ';': 
		case ':': 
		case '?':
			punct++;
			if(j < n-1 && bp[j+1] != ' ' && bp[j+1] != '\n')
				badpun++;
		}
	}
	if (badpun*5 > punct)
		return(0);
	vow = ct['a'] + ct['e'] + ct['i'] + ct['o'] + ct['u'];
	freq = ct['e'] + ct['t'] + ct['a'] + ct['i'] + ct['o'] + ct['n'];
	rare = ct['v'] + ct['j'] + ct['k'] + ct['q'] + ct['x'] + ct['z'];
	if(2*ct[';'] > ct['e'])
		return(0);
	if((ct['>']+ct['<']+ct['/'])>ct['e'])
		return(0);	/* shell file test */
	return (vow*5 >= n-ct[' '] && freq >= 10*rare);
}

script() 	/* Also prints script type */
{
	reg int i;
	char *shellname;

	if (fbuf[0] == '#' && fbuf[1] == '!') {
		i = 2;
		while (fbuf[i] != '\n' && fbuf[i] != '\0' && i < fbsz) {
			i++;
		}
		if(i >= fbsz)
			return 0;
		pfmt(stdout, MM_NOSTD, "uxsgicore:60:%.*s script text\n", i - 2,
			&fbuf[2]);
		return 1;
	}
	return 0;
}

#include <elf.h>

#define TRUE	1
#define FALSE	0
#ifndef EF_MIPS_ARCH_3
#define EF_MIPS_ARCH_3	0x20000000
#endif
/*
 * The variable for STRING_TBLE ... i
 *	H = elfhdr
 * 	S = elfshdr
 */
char *name = 0;
#define STRING_TBLE(H,S) \
	(name = elf_strptr(elf, H->e_shstrndx, (size_t)S->sh_name)) && \
	(strncmp(name,MIPS_DEBUG_INFO,strlen(name)) == 0)

static char * e_type_str[] =
{
	"uxsgicore:137: unknown type",
	"uxsgicore:138: relocatable",
	"uxsgicore:139: executable",
	"uxsgicore:140: dynamic lib",
	"uxsgicore:141: core file"
};

int
mips_elf(int fd)
{
	Elf32_Ehdr *elfhdr;

	elfhdr = (Elf32_Ehdr *) fbuf;
	if (!IS_ELF(*elfhdr) || elfhdr->e_machine != EM_MIPS) 
	  	return 0;

	switch (elfhdr->e_ident[EI_CLASS]) {
	case ELFCLASSNONE:
		return mips_elfnone(fd);
	case ELFCLASS32:
		return mips_elf32(fd);
	case ELFCLASS64:
		return mips_elf64(fd);
	default:
		break;
	}
	return 0;
}

int
mips_elfnone(int fd)
{
	Elf32_Ehdr *elfhdr;
	Elf32_Shdr elfshdr;
	int n;

	elfhdr = (Elf32_Ehdr *) fbuf;

	printf("ELF");
	/* It is ELFCLASSNONE */
	if (elfhdr->e_ident[EI_DATA] == ELFDATA2LSB)
	  	pfmt(stdout, MM_NOSTD, "uxsgicore:143: LSB");
	if (elfhdr->e_ident[EI_DATA] == ELFDATA2MSB)
	  	pfmt(stdout, MM_NOSTD, "uxsgicore:144: MSB");
	if (check_e_type(elfhdr->e_type))
		return 0;
	pfmt(stdout, MM_NOSTD, "uxsgicore:146: MIPS");
	pfmt(stdout, MM_NOSTD, "uxsgicore:147: - version %ld\n", elfhdr->e_version);

	return 1;
}

int
mips_elf32(int fd)
{

	Elf *elf;
	Elf_Cmd cmd;
	Elf_Scn *scn;
	Elf32_Ehdr *elfhdr;
	Elf32_Shdr *elfshdr;
	int dynamic = FALSE;
	int stripped= TRUE;
	Elf32_Word arch;
	int n;

	scn=0;
	cmd = ELF_C_READ;
	if(elf_version(EV_CURRENT) == EV_NONE) {
		pfmt(stdout, MM_NOSTD, "uxsgicore:946: Using old version of libelf.");
		return (1);
	}

	if((elf = elf_begin(fd, cmd, (Elf *)0)) == NULL) {
		pfmt(stdout, MM_NOSTD,
			"uxsgicore:947: Error from file command using elf_begin routine.");
		elf_end(elf);
		return (1);
	}

	if ((elfhdr = elf32_getehdr(elf)) == NULL) {
		pfmt(stdout, MM_NOSTD, 
			"uxsgicore:948: Error from file command using elf_getehdr routine.\n");
		elf_end(elf);
		return (1);
	}

	while ((scn = elf_nextscn(elf, scn)) != 0) {
		if ((elfshdr = elf32_getshdr(scn)) != 0) {
			if (elfshdr->sh_type == SHT_DYNAMIC)
				dynamic = TRUE;
			/* if it's n32 and has a dwarf or it's o32 and has an mdebug
			 * symbol table, it's not stripped */
			if(elfhdr->e_flags & EF_MIPS_ABI2) { /* n32 */
				if(elfshdr->sh_type == SHT_MIPS_DWARF && 
					STRING_TBLE(elfhdr,elfshdr))
					stripped = FALSE;
			}
			else /* o32 */ if((name = elf_strptr(elf, elfhdr->e_shstrndx,
				(size_t)elfshdr->sh_name)) &&
				strncmp(name,MIPS_MDEBUG,strlen(name)) == 0)
				stripped = FALSE;
		}
	}

	printf("ELF");
	/* It is ELFCLASS32 */
	if (elfhdr->e_flags & EF_MIPS_ABI2)
	    pfmt(stdout, MM_NOSTD, "uxsgicore:945: N32");
	else
	    pfmt(stdout, MM_NOSTD, "uxsgicore:142: 32-bit");
	if (elfhdr->e_ident[EI_DATA] == ELFDATA2LSB)
	  	pfmt(stdout, MM_NOSTD, "uxsgicore:143: LSB");
	if (elfhdr->e_ident[EI_DATA] == ELFDATA2MSB)
	  	pfmt(stdout, MM_NOSTD, "uxsgicore:144: MSB");

	if (arch=elfhdr->e_flags & EF_MIPS_ARCH) {
		if (arch == EF_MIPS_ARCH_2)
			pfmt(stdout, MM_NOSTD, "uxsgicore:135: mips-2");
		else if (arch == EF_MIPS_ARCH_3)
			pfmt(stdout, MM_NOSTD, "uxsgicore:136: mips-3");
		else if (arch == EF_MIPS_ARCH_4)
			pfmt(stdout, MM_NOSTD, "uxsgicore:942: mips-4");
	}
	if (dynamic && elfhdr->e_type==ET_EXEC)
		pfmt(stdout, MM_NOSTD, "uxsgicore:133: dynamic");
	if (check_e_type(elfhdr->e_type)) {
		elf_end(elf);
		return 0;
	}
	if (!stripped && (elfhdr->e_type==ET_REL || elfhdr->e_type==ET_EXEC))
		pfmt(stdout, MM_NOSTD, "uxsgicore:145: (not stripped)");
	pfmt(stdout, MM_NOSTD, "uxsgicore:146: MIPS");
	pfmt(stdout, MM_NOSTD, "uxsgicore:147: - version %ld\n", elfhdr->e_version);

	elf_end(elf);

	return 1;
}

int
mips_elf64(int fd)
{
	Elf *elf;
	Elf_Cmd cmd;
	Elf_Scn *scn;
	Elf64_Ehdr *elfhdr;
	Elf64_Shdr *elfshdr;
	int dynamic = FALSE;
	int stripped= TRUE;
	Elf64_Word arch;
	int n;

	scn=0;
	cmd = ELF_C_READ;
	if(elf_version(EV_CURRENT) == EV_NONE) {
		pfmt(stdout, MM_NOSTD, "uxsgicore:946: Using old version of libelf.");
		return (1);
	}

	if((elf = elf_begin(fd, cmd, (Elf *)0)) == NULL) {
		pfmt(stdout, MM_NOSTD, 
			"uxsgicore:947: Error from file command using elf_begin routine.");
		elf_end(elf);
		return (1);
	}

	if ((elfhdr = elf64_getehdr(elf)) == NULL) {
		pfmt(stdout, MM_NOSTD,
			"uxsgicore:948: Error from file command using elf_getehdr routine.");
		elf_end(elf);
		return (1);
	}

	while ((scn = elf_nextscn(elf, scn)) != 0) {
		if ((elfshdr = elf64_getshdr(scn)) != 0) {
			if (elfshdr->sh_type == SHT_DYNAMIC)
				dynamic = TRUE;
			if (elfshdr->sh_type == SHT_MIPS_DWARF && 
				STRING_TBLE(elfhdr,elfshdr))
				stripped = FALSE;
		}
	}

	printf("ELF");
	/* It is ELFCLASS64 */

	/*
 	 * If the type is ET_IR the check the flags for :
	 * 	 EF_MIPS_64BIT_WHIRL = 64bit binary
	 * 	 !EF_MIPS_64BIT_WHIRL = N32 binary
	 * 
	 * NOTE : that the class type is set for 64bits for objects
	 * with ET_IR set.
 	 */

	if(elfhdr->e_type == ET_IR) {
		if (elfhdr->e_flags & EF_MIPS_64BIT_WHIRL) 
			pfmt(stdout, MM_NOSTD, "uxsgicore:134: 64-bit");
		else
	    		pfmt(stdout, MM_NOSTD, "uxsgicore:945: N32");
	} else {
		if (elfhdr->e_flags & EF_MIPS_ABI2)
	    		pfmt(stdout, MM_NOSTD, "uxsgicore:945: N32");
		else
			pfmt(stdout, MM_NOSTD, "uxsgicore:134: 64-bit");
	}

	if (elfhdr->e_ident[EI_DATA] == ELFDATA2LSB)
	  	pfmt(stdout, MM_NOSTD, "uxsgicore:143: LSB");
	if (elfhdr->e_ident[EI_DATA] == ELFDATA2MSB)
  		pfmt(stdout, MM_NOSTD, "uxsgicore:144: MSB");
	
	if (arch=elfhdr->e_flags & EF_MIPS_ARCH) {
		if (arch == EF_MIPS_ARCH_2)
			pfmt(stdout, MM_NOSTD, "uxsgicore:135: mips-2");
		else if (arch == EF_MIPS_ARCH_3)
			pfmt(stdout, MM_NOSTD, "uxsgicore:136: mips-3");
		else if (arch == EF_MIPS_ARCH_4)
			pfmt(stdout, MM_NOSTD, "uxsgicore:942: mips-4");
	}

	if (dynamic && elfhdr->e_type==ET_EXEC)
		pfmt(stdout, MM_NOSTD, "uxsgicore:133: dynamic");

	if (check_e_type(elfhdr->e_type)) {
		elf_end(elf);
		return 0;
	}

	if (!stripped && (elfhdr->e_type==ET_REL || elfhdr->e_type==ET_EXEC))
		pfmt(stdout, MM_NOSTD, "uxsgicore:145: (not stripped)");

	pfmt(stdout, MM_NOSTD, "uxsgicore:146: MIPS");
	pfmt(stdout, MM_NOSTD, "uxsgicore:147: - version %ld\n", elfhdr->e_version);

	elf_end(elf);

	return 1;
}

static int
check_e_type(ushort elf_e_type)
{
	int ret_val = 0;

	switch (elf_e_type) {
		case ET_NONE :
		case ET_REL  :
		case ET_EXEC :
		case ET_DYN  :
		case ET_CORE :
			pfmt(stdout, MM_NOSTD, e_type_str[elf_e_type]);
			break;
		case ET_IR :
			pfmt(stdout, MM_NOSTD, "uxsgicore:941: intermediate object");
			break;
		/* 
		 * At this time if the ET_HIPROC bits are set then claim that
		 * the file has "No file type".
		 */
		case ET_HIPROC :
			pfmt(stdout, MM_NOSTD, e_type_str[0]);
			break;
		default	:
			ret_val = 1;
			break;
	}
	return ret_val;
}

#define BLANKS "                "

int
object_archive()
{
	struct ar_hdr *hdr;

	if(fbsz < (SARMAG+sizeof(hdr)))
		return 0;

	/* see if first 8 bytes are "!<arch>\n" */
	if (strncmp(fbuf, ARMAG, SARMAG))
		return 0;

	/* we now know the file is an archive; now we check to see if it
	   contains objects - if so, print it out; otherwise, return fail */

	hdr = (struct ar_hdr *)&fbuf[SARMAG];

	if (!strncmp(hdr->ar_name, ELF_AR64_SYMTAB_NAME,
			ELF_AR64_SYMTAB_NAME_LEN) &&
	    !strncmp(&hdr->ar_name[ELF_AR64_SYMTAB_NAME_LEN], BLANKS,
			16 - ELF_AR64_SYMTAB_NAME_LEN))
		printf("current ar archive containing 64-bit objects\n");

	else if (!strncmp(hdr->ar_name, ELF_AR_SYMTAB_NAME,
			ELF_AR_SYMTAB_NAME_LEN) &&
	    !strncmp(&hdr->ar_name[ELF_AR_SYMTAB_NAME_LEN], BLANKS,
			16 - ELF_AR_SYMTAB_NAME_LEN))
		printf("current ar archive containing 32-bit objects\n");

	else
		return(0);

	return(1);
}

is_bru(void)
{
	/* A_MAGIC at 176, and H_MAGIC at 2224, and "SGI Release" at 400; 
	 * allow that to not be there, in case they use 3rd party bru.
	 */

	/* protect against somebody dropping FBSZ */
	if(FBSZ < 2228) printf("error, FBSZ too small\n");

	if(fbsz>=2228 && strncmp("1234", &fbuf[176], 4) == 0  && 
		strncmp("2345", &fbuf[2224], 4) == 0) {
		char *notsgi;
		if(strncmp("SGI Release", &fbuf[400], 11) == 0)
			notsgi = "";
		else
			notsgi = "non-SGI ";
		pfmt(stdout, MM_NOSTD, ":749:%sbru archive\n", notsgi);
		return 1;
	}
	return 0;
}
