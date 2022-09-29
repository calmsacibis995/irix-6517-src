/* CODE.C -- CODE-GENERATION SUPPORT FUNCTIONS                  */

/* history:                                                     */

/* n/a       E. Wilner   --  created                            */

/*  8/25/88  RJK         --  rev 0.16                           */
/*  9/12/88  RJK         --  rev 0.17                           */
/* 10/15/88  RJK         --  rev 0.18                           */
/* 11/22/88  RJK         --  rev 0.19                           */
/*  2/28/89  RJK         --  rev 0.21 (1.0e0)                   */

/* ------------------------------------------------------------ */

#include "asm29.h"

#define CBUFSIZE        8192
#define RELBSIZE        400

static char codebuf[CBUFSIZE], *cptr;
static int crem, nrel;
static struct reloc relbuf[RELBSIZE];

static void genval(struct value*);


/* ------------------------------------------------------------ */

/*
 * Clear the object-code buffer.
 */

/* ------------------------------------------------------------ */

void
initbuf(void)
{
    cptr = codebuf;
    crem = CBUFSIZE;
    nrel = 0;
}

/* ------------------------------------------------------------ */

/*
 * Output the object-code and relocatable buffers to the COFF file and
 *   update the count of relocatable items.
 */

/* ------------------------------------------------------------ */

flushbuf()
{
    long save;
    int j;

    if ((cptr > codebuf) && (pass == 3))
    {
	if (crem < 0) crem = 0;
	fwrite (codebuf,1,CBUFSIZE-crem,outf);
    }

    if (nrel)
    {
	if (pass == 3)
	{
	    save = ftell (outf);

	    fseek (outf, (cursegp->sg_head.s_relptr +
		cursegp->sg_head.s_nreloc*RELSZ), 0);

	    for (j = 0; j < nrel; j++)
	    {
		fwrite ((char *) &relbuf[j],1,RELSZ,outf);
	    }
	    fseek (outf,save,0);
	}
	cursegp->sg_head.s_nreloc += nrel;
    }

    initbuf();
}

/* ------------------------------------------------------------ */

/* "cs_flags(mask)"  returns  true  (nonzero)  if  the  current */
/* segment's STYP_XXXX flag bits intersect the bits in  "mask", */
/* and false (zero) otherwise.                                  */

/* ------------------------------------------------------------ */

int cs_flags(mask)
int mask;                       /* segment flag bit mask        */
{
    long l_flags;               /* flag bits found              */
    long l_mask;                /* mask (as a long value)       */

    if (cursegp == NULL) panic ("cs_flags");
    l_mask  = ((long) (unsigned) mask) & 0xFFFF;
    l_flags = (cursegp->sg_head.s_flags) & l_mask;
    return (l_flags ? TRUE : FALSE);
}

/* ------------------------------------------------------------ */

/*
 * "End" a segment by emptying the object & relocatable buffers, saving the
 * current program counter in the segment structure, and padding the section
 * to the next word boundary.
 */

/* ------------------------------------------------------------ */

void
endseg(int alignit)
{
    int i;
    long lx;
    int nb;

    if (cursegp != NULL) {
	    nb  = (int) (4L - (pc & 0x03));

	    if (alignit && (nb < 4) && !cs_flags (no_align)) {
		    for (i = 0; i < nb; i++)
			    genbyte(0);
	    }

	    if (!cs_flags (STYP_DSECT)) {
		    lx = pc - cursegp->sg_head.s_paddr;
		    cursegp->sg_head.s_size = lx;
	    }
    }

    flushbuf();
    cursegp = NULL;
}

/* ------------------------------------------------------------ */

/* "algn_tree()" is a support function for "algn_data()".       */

/* ------------------------------------------------------------ */

static void
algn_tree(struct sym *symp)
{
    if (symp == NULL) return;
    algn_tree (symp->sy_llink);
    use_ps (symp);
    if (cs_flags (STYP_DATA)) endseg (1);
    algn_tree (symp->sy_rlink);
}


/* aligns all data segments.                      */
static void
algn_data(void)
{
    endseg (1);                 /* end current segment          */
    no_align &= ~STYP_DATA;
    algn_tree (segtab->sy_rlink);
    no_align |= STYP_DATA;
    endseg (1);                 /* end current segment          */
}

/* ------------------------------------------------------------ */

/* "post_algn()"  handles the segment  wrapup and/or  alignment */
/* operations required after each pass.                         */

/* ------------------------------------------------------------ */

post_algn()
{
    endseg (1);                 /* end current segment          */
    algn_data();                /* align data segments          */
}

/* ------------------------------------------------------------ */

/*
 * Write a byte, halfword (2 bytes), or fullword (4 bytes) to the object-code
 *   buffer, and flush the buffer when full.
 */
void
genbyte(int b)
{
	if (curseg == DSECT)                    /* don't write to dummy section ! */
		{
		error (e_dsect);
		return;
		}
	/* listing the PC is interesting */
	listpc = 1;
	listovr = 1;
	if (--crem < 0)
		{
		flushbuf();
		crem--;
		}
	*cptr++ = b;
	pc++;
}

genhword (h)
int h;
{
	genbyte (h>>8);
	genbyte (h);
}

genword (w)
long w;
{
	genhword ((int)(w>>16));
	genhword ((int)w);
}

/*
 * Align the PC to a specified byte boundary (pad output with zeros and
 *   advance the PC).
 */

void
align(int n)
{
    while (pc % n)
    {
	genbyte (0);
	lpc++;
    }
}

/*
 * Check that the PC is aligned to a specified boundary (error if not!).
 */
void
ck_align(int n)
{
	if (pc % n)
		error(e_noalign);
}

/*
 * Write a COFF relocatable item to the relocatable buffer, and output the
 *   buffer if it is full.
 */
void
genrel(long index,			/* item index in COFF symbol table */
       char type)			/* COFF relocatable type */
{
	struct reloc *rp;

/* >>><<< assume relocation type assigned  -- no default */

	rp = &relbuf[nrel++];
	rp->r_vaddr  = pc;
	rp->r_symndx = index;
	rp->r_type   = type;

#ifdef SWAP_COFF
	if (target_order != host_order) {
		/* target here is processing machine */
		putswap(rp->r_vaddr, &rp->r_vaddr, sizeof (long));
		putswap(rp->r_symndx, &rp->r_symndx, sizeof (long));
		putswap((long)rp->r_type, &rp->r_type, sizeof (short));
	}
#endif

	if (nrel == RELBSIZE)
		flushbuf();
}



void
genvrel(struct value *vp)		/* value pointer                */
{
	struct scnhdr *shp;		/* section header pointer       */
	int sn;				/* section-number referenced    */

	if (vp->v_ext) {		/* external reference? */
		/* yes, generate relocation     */
		genrel(vp->v_ext->sy_index,vp->v_reloc);
		return;
	}

/* local reference?             */

	sn = vp->v_seg;
	if ((sn == 0) || (vp->v_reloc == 0))
		return;

/* The value references a local label.  The reference will be resolved
 * through the use of a dummy section symbol located at the section
 * base address.*/

/* get reference section header */

	if (sn == curseg) {		/* used in current segment? */
		shp = &(cursegp->sg_head);  /* yes -- already have header   */
	} else if (sn > 0) {
		shp = num_hdr(sn);	/* get header using section #   */
	} else {
		shp = 0;
	}

/* suppress absolute relocation */

	if ((shp != NULL) && (shp->s_flags & STYP_ABS))
		return;

/* generate special relocation  */

	genrel (((sn-1) * 2) + viSegs,vp->v_reloc);
}

/* ------------------------------------------------------------ */

/*
 * Write relocatable items and object-code from a specified value structure.
 *   The object code is saved in a buffer for the listing routine.
 */
static void
genval(struct value *v)
{
#ifdef OLD
	if (v->v_ext) {			/* an external only */
		genrel (v->v_ext->sy_index, v->v_reloc);
	} else if ((v->v_seg) && (v->v_reloc)) {
		/* a local label that will be resolved via a
		 * dummy section symbol at base address */
		genrel ((v->v_seg * 2) - 2 + viSegs, v->v_reloc);
	}
#else
	genvrel(v);
#endif

	switch (v->v_flags & VF_SIZE) {
	case VF_BYTE:
		genbyte ((int)v->v_val);
		break;
	case VF_HWORD:
		genhword ((int)v->v_val);
		break;
	case VF_WORD:
		genword(v->v_val);
	}
	if (lwords < 50)
		lcode[lwords++] = *v;
}

/*
 * Set the byte, halfword, or fullword flag in a specified value structure and
 *   then write out relocatable items and object code.
 */
void
genvbyte(struct value *v)
{
	v->v_flags = (v->v_flags & ~VF_SIZE) | VF_BYTE;
	genval (v);
}

void
genvhword(struct value *v)
{
	v->v_flags = (v->v_flags & ~VF_SIZE) | VF_HWORD;
	genval (v);
}

void
genvword(struct value *v)
{
	v->v_flags = (v->v_flags & ~VF_SIZE) | VF_WORD;
	genval (v);
}

/*
 * Generate a relocatable item of type R_BYTE, for instructions containing
 *   multiple relocatable items.
 */

genbrel(vp, vad)
struct value *vp;    /* pointer to value structure */
long vad;            /* virtual address of relocatable byte */
{
    long save;

    save = pc;
    pc = vad;

    if (vp->v_ext)
    {
	genrel (vp->v_ext->sy_index,R_BYTE);
	pr_reloc = R_EXT;
    }
    else
    {
	if (vp->v_seg)
	{
	    genrel (vp->v_seg * 2 - 2 + viSegs,R_BYTE);
	    pr_reloc = R_SEG;
	}
    }

    pc = save;
}

/*
 * Convert an internal symbol-table entry to COFF symbol-table format and
 *   write it to the appropriate place in the COFF file, along with the
 *   string-table entry if needed.
 */

wrsym(ps)
struct sym *ps;
{
    int i;
    long save;

#ifdef SWAP_COFF
    if (target_order != host_order)
    {
	putswap (ps->se.n_value, (char *)&s.n_value, sizeof (long));
	putswap ((long)ps->se.n_scnum, (char *)&s.n_scnum, sizeof (short));
	putswap ((long)ps->se.n_type, (char *)&s.n_type, sizeof (short));
    }
#endif

    if ((i = strlen (ps->sy_name)) > 8)
    {
	ps->se.n_zeroes = 0;
	ps->se.n_offset = cur_strgoff;

#ifdef SWAP_COFF
	if (target_order != host_order)
	{
	    putswap (cur_strgoff, (char *)&s.n_offset, sizeof (long));
	}
#endif

	save = ftell (outf);
	fseek (outf,stringptr + cur_strgoff,0);
	fwrite (ps->sy_name, 1, i+1, outf);
	fseek (outf,save,0);
	cur_strgoff += i+1;
    }
    fwrite ((char *) &ps->se,1,SYMESZ,outf);
}

/* ------------------------------------------------------------ */

/* export local symbols         */

exp_loc(ps)
struct sym *ps;
{
    int c,d;                    /* characters from symbol name  */
    char *cp;                   /* scratch                      */
    int lf;                     /* "L*"-symbol flag             */
    long lx;                    /* flag bits                    */
    long sf;                    /* symbol flags                 */

    if ((ps == NULL) || (pass != 1)) return;
    exp_loc (ps->sy_llink);
    sf = ps->sy_flags;

    if (sf & F_LOCSYM)
    {
	cp = ps->sy_name;
	c  = cp[0];
	d  = cp[1];

	lf = (c == 'L') || ((c == '.') && (d == 'L'));
	lx = sf & (F_EXTERN | F_MULDEF | F_SETSYM | F_UNDEF);

	if ((lx == 0) && (lcflag || !lf) && !ps->se.n_sclass)
	{                       /* export the symbol            */
	    ps->sy_flags |= F_PUBLIC;
	    ps->se.n_sclass = C_STAT;
	}
    }

    exp_loc (ps->sy_rlink);
}

/* ------------------------------------------------------------ */

/*
 * Write out global symbols to the COFF file in alphabetical order.
 */

wrdefs(st)
struct sym *st; /* ptr to start of internal symbol table */
{
    if (st == NULL) return;
    wrdefs (st->sy_llink);

    if ((st->sy_flags & F_PUBLIC) && (st->sy_index >= ndbsyms))
    {
	wrsym (st);
    }

    wrdefs (st->sy_rlink);
}

/*
 * Write all external symbols to the DCOFF file.
 */

wrrefs(st)
struct sym *st;
{
    if (st == NULL) return;
    wrrefs (st->sy_llink);

    if ((st->sy_flags & (F_EXTERN | F_UNDEF)) && (st->sy_index >= ndbsyms))
    {
	wrsym (st);
    }

    wrrefs (st->sy_rlink);
}

/*
 * Write all section symbols to the COFF file (with auxiliary entries).
 */

wrsects()
{
    int i, j = 1;
    struct syment coffsym;
    union auxent asym;
    struct seglist *sp;

    sp = sectlist;

    while (sp != NULL)
    {
	zap(&coffsym);
	zap(&asym);

	for (i = 0; i < S_LEN; i++)
	{
	    if ((coffsym.n_name[i] = sp->sg_head.s_name[i]) == '\0')
		break;
	}

	coffsym.n_numaux = 1;
	coffsym.n_sclass = C_STAT;
	coffsym.n_scnum = j;
	asym.xx_nreloc = sp->sg_head.s_nreloc;
	asym.xx_nlinno = sp->sg_head.s_nlnno;
	asym.xx_scnlen = sp->sg_head.s_size;

#ifdef SWAP_COFF
	if (target_order != host_order)
	{
	    putswap ((long)j, (char *)&coffsym.n_scnum, sizeof (short));
	    putswap (sp->sg_head.s_size, (char *)&asym.xx_scnlen,
		sizeof (long));
	    putswap ((long)sp->sg_head.s_nlnno, (char *)&asym.xx_nlinno,
		sizeof (short));
	    putswap ((long)sp->sg_head.s_nreloc, (char *)&asym.xx_nreloc,
		sizeof (short));
	}
#endif

	fwrite ((char *)&coffsym, 1, SYMESZ, outf);
	j++;
	fwrite ((char *)&asym, 1, SYMESZ, outf);
	sp = sp->sg_link;
    }
}

/*
 * Write a section header to the COFF file, swapping bytes if requested.
 */

wrhead(isp,fp)
struct scnhdr *isp;
FILE *fp;
{
#ifdef SWAP_COFF
    if (target_order == host_order)
#endif
    {
	fwrite ((char *) isp,1,SCNHSZ,fp);
	return;
    }

#ifdef SWAP_COFF
    putswap (isp->s_paddr, (char *)&osp.s_paddr, sizeof (long));
    putswap (isp->s_vaddr, (char *)&osp.s_vaddr, sizeof (long));
    putswap (isp->s_size, (char *)&osp.s_size, sizeof (long));
    putswap (isp->s_scnptr, (char *)&osp.s_scnptr, sizeof (long));
    putswap (isp->s_relptr, (char *)&osp.s_relptr, sizeof (long));
    putswap (isp->s_lnnoptr, (char *)&osp.s_lnnoptr, sizeof (long));
    putswap (isp->s_flags, (char *)&osp.s_flags, sizeof (long));
    putswap ((long)isp->s_nreloc, (char *)&osp.s_nreloc, sizeof (short));
    putswap ((long)isp->s_nlnno, (char *)&osp.s_nlnno, sizeof (short));

    for (i = 0; i < S_LEN; i++)
    {
	osp.s_name[i] = isp->s_name[i];
    }

    fwrite ((char *) &osp,1,SCNHSZ,fp);
#endif
}

/*
 * Write a line-number entry to the COFF file.
 */

wrline(ilp,fp)
struct lineno *ilp;
FILE *fp;
{
#ifdef SWAP_COFF
    if (target_order == host_order)
#endif
    {
	fwrite ((char *) ilp,1,LINESZ,fp);
	return;
    }

#ifdef SWAP_COFF
    putswap (ilp->l_addr.l_symndx, (char *)&olp.l_addr.l_symndx, sizeof(long));
    putswap ((long)ilp->l_lnno, (char *)&olp.l_lnno, sizeof (short));
    fwrite ((char *)&olp, 1, LINESZ, fp);
#endif
}

/*
 * Write the COFF object-file header.
 */

wrfhead(fp)
FILE *fp;
{
#ifdef SWAP_COFF
    struct filehdr ohead;

    if (target_order == host_order)
#endif
    {
	fwrite ((char *) &head,1,FILHSZ,fp);
	return;
    }

#ifdef SWAP_COFF
    putswap ((long)head.f_magic, (char *)&ohead.f_magic, sizeof (short));
    putswap ((long)head.f_nscns, (char *)&ohead.f_nscns, sizeof (short));
    putswap (head.f_timdat, (char *)&ohead.f_timdat, sizeof (long));
    putswap (head.f_symptr, (char *)&ohead.f_symptr, sizeof (long));
    putswap (head.f_nsyms, (char *)&ohead.f_nsyms, sizeof (long));
    putswap ((long)head.f_opthdr, (char *)&ohead.f_opthdr, sizeof (short));
    putswap ((long)head.f_flags, (char *)&ohead.f_flags, sizeof (short));
    fwrite ((char *) &ohead,1,FILHSZ,fp);
#endif
}
