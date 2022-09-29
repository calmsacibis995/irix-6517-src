/* UTIL.C -- MISCELLANEOUS SUPPORT FUNCTIONS                    */

/* history:                                                     */

/*  6/23/87  J. Howe     --  created                            */
/*  3/30/88  Gumby       --  added "tempext()"                  */
/*  7/27/88  J. Ellis    --  various revisions                  */

/*  8/25/88  RJK         --  rev 0.16                           */
/*  9/11/88  RJK         --  rev 0.17                           */
/* 10/25/88  RJK         --  rev 0.18                           */
/* 11/22/88  RJK         --  rev 0.19                           */
/*  2/28/89  RJK         --  rev 0.21 (1.0e0)                   */

#include "asm29.h"

#define fno(fp)     fp->_file           /* pseudo-file definition */
#define SFP_FLAG    99                  /* string file-pointer flag     */

extern char header[], listtitle[], listsubttl[], symheader[], symunder[];

/*
 * Array of valid symbol characters.
 */
char _namech[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,
	1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,
	0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,0,1,0,0,1,
	0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
	0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
};


char *
nam_str(char *s)
{
	static char buf[S_LEN+1];

	(void)strncpy(buf,s,S_LEN);
	return (buf);
}

/*
 * Allocate memory (with error-message).
 */
char *
m_alloc(unsigned int siz)
{
	char *ptr;

	if (!(ptr = (char *)malloc(siz))) {
		printf ("Out of Memory - assembler aborted\n");
		(void)unlink(dstname);
		exit(1);
	}
	return ptr;
}

/*
 * Allocate an array (with error-message).
 */
char *
e_alloc (unsigned int num, unsigned int siz)
{
	char *ptr;

	if (!(ptr = calloc(num, siz))) {
		printf ("Out of Memory - assembler aborted\n");
		(void)unlink(dstname);
		exit(1);
	}
	return ptr;
}

/* ------------------------------------------------------------ */

/*
 * Save a label.
 */
savelabel()
{
	int i;
	struct sym *ps;
	long value;
	short scnum;

	i = (inst == NULL) ? 0 : inst->rs_flags;

	if ((tagname[0] == EOS) || ifstk[ifsp]) return;

	if (i & R_NOLAB) {
		error (e_lbl);
		return;
	}

	if (inmac) return;

#ifdef OLD
	tag = insertsym (tagname,NULL);
#else
	if ((ps = findsym (tagname,NULL)) != NULL) {
		if (ps->sy_flags & F_EXTERN) {
			/* previously COMMON or EXTERN  */
			ps->sy_flags &= ~(F_EXTERN | F_UNDEF);
			ps->se.n_value = 0;
		} else {
			/* multiply-defined             */
			ps = NULL;
		}
	}

	tag = ps;
	if (tag == NULL)
		tag = insertsym (tagname,NULL);
#endif

	if (tag == NULL) return;
	if (srctype == ' ')
		tag->sy_defline = lineno;

	/* list generated code */
	if (curseg != DSECT)
		listovr = 1;
	listpc = 1;

	/* make '.' absolute if the current segment is */
	value = lpc;
	scnum = (cursegp->sg_head.s_flags & STYP_ABS)? 0 : curseg;
	if (pass == 3) {
		if ((tag->se.n_scnum != scnum)
		    || (tag->se.n_value != value)) {
			error (e_phase);
		}
	}

	tag->se.n_scnum = scnum;
	tag->se.n_value = value;
	tag->sy_flags |= F_LOCSYM;
}


/*
 * Compare two strings.  Returns -1 if a < b, 0 if a = b, +1 if a > b.
 */
int
cmpstr(char *a, char *b)
{
	int i;

	if ((i = strcmp(a,b)) == 0)
		return 0;
	if (*a == '?')
		return 1 - (i & 2);  /* arbitrarily 1 or -1 */
	if (i < 0)
		return -1;
	return 1;
}

/* ------------------------------------------------------------ */

/* "ne_jmp(jmp_b)"  returns  true (nonzero) if and only  if the */
/* jump buffer "jmp_b" is non-empty.                            */

/* This function assumes that jump buffers are arrays.          */

/* ------------------------------------------------------------ */

int ne_jmp(jmp_b)
jmp_buf jmp_b;                  /* jump buffer                  */
{
    jmp_buf jmp_tmp;            /* kludge                       */
    int n = sizeof(jmp_tmp);    /* size of a jump buffer        */
    char *s = (char *) jmp_b;   /* pointer into jump buffer     */

    while (n-- > 0) if (*s++) return (TRUE);
    return (FALSE);
}

/* ------------------------------------------------------------ */

/* "jmpcpy(jmp_s,jmp_t)"  copies the  jump buffer  "jmp_s" into */
/* the jump buffer "jmp_t".                                     */

/* This function assumes that jump buffers are arrays.          */

/* ------------------------------------------------------------ */

jmpcpy(jmp_s,jmp_t)
jmp_buf jmp_s;                  /* source buffer                */
jmp_buf jmp_t;                  /* target buffer                */
{
    static jmp_buf jmp_tmp;     /* kludge                       */
    int n = sizeof(jmp_tmp);    /* size of a jump buffer        */

    char *s = (char *) jmp_s;   /* pointer into source buffer   */
    char *t = (char *) jmp_t;   /* pointer into target buffer   */

/* copy target into source      */

    while (n-- > 0) *t++ = *s++;
}

/* ------------------------------------------------------------ */

/* "clrjmp(jmp_b)" clears the jump buffer "jmp_b".              */

/* This function assumes that jump buffers are arrays.          */

/* ------------------------------------------------------------ */

clrjmp(jmp_b)
jmp_buf jmp_b;                  /* target buffer                */
{
    static jmp_buf no_jmp;      /* empty jump buffer            */

    jmpcpy (no_jmp,jmp_b);      /* clear the target buffer      */
}

/* ------------------------------------------------------------ */

/* If  the character  contained  in  "c" is  an  ASCII  letter, */
/* "lower(c)"  returns  the  lower-case  form  of  the  letter. */
/* Otherwise, "lower()" returns the character unchanged.        */

/* ------------------------------------------------------------ */

int lower(c)
int c;                          /* character to translate       */
{
    if ((c < 'A') || (c > 'Z')) return (c);
    return (c - 'A' + 'a');
}

/* ------------------------------------------------------------ */

/*
 * Convert a string to lower case.
 */
downshift (s)
char *s;
{
	while (*s)
		{
		if (isupper(*s))
			*s += 'a'-'A';
		s++;
		}
}

/* ------------------------------------------------------------ */

/*
 * Allocate memory & save a string.  Returns a pointer to the saved string.
 */
char*
savestring(char *s)
{
	char *cp;

	cp = m_alloc(strlen(s)+1);
	(void)strcpy(cp, s);
	return cp;
}

/*
 * Eject to new page & print header, if needed.
 */
int
newpage(void)
{
	if (pglength && pageline && (pageline >= pglength)) {
		printf ("\f");
		printf ("%s\n",header);
		printf ("   %s          %s\n\n",listtitle,listsubttl);
		pageline = 0;
		return 1;
	}
	return 0;
}

/*
 * Open "include" files on a nested basis.  Each time a new source file is
 *   opened, this routine sets up an include-file table entry which contains
 *   pertinent info on file state. The table is a linked list which reverts
 *   to the previous file when the current one is closed.  Macros are treated
 *   as a special case and 'opened' as though they were a normal 'C'  file.
 */
int
infopen(char *flname,			/* file name */
	int type,			/* 'M' = macro, ' ' = source */
	char *body)			/* pointer to macro body if type=M */
{
	struct include *ip;
	FILE *fp;

	ip = (struct include *)e_alloc(1,sizeof(struct include));
	if (type == 'M') {
		fp = &ip->if_fs;
		s_open (body,strlen(body),fp);
	} else if (!(fp = fopen(flname,"r"))) {
		return -1;
	}
	ip->if_link = src;
	ip->if_fp = fp;
	ip->if_name = savestring(flname);
	ip->if_type = type;
	src = ip;
	error(0);			/* reset errline */
	return 0;
}


/*
 * Close an "include" file.
 */
int
inclose(int absolutely)
{
	struct include *ip;
	int i;

	if (src == NULL) {
		printf ("Assembler foo: src\n");
		exit (1);
	}
	error (0);			/* reset errline */

	if (ifstk[ifsp] || (src->if_link == NULL && ifsp)) {
		error (e_noendc);
		if (src->if_link == NULL)
			ifsp = 0;
	}
	if (src->if_link == NULL
	    && !absolutely)
		return 0;		/* is top-level source file */

	if (src->if_fp != &src->if_fs) {    /* if not a macro, just close */
		(void)fclose (src->if_fp);
	} else {			/* macro; decrement recursion count */
		if (src->if_mac->m_rcnt > 0)
			src->if_mac->m_rcnt--;
		else
			error (e_recur);
	}

	if (src->if_name != NULL)
		free (src->if_name);
	for (i = 0; i < src->if_argc; i++) {
		if (src->if_args[i].arg_val != NULL)
			free (src->if_args[i].arg_val);
	}
	ip = src;
	src = src->if_link;
	free ((char *)ip);
	return 1;
}

/*
 * Insert a string into another string.
 */
strinsert (d, s)
char *d, *s;
{
char *p1, *p2;
int i;

	/* copy rest of destination after end of destination */
	i = strlen (d);
	p1 = d + i;                                             /* 1 char after current */
	p2 = p1 + strlen (s);                   /* 1 char after final */
	*p2 = '\0';                                             /* null after end of result */
	while (i-- > 0)
		*--p2 = *--p1;
	while (*s)                                              /* copy source into the hole */
		*d++ = *s++;
}

/*
 * Delete characters from a string.
 */
void
strdelete (char *d, int n)
{
	char *s;

	s = d + n;
	while (*d++ = *s++) {
		;
	}
}

/*
 * Check that a value is within specified bounds.
 */

long
bcheck(long n, long l, long h)
{
#ifdef OLD
	if ((n < l) || (n > h)) error (e_range);
#else
	if ((n < l) || (n > h)) {
		error (e_range);
		n = l;
	}
#endif
	return (n);
}


/* If "sn" is a normal (positive) section number, "num_hdr(sn)" */
/* returns a pointer to the associated section header.          */
struct scnhdr *
num_hdr(int sn)                         /* section number               */
{
	struct seglist *slp;		/* segment-list pointer         */

	for (slp = sectlist; slp != NULL; slp = slp->sg_link) {
		if (--sn == 0) return (&(slp->sg_head));
	}

	panic ("num_hdr");		/* inconsistent; panic          */
	return (NULL);			/* superfluous (not reached)    */
}



/*
 * Add a segment to the segment-table.  If needed, a symbol-table entry
 *   (name = segname) is created to be used in the COFF file when relocating
 *   local-label references.
 */

addseg(sh)
struct scnhdr *sh;
{
    char *cp;
    struct sym *s;
    struct seglist *sl;

#ifdef OLD
    s = insertsym (sh->s_name,segtab);
#else
    cp = nam_str (sh->s_name);
    s  = insertsym (cp,segtab);
#endif

    if ((pass != 1) || (s == NULL)) return;
    s->sy_segx = SEGX_MARK;
    sl = sectlist;

    while (sl->sg_link != NULL)
    {
	sl = sl->sg_link;
    }

    cp            = e_alloc (1,sizeof(struct seglist));
    sl->sg_link   = (struct seglist *) cp;
    sl            = sl->sg_link;
    sl->sg_head   = *sh;
    s->se.n_value = (long) sl;
    s->se.n_scnum = ++nsegs;    /* local segment number         */
}


/*
 * Open a string as an I/O stream.
 */
void
s_open(char *cptr,			/* pointer to string */
       int cnt,				/* string length */
       FILE *fp)			/* pointer to file-structure */
{
	fp->_flag = 0;          /* force reads to fail */
	fp->_base = fp->_ptr = (unsigned char*)cptr;
	fp->_cnt = cnt;
	fno(fp) = SFP_FLAG;
}

/*
 * Determine if "fp" represents a string-file or a normal file.
 *    Returns "true" if a string-file, "false" if normal.
 */
static int
is_sfp(FILE *fp)
{
	return (fno(fp) == SFP_FLAG);
}

/*
 * Seek to a specified position in a normal or string file.
 */
void
s_seek (FILE *fp,
	long offset,
	int mode)
{
	short *xp = (short *) &offset;

	if (is_sfp (fp)) {
		fp->_cnt = (int) xp[0];
		fp->_ptr = fp->_base + (int) xp[1];
		return;
	}

	fseek(fp, offset, mode);
}


/*
 * Get the current position of a normal or string file.
 */
long
s_tell(FILE *fp)
{
long result;

	short *xp = (short *) &result;
	if (is_sfp (fp))
		{
		xp[0] = (short) fp->_cnt;
		xp[1] = (short) (fp->_ptr - fp->_base);
		return (result);
		}
	return (ftell (fp));
}

#ifdef NOTUSED
/*
 * Convert an integer to an ascii string.
 */
itos (i, cp)
int i;                                                          /* integer to be converted */
char *cp;                                                       /* pointer to ascii string */
{
char c[10];

	char *cptr = &c[1];
	c[0] = '\0';
	if (!i)
		{
		strcpy(cp,"0");
		return;
		}
	while (i > 0)
		{
		*cptr++ = (i % 10) + 0x30;
		i = i / 10;
		}
	while (*(--cptr))
		*cp++ = *cptr;
	*cp = '\0';
}
#endif /* NOTUSED */

/*
 * Extract the actual name portion of a filename.
 */
char *
fname(char *s)
{
	char *p;

	p = s;
	while (*p) {
		if ((*p == ':') || (*p == '/') || (*p == '\\'))
			s = p;
		p++;
	}
	return s;
}

/*
 * Generate an extension for a temporary symbol name.
 */
char *
tempext(void)
{
	int c;
	char *cp;
	int i = 0;
	struct include *ip;
	static char retbuf[256];

	ip = src;
	if (ip->if_type == 'M') {	/* generate unique-in-macro label */
		do {
			ip = ip->if_link;
			i += ip->if_lineno;
		} while (ip->if_type == 'M');   /* outer macri */
		sprintf (retbuf, "_%s_%d", fname(ip->if_name), i);

		for (cp = retbuf; (c = *cp) != EOS; cp++) {
			if (!namech(c)) *cp = USC;
		}
	} else {
		sprintf (retbuf, "_%d", lab_cnt);
	}
	return retbuf;
}


void
checkfor(int c, int req)
{
	deblank();
	if (*lptr != c) {
		if (req)
			error (e_missing);
		return;
	}
	lptr++;
	deblank();
}

#ifdef SWAP_COFF
/*
 * Put a value in a byte buffer in a specified order.
 */
#ifndef LSB
#define LSB     1
#define MSB     0
#endif

void
putswap(long m,				/* desired value */
	char *cp,			/* buffer pointer */
	int size)			/* number of bytes to convert */
{
	int     i;

	if (target_order == LSB)
		for (i = 0; i < size; i++)
			{
			*(cp+i) = m ;
			m >>= 8;
			}
	  else
		for (i = size-1; i >= 0; i--)
			{
			*(cp+i) = m;
			m >>= 8;
			}
}
#endif /* SWAP_COFF */

/*
 * Write a line number.
 */
wrlineno (slp, lnp)
struct seglist *slp;
struct lineno *lnp;
{
long save;

	if (pass == 3)
		{
		save = ftell(outf);
		fseek (outf, slp->sg_head.s_lnnoptr+slp->sg_head.s_nlnno*LINESZ, 0);
		wrline (lnp, outf);
		fseek (outf, save, 0);
		 }
	slp->sg_head.s_nlnno++;
}

/* handle SIGFPE exceptions */

#ifdef SIGFPE
#ifdef HIF
void fpe_sig()
#else
void fpe_sig()
#endif
{
    if (ne_jmp (jmp_fltp))      /* use jump vector?             */
    {                           /* yes                          */
	jmp_buf jmp_tmp;

	jmpcpy (jmp_fltp,jmp_tmp);
	clrjmp (jmp_fltp);

	(void)signal(SIGFPE,fpe_sig);
	longjmp (jmp_tmp,TRUE);
    }

    s_fatal ("floating-point exception");
}
#endif

/*
 * Generate the listing-header time string in "timestr".
 */

char timestr[30];

#if MSDOS || HIF
#define unix
#endif

#ifdef unix
#include <time.h>
define_now()
{
long now;
struct tm *tp;

	now = time(NULL);
#ifdef DOWNUNDA
	tp = localtime(now);
#else   /* down-under UNIX */
	tp = localtime(&now);
#endif
#endif

#ifdef IPT_XCC
#include <time.h>
define_now()
{
long time(), now;
struct tm *tp, *localtime();

	now = time(0);
	tp = localtime(now);
#endif

#ifdef vax11c
#include <time.h>
struct tm *gtp;
define_now()
{
long time(), now;
struct tm *tp, *localtime();

	now = time(0);
	tp = localtime(&now);
	gtp = tp;
#endif

#ifdef AOSVS			/* treat all D.G.C. machines the same */
#define AOS
#endif
#ifdef RDOS
#define AOS
#endif

#ifdef AOS
define_now()
{
short int date[3], time[3];

	gdate (date);		/* set up D.G.C header time */
	gtime (time);
	sprintf (timestr, "%d-%.3s-%d, %02d:%02d", date[0],
			 &"JanFebMarAprMayJunJulAugSepOctNovDec"[3*(date[1]-1)],
			 date[2], time[2], time[1]);
#else                                                           /* set up non-D.G.C. header time */
	sprintf (timestr, "%d-%.3s-%d, %02d:%02d", tp->tm_mday,
			 &"JanFebMarAprMayJunJulAugSepOctNovDec"[3*tp->tm_mon],
			 tp->tm_year+1900, tp->tm_hour, tp->tm_min);
#endif
}
