/* "ASM29.C" -- THE Am29000 ASSEMBLER MAIN PROGRAM              */

/*
 * history:
 *
 * created   n/a       E. Wilner  --  created
 * n/a        6/24/87  J. Howe    --  modified for COFF structures
 * Rev 0.01   9/16/87  J. Howe    --  update to spec of 9/14/87
 * Rev 0.02   9/21/87  J. Howe    --  update to spec of 9/18/87
 * Rev 0.03  10/09/87  J. Howe    --  general fixes
 * Rev 0.04  10/21/87  J. Howe    --  COFF headers MSB 1st
 * Rev 0.05  10/28/87  J. Howe    --  header size portability bug
 * Rev 0.06  n/a       n/a        --  never existed
 * Rev 0.07   1/20/88  M. Ahern   --  fix per bug list from EPI
 * Rev 0.08   1/29/88  M. Ahern   --  fix per R.Carlson (new spec)
 * Rev 0.09   2/05/88  M. Ahern   --  misc bug fixes
 * Rev 0.10   2/15/88  M. Ahern   --  general fixes
 * Rev 0.11   3/30/88  M. Ahern   --  spec modifications and fixes
 * Rev 0.12   4/04/88  M. Ahern   --  n/a
 * Rev 0.13   5/02/88  n/a        --  misc adjustments
 * Rev 0.14   6/03/88  Gumby      --  misc bug fixes & cleaning-up
 * Rev 0.15   8/15/88  J. Ellis   --  various revisions
 */

/*  8/25/88  RJK         --  rev 0.16                           */
/*  9/12/88  RJK         --  rev 0.17                           */
/* 10/26/88  RJK         --  rev 0.18                           */
/* 11/22/88  RJK         --  rev 0.19                           */
/* 12/10/88  RJK         --  rev 0.20                           */
/*  2/28/89  RJK         --  rev 0.21 (1.0e0)                   */

/* ------------------------------------------------------------ */

#define GLOBAL
#include "asm29.h"

extern void initbuf(void);
extern void useseg(char*);
extern char timestr[];

char header[82];
char listtitle[LLEN+1];
char listsubttl[LLEN+1];

static long fpos;
static long fpos_raw;

/* ------------------------------------------------------------ */

/* predefined sections          */

struct seglist defsegs[] =
{
    {                           /* text                         */
	defsegs+1,
	{
	    ".text", 0L, 0L, 0L, NULL, NULL, NULL, 0, 0, STYP_TEXT
	},
	0, 0L, 0L, NULL
    },
    {                           /* data                         */
	defsegs+2,
	{
	    ".data", 0L, 0L, 0L, NULL, NULL, NULL, 0, 0, STYP_DATA
	},
	0, 0L, 0L, NULL
    },
    {                           /* bss                          */
	NULL,
	{
	    ".bss",  0L, 0L, 0L, NULL, NULL, NULL, 0, 0, STYP_BSS
	},
	0, 0L, 0L, NULL
    }
};

/* ------------------------------------------------------------ */

/*
 * Predefined symbols for Am29000 registers
 */

struct pred_reg
	{
	char    reg_name[4];
	int     reg_val;
	};

struct pred_reg predsyms[] =
	{
	{ "vab",0 }, {"ops",1 }, {"cps",2 }, {"cfg",3},
	{ "cha",4 }, {"chd",5 }, {"chc",6 }, {"rbp",7},
	{ "tmc",8 }, {"tmr",9 }, {"pc0",10}, {"pc1",11},
	{ "pc2",12}, {"mmu",13}, {"lru",14},

	{ "cir",29}, {"cdr",30},

	{ "ipc",128},{"ipa",129},{"ipb",130},{"q",131},
	{ "alu",132},{"bp", 133},{"fc", 134},{"cr",135}
	};

#define PREDSZ (sizeof(predsyms)/sizeof(predsyms[0]))

/* ------------------------------------------------------------ */

static void
reset_sy(struct sym *sptr)
{
    if (sptr == NULL) return;

    if (sptr->sy_flags & F_SETSYM)
    {
	sptr->sy_flags |= F_UNDEF;
    }

#ifdef CS_IF
    sptr->sy_f2 &= ~F2_DEFP;
#endif
    reset_sy (sptr->sy_rlink);

    reset_sy (sptr->sy_llink);
}

/* ------------------------------------------------------------ */

static void
undef_macs(struct sym *sptr)
{
	if (sptr == NULL)
		return;
	undef_macs (sptr->sy_llink);
	sptr->sy_flags |= F_UNDEF;
	undef_macs (sptr->sy_rlink);
}

/* ------------------------------------------------------------ */

/*
 * Initialize counters and buffers before each pass.
 */

pass_init()
{
	fseek (src->if_fp, 0L, 0);
	clearerr (src->if_fp);
	errcnt  = 0;
	warncnt = 0;
	lwords = pageline = lineno = 0;
	src->if_lineno = 0;
	lab_cnt = 0;
	ndbsyms = 0;
	ndbfils = 0;
	nblocks = 0;
	llist   = list;
	lsttag  = lastdbp = dbugtab;
	saptr   = NULL;
	lpc = pc = 0L;
	ifsp = ifstk[0] = 0;
	initbuf();			/* initialize the code buffer */
	useseg(".text");		/* default section */
	prevsegp = NULL;
	prevseg = 0;
	reset_sy (symtab);      /* reset symbol information     */
	undef_macs(mactab);
}

/* ------------------------------------------------------------ */

/*
 * Assign alphabetical indices to symbol-table entries for COFF.
 */

static void
set_indices(struct sym *sptr,		/* user's symbol table to search */
	    short mask)
{
	struct debug_sym *dbp;

	if (sptr == NULL)
		return;
	set_indices (sptr->sy_llink, mask);
	if (sptr->sy_flags & mask)              /* search user's table for this name; if */
		{                                                       /* found, use its index, else use ours */
		if (dbp = finddbugsym (sptr->sy_name))
			sptr->sy_index = dbp->db_index;
		  else
			sptr->sy_index = sym_index++;
		}
	set_indices (sptr->sy_rlink,mask);
}

/* ------------------------------------------------------------ */

/*
 * Process list flags.
 */

lflags(fcp)
char *fcp;
{
    int c;
    int flipflop;

    while (TRUE)
    {
	if (*fcp == 'n')
	{
	    fcp++;
	    flipflop = FALSE;
	}
	else
	{
	    flipflop = TRUE;
	}

	c = *fcp++;
	if (c && isspace(c)) continue;

	switch (c)
	{
    case EOS:
    case HASH:
    case SEMIC:
	    return;

    case 'c':
	    listcond = flipflop;
	    break;

    case 'g':
	    pasyms = flipflop;
	    listsym = TRUE;
	    break;

    case 'i':
	    listinc = flipflop;
	    break;

    case 'l':
	    pgcolumns = 0;

	    while (isdigit(*fcp))
	    {
		pgcolumns = (pgcolumns * 10) + (*fcp++ - '0');
	    }
	    if (pgcolumns < 79) pgcolumns = 79;
	    break;

    case 'm':
	    listmac = flipflop;
	    break;

    case 'o':
	    listalld = flipflop;
	    break;

    case 'p':
	    pglength = 0;

	    while (isdigit(*fcp))
	    {
		pglength = (pglength * 10) + (*fcp++ - '0');
	    }
	    break;

    case 's':
	    listsym = flipflop;
	    break;

    case 't':
	    psyms = flipflop;
	    listsym = TRUE;
	    break;

    case 'w':
	    omit_warn = 1;
	    break;

    case 'x':
	    xref = flipflop;
	    listsym = TRUE;
	    break;
    case 'P':
	    pcprec = 0;
	    while (isdigit(*fcp)) {
		    pcprec = (pcprec * 10) + (*fcp++ - '0');
	    }
	    break;

    case CR:
	    break;

    default:
	    if (pass > 1)
		    break;
	    printf("Illegal list flag: '%c'\n",fcp[-1]);
	    break;
	}
    }
}

/* ------------------------------------------------------------ */

/* "err_summ()" prints an error and/or warning summary.         */

/* Note:                                                        */

/* If any errors have occurred, "err_summ()" deletes the output */
/* file and prints a message to this effect.                    */

/* ------------------------------------------------------------ */

err_summ()
{
	char buf[80];

	char *fmt1 = "*** %d error(s) and %d warning(s) detected\n";
	char *fmt2 = "*** %d error(s) detected\n";

	if (omit_warn == FALSE) {
		sprintf (buf,fmt1,errcnt,warncnt);
	} else {
		sprintf (buf,fmt2,errcnt);
		warncnt = 0;            /* superfluous                  */
	}

	if ((silent == FALSE) || ((errcnt + warncnt) > 0)) {
		fflush (stdout);
		fprintf (stderr,buf);
		fflush (stderr);
		if (isatty (1) != isatty (2)) printf (buf);
	}

	if (errcnt > 0) {
		fflush (stdout);
		fprintf (stderr,"No object file produced.\n");
		fflush (stderr);
		(void)unlink(dstname);
	}
}


static void
usage(const char *pgm)
{
    static char help[] = {
	"\n    Options:\n"
	"\t-Dsym[=val]  --  define symbol with optional value\n"
	"\t-L		--  export local $L$ and $.L$ symbols\n"
	"\t-R		--  suppress $.data$ directive\n"
	"\n"
	"\t-l		--  generate a listing\n"
	"\t-f flags	--  listing flags\n"
	"\t-o outfile   --  specify output file\n"
	"\t-s		--  $silent$ operation\n"
	"\tListing flags (used with -f):\n"
	"\n"
	"\tc		--  include suppressed code\n"
	"\tg		--  include internal symbols\n"
	"\ti		--  list include files\n"
	"\tl###		--  specify line length (default is 132)\n"
	"\tm		--  include macro and rep expansions\n"
	"\to		--  list data storage overflow\n"
	"\tp###		--  specify page length\n"
	"\ts		--  list symbols\n"
	"\tw		--  suppress warning messages\n"
	"\tx		--  cross-reference\n"
	"\n"
    };

    (void)fprintf(stderr, "%s: [-LRlZHs] [-D name] [-o ofile] [-f lflags]\n",
		  pgm);
    if (isatty(2))
	(void)fputs(help, stderr);
    exit(1);
}

main(argc,argv)
int argc;
char *argv[];
{
    int i;
    struct scnhdr *isp;
    char *fcp, *cp, srcname[MAXPATHLEN], name[65];
    int n;
    char *prgnam = argv[0];     /* program name                 */
    struct seglist *slp;
    struct sym *sy;
    long val;

#ifdef SIGFPE
#ifdef HIF
    void fpe_sig();              /* handles SIGFPE exceptions    */
#else
    void fpe_sig();              /* handles SIGFPE exceptions    */
#endif
#endif

    list       = llist      = 0;
    listcond   = listmac    = listinc = listsym = 0;

    no_align   = STYP_DATA | STYP_DSECT;
    pgcolumns  = 132;
    pglength   =  55;
    psyms      = TRUE;
    silent     = 0;
    srcname[0] = dstname[0] = EOS;

    target_order = MSB;         /* target is processing machine */
				/* for now always takes MSB 1st */
#ifdef SIGFPE
    (void)signal(SIGFPE,fpe_sig);	/* handles SIGFPE exceptions    */
#endif

/* Initialize tables and variables before assembly. */

    symtab = (struct sym *) e_alloc (1, sizeof (struct sym));
    segtab = (struct sym *) e_alloc (1, sizeof (struct sym));
    mactab = (struct sym *) e_alloc (1, sizeof (struct sym));
    symtab->sy_name = segtab->sy_name = mactab->sy_name = "";
    dbugtab = (struct debug_sym *) e_alloc (1, sizeof (struct debug_sym) );
    sectlist = defsegs;

    for (i = 0; i < sizeof (defsegs) / sizeof (defsegs[0]); i++)
    {
#ifdef OLD
	sy = insertsym (defsegs[i].sg_head.s_name,segtab);
#else
	cp = nam_str (defsegs[i].sg_head.s_name);
	sy = insertsym (cp,segtab);
#endif
	if (sy == NULL) panic ("defsegs");
	sy->sy_segx = SEGX_MARK;
	sy->se.n_value = (long) &defsegs[i];
	sy->se.n_scnum = i+1;           /* local seg no */
    }
    nsegs = i;

/* define dummy symbol and section for .dsect directive */

    dummy = (struct sym *) e_alloc (1, sizeof(struct sym));
    dsection = (struct seglist *) e_alloc (1, sizeof (struct seglist));
    strcpy (dsection->sg_head.s_name, "$$");
    dummy->sy_segx = SEGX_MARK;
    dummy->se.n_value = (long) dsection;
    dummy->se.n_scnum = (-1);
    dsection->sg_head.s_flags = STYP_DSECT;

/* insert predefined register names into symbol table */

    for (i = 0; i < PREDSZ; i++) {
	sy = insertsym(predsyms[i].reg_name,NULL);
	if (sy == NULL)
	    panic ("predsyms");
	sy->se.n_value = (long) predsyms[i].reg_val;
	sy->se.n_scnum = N_ABS;
	sy->sy_flags = F_SPREG;
    }

    i = 1;
    if (*((char *)&i) == 1)
	host_order = LSB;

    while ((n = getopt(argc, argv, "LRD:lo:Zf:Hs")) != EOF) {
	switch (n) {
	case 'L':			/* export L* local symbols      */
	    lcflag = TRUE;
	    break;

	case 'R':			/* suppress .data direct. */
	    textonly = 1;		/* assemble all in.text section */
	    break;			/* >>><<< not yet implemented */

	case 'D':			/* define following sym and value */
	    for (fcp = name, cp = optarg; *cp > SPACE && *cp != '='; cp++)
		*fcp++ = *cp;
	    *fcp = EOS;
	    sy = insertsym(name,NULL);

	    if (sy == NULL)
		x_error ("multiply-defined symbol: %s",name);

	    if (*cp != '=') {
		val = 1;		/* value = 1 if no other specified */
	    } else {
		val = strtoul(++cp, &cp, 0);
	    }
	    sy->se.n_value = val;
	    sy->sy_f2 |= F2_DDEF;

	    if (*cp != EOS)
		eprintf ("Symbol value \"%s\" indecipherable\n", optarg);
	    break;

	case 'l':
	    llist = list = 1;
	    break;

	case 'o':			/* output file name */
	    (void)strcpy(dstname,optarg);
	    break;

	case 'Z':			/* pass 1 listing */
	    debugon = 1;
	    break;

	case 'f':			/* listing flags */
	    lflags(optarg);
	    break;

	case 'H':			/* no byte swapping of COFF headers */
	    target_order = host_order;
	    break;

	case 's':
	    silent = 1;
	    break;

	default:
	    usage(argv[0]);
	    break;
	}
    }

    if (optind+1 != argc)
	usage(argv[0]);

#if !HIF
    if (!silent && (isatty(1) != isatty(2))) {
	eprintf("%s V%s\n",DESCR,REVISION);
    }
#endif

    if (pcprec == 0 || pcprec > 8) {
	    pcprec = 8;
	    pcmask = -1;
    } else {
	    pcmask = (1<<(pcprec*4)) - 1;
    }
    strcpy(srcname,argv[optind]);   /* get input file name (with .s first) */
    i = strlen(srcname);
    (void)strcat(srcname, ".s");

    if (infopen(srcname,SPACE,NULL) < 0) {
	srcname[i] = '\0';
	if (infopen(srcname,SPACE,NULL) < 0)
	    error(e_nofile);
    }

    if (src == NULL) {
	perror(srcname);
	printf("No source\n");
	exit(1);
    }

    src->if_type = ' ';			/* mark as main file */
    mainsrc = src;

    if (*dstname == '\0') {
	(void)strcpy(dstname,srcname);
	i = strlen(dstname) - 2;

	if ((dstname[i] == '.') && (toupper(dstname[i+1]) == 'S')) {
	    dstname[i+1] = 'o';
	} else {
	    (void)strcat(dstname, ".o");
	}
    }

    if ((outf = fopen (dstname,"w")) == NULL) {
	perror (dstname);
	printf("Can't open output file: %s\n",dstname);
	exit(1);
    }

#if !HIF
    if ((ttyout = isatty (1)) != 0) {
#ifdef OLD
	setbuf (stdout,e_alloc (1,512));
#else                           /* CHANGE 8/21/88               */
	setbuf (stdout,e_alloc (1,BUFSIZ));
#endif
    }
#endif

    define_now();               /* time/date stamp */

    if (silent) {
	header[0] = EOS;
	if (list == FALSE)
	    omit_warn = TRUE;
    } else {
	for (fcp = cp = srcname; *cp != EOS; cp++) {
	    if (dir_sep(*cp))
		fcp = cp+1;
	}
	if (*fcp == EOS)
	    fcp = "?";

	sprintf (header,"%79s",timestr);
	sprintf (header,"%s V%s   %s",DESCR,REVISION,fcp);
	header [strlen (header)] = SPACE;

	printf ("%s\n\n",header);
	fflush (stdout);
    }

/* assembly pass 1              */

    pass = 1;
    pass_init();
    while (parseline())
	doline();
    post_algn();                /* handle post-pass alignment   */

/* calculate address of COFF relocatable data and other info */

/* seek past file hdr + opt info in COFF */
/* to beginning of section headers */

    fpos = FILHSZ + head.f_opthdr;
    fpos_raw = fpos + ((long) nsegs * SCNHSZ);

/* point to begin of raw data in COFF */

    slp = sectlist;

    while (slp != NULL)
    {
	fpos += (long) SCNHSZ;

	if ((slp->sg_head.s_size > 0)
	    && !(slp->sg_head.s_flags & STYP_BSS)) {
		slp->sg_head.s_scnptr = fpos_raw;   /* start of raw data */
		fpos_raw += slp->sg_head.s_size;    /* next segs raw data */
	}
	slp->sg_head.s_size = 0;
	slp = slp->sg_link;
    }

/* get pointers to relocatable data for each section/segment */

    slp = sectlist;

    while (slp != NULL)
    {
	if (slp->sg_head.s_nreloc > 0)
	{
	    slp->sg_head.s_relptr = fpos_raw;
	    fpos_raw += slp->sg_head.s_nreloc*RELSZ;  /* adv past reloc sect */
	    slp->sg_p1nrel = slp->sg_head.s_nreloc;
	    slp->sg_head.s_nreloc = 0;
	}
	slp = slp->sg_link;
    }

/* get pointers to line number data for each section/segment */

    slp = sectlist;

    while (slp != NULL)
    {
	if (slp->sg_head.s_nlnno > 0)
	{
	    slp->sg_head.s_lnnoptr = fpos_raw;
	    fpos_raw += slp->sg_head.s_nlnno * LINESZ;
	    slp->sg_head.s_nlnno = 0;
	}
	slp = slp->sg_link;
    }

    head.f_symptr = fpos_raw;   /* set file-header symbol-table pointer */

/* set symbol indices   */
/* include COFF section symbols in static area, before globals */

    viSegs = ndbsyms;
    dbfiles[ndbfils] = sym_index = ndbsyms + nsegs * 2;

    exp_loc (symtab->sy_rlink); /* export local symbols         */
    set_indices (symtab,F_PUBLIC);
    set_indices (symtab,F_EXTERN | F_UNDEF);
    head.f_nsyms = sym_index;

/* calculate beginning of string table */

    stringptr = fpos_raw + (head.f_nsyms * SYMESZ);
    cur_strgoff =  4;    /* 1st 4 bytes are length */
    fixup_undef (symtab);

/*
 * Assembly pass 3.  Each entry in the symbol table must have an alphabetical
 *   index for COFF.  Those symbols which will be included in the COFF table
 *   are externals, globals, and dummy section entries for relocating local
 *   label references (i.e., the relocatable entries for references to local
 *   labels will point to a dummy symbol which will be resolved to the base
 *   address of that section by the linker).
 */

    pass = 3;
    pass_init();
    while (parseline()) doline();
    post_algn();                /* handle post-pass alignment   */

/* write out the section headers, symbol table, line-numbers, etc. */

/* seek past file hdr + opt info in COFF */
/* to beginning of section headers */

    fpos = FILHSZ + head.f_opthdr;
    slp = sectlist;

    while (slp != NULL)
    {                           /* flush any blank reloc. from pass 1 */
	if (slp->sg_head.s_nreloc < slp->sg_p1nrel)
	{
	    fseek (outf,
		slp->sg_head.s_relptr+slp->sg_head.s_nreloc*RELSZ,
		    0);
	    i = (slp->sg_p1nrel - slp->sg_head.s_nreloc) * RELSZ;
	    while (--i >= 0) putc(0,outf);
	}
	fseek (outf,fpos,0);
#ifdef OLD
	wrhead (&slp->sg_head.s_name[0],outf);
#else
	isp = (struct scnhdr *) &(slp->sg_head.s_name[0]);
	wrhead (isp,outf);
#endif
	slp = slp->sg_link;
	fpos += SCNHSZ;
    }

    fseek (outf, head.f_symptr, 0); /* write out debugger symbols */
    wrdbugsyms (dbugtab->db_link);
    wrsects();
    wrdefs (symtab->sy_rlink);
    wrrefs (symtab->sy_rlink);

    if (cur_strgoff > 4)        /* anything in the string-symbol table? */
    {
	fseek (outf,stringptr,0);
#ifdef SWAP_COFF
	if (target_order != host_order)
	{
	    putswap (cur_strgoff,(char *) &cur_strgoff,sizeof(long));
	}
#endif
	fwrite ((char *) &cur_strgoff,1,4,outf);
    }

    head.f_nscns = nsegs;
    head.f_magic = SIPFBOMAGIC;
    head.f_flags = target_order == LSB ? F_AR32WR : F_AR32W;
    head.f_timdat = time (NULL);
    fseek (outf,0L,0);
    wrfhead (outf);
    (void)inclose(1);

    if (fclose(outf) == EOF) {
	printf("EOF ERR\n");
    }

/*
 * End of assembly -- print the symbol-table and error-counts.
 */

    if (listsym)
    {
	if (pageline)
	{
	    pageline = pglength;    /* force new page for syms */
	}
	prsyms (symtab->sy_rlink);
    }

    err_summ();				/* print error summary          */
    n = errcnt ? 1 : 0;			/* exit code                    */
    return n;				/* done, exit                   */
}
