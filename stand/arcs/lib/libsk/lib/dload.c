#if _MIPS_SIM != _ABI64			/* 64-bit stuff does not use COFF */
/*
 * dload.c
 *
 * 	This code reads and relocates an executable from a file.
 *      It is intended to work with a stand-alone system, so
 *      it only seeks forward through the file, and only reads once.
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.30 $"

/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1989 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */
/*
	doload.c - dynamic loader for class system

	Author:  Zalman Stern July 1989
 */


#include <sys/types.h>
#include <aouthdr.h>
#include <filehdr.h>
#include <reloc.h>
#include <scnhdr.h>
#include <sym.h>
#include <symconst.h>
#include <setjmp.h>

#include <arcs/dload.h>
#include <arcs/types.h>
#include <arcs/io.h>
#include <arcs/errno.h>
#include <libsc.h>
#include <libsk.h>

/*#define	DEBUG	*/

#define STYPof(x) ((x) & ~S_NRELOC_OVFL)

extern int swapendian;

#ifdef HANDLEGP
extern int _gp ; /* The gp is a pointer to this variable. */
#endif

#if !defined MIPSEB && !defined MIPSEL
	****** ERROR -- MUST BE ONE OR THE OTHER !!
#endif /* !defined MIPSEB && !defined MIPSEL */

#if defined MIPSEB
#define HighShortOf( longIntPtr ) ( ( (short int *)( longIntPtr ) )[0] ) 
#define LowShortOf( longIntPtr ) ( ( (short int *)( longIntPtr ) )[1] )
#else /* !defined MIPSEL */
#define HighShortOf( longIntPtr ) ( ( (short int *)( longIntPtr ) )[1] )
#define LowShortOf( longIntPtr ) ( ( (short int *)( longIntPtr ) )[0] )
#endif

#if defined MIPSEB
#define UHighShortOf(longIntPtr)(((unsigned short int *)(longIntPtr))[0])
#define ULowShortOf(longIntPtr)(((unsigned short int *)(longIntPtr))[1])
#else /* !defined MIPSEL */
#define UHighShortOf(longIntPtr)(((unsigned short int *)(longIntPtr))[1])
#define ULowShortOf(longIntPtr)(((unsigned short int *)(longIntPtr))[0])
#endif


/*
   These variables allow us to index into the right e->section
   and e->contents, without assuming much about the order of
   stuff or what's there.
*/

static short DLn_text;
static short DLn_rdata;
static short DLn_data;
static short DLn_bss;

/* Forward declaration */
static void doload_punt(struct doload_environment *, int);
static char *safe_malloc(struct doload_environment *, long);
static void safe_read(struct doload_environment *, char *, long);
static void prt_class(int);
static void prt_lderr(int);
static void prt_section(int);

/* 
 * doload_cleanup - Tear down environment. 
 */

static void
doload_cleanup(register struct doload_environment *e)
{
	if ( e->problems > 0 ) {
		e->problems = 0 ;
		doload_punt( e, DLerr_Prob ) ;
		/*NOTREACHED*/
	}
	if ((void *)e->rtab != NULL)
		dmabuf_free(e->rtab);
	if ((void *)e->symtab != NULL)
		dmabuf_free(e->symtab);
	if ((void *)e->contents != NULL)
		dmabuf_free(e->contents);
	if ((void *)e->stringtab != NULL)
		dmabuf_free(e->stringtab);
	return ;
}

/*
 * read_section - Read section from object file.
 */

static char *
read_section(
	struct doload_environment *e,
	long offset,
	char *buffer,
	long length )
{
	LARGE off;

	longtolarge (offset, &off);
	if (Seek (e->fd, &off, SeekAbsolute)) {
		doload_punt( e, DLerr_BadSeek ) ;
		/*NOTREACHED*/
	}
	safe_read( e, buffer, length ) ;
	return buffer ;
}


/* 
 * doload_map - Map module into memory. 
 */

static void
doload_map(register struct doload_environment *e)
{
	register int offset ;
	register int size ;
	register int scn ;
	register unsigned short nscns ;
	register char *curtextaddr ;
	register char *curdataaddr ;

	nscns = e->filehdr->f_nscns;
	curtextaddr = e->text;
	curdataaddr = e->data;
	
	/* allocate space for the contents pointers */
	e->contents = (void **) 
	  safe_malloc(e, (long)sizeof(void *)*nscns);
	
	for (scn=0; (scn < nscns); scn++) {
	  offset = e->sections[scn].s_scnptr ;
	  size = e->sections[scn].s_size ;

	  switch(STYPof(e->sections[scn].s_flags)) {

	  case STYP_TEXT:
	    if (Verbose)
	      printf ("%d+", size);
	    DLn_text = scn;
	    curtextaddr = 
	      read_section(e,offset, curtextaddr, size);
	    e->contents[scn] = curtextaddr;
	    curtextaddr += size;
	    break;
	  
	  case STYP_RDATA:
	    if (Verbose)
	      printf ("%d+", size);
	    DLn_rdata = scn;
	    curdataaddr = 
	      read_section(e, offset, curdataaddr, size);
	    e->contents[scn] = curdataaddr;
	    curdataaddr += size;
	    break;

	  case STYP_DATA:
	    if (Verbose)
	      printf ("%d+", size);
	    DLn_data = scn;
	    curdataaddr = 
	      read_section(e, offset, curdataaddr, size);
	    e->contents[scn] = curdataaddr;
	    curdataaddr += size;
	    break;

	  case STYP_BSS:
	    if (Verbose)
	      printf ("%d+", size);
	    DLn_bss = scn;
	    bzero( curdataaddr, size) ;
	    e->contents[scn] = curdataaddr;
	    curdataaddr += size;
	    break;

	  default:
	    printf ("\nUnexpected section \"");
	    prt_section (STYPof(e->sections[scn].s_flags));
	    printf ("\"\n");
	    doload_punt(e, DLerr_RogueScn);
	    /* NOT REACHED */
	  } /* switch */
	}

	return ;
}

/*
TODO put DLn_* into environment struct
*/

/* 
 * doload_read - This routine reads the actual section contents 
 * into memory. When we enter it, the fileheader, optional headers, 
 * and section headers have been read.
 */

static void
doload_read( register struct doload_environment *e )
{
  register long count ;
  register int scn ;
  register long int totalRelocs ;
  pHDRR symheader;

  /* Insist on TEXT, it must be first!! */

  if ( strcmp( e->sections[0].s_name, _TEXT )
      || e->sections[0].s_size == 0 ) {
    doload_punt( e, DLerr_NoText ) ;
    /*NOTREACHED*/
  }

  doload_map( e ) ;

  totalRelocs = 0;
  for (scn=0; (scn < e->filehdr->f_nscns); scn++) {
    totalRelocs += e->sections[scn].s_nreloc;
  }
	
#ifdef	DEBUG
  printf ("doload_read: total relocs=%d\n", totalRelocs);
#endif	/*DEBUG*/

  if ( totalRelocs ) {
    e->rtab = (struct reloc *)
	safe_malloc(e, totalRelocs * sizeof (struct reloc));
    if (Verbose)
	printf ("%d", totalRelocs * sizeof (struct reloc));
    (void)read_section( e, e->sections[0].s_relptr, (char *)e->rtab,
		   totalRelocs * sizeof (struct reloc) ) ;
  }

  /* Don't bother with the symbol table of a stripped binary
   */
  if (e->filehdr->f_symptr && e->filehdr->f_nsyms) {
    /* Get the symbolic header */
    symheader = (pHDRR)safe_malloc(e, sizeof(HDRR));
    if (Verbose)
  	printf ("%+d+", sizeof(HDRR));
    (void) read_section( e, e->filehdr->f_symptr, (char *)symheader,
  		      sizeof(HDRR) ) ;
    bcopy((void *)symheader, (void *)&e->symheader, sizeof(HDRR));
  
    /* Swap symheader entries if loading an opposite endian file */
    if (swapendian) {
  	e->symheader.iextMax = nuxi_l (e->symheader.iextMax);
  	e->symheader.issExtMax = nuxi_l (e->symheader.issExtMax);
  	e->symheader.cbSsExtOffset = nuxi_l (e->symheader.cbSsExtOffset);
  	e->symheader.cbExtOffset = nuxi_l (e->symheader.cbExtOffset);
    }
  
    /* Get scratch space for symbol split fix-ups */
    count = e->symheader.iextMax ;
    if ( count ) {
      count *= ( sizeof (EXTR) + ( 2 * sizeof (short) ) ) ;
      e->symtab = (pEXTR) safe_malloc( e, count ) ;
      e->stringtab = (char *) safe_malloc( e, e->symheader.issExtMax ); 
      
      if (Verbose)
  	printf ("%d+", e->symheader.issExtMax);
      /* Read the external strings */
      (void) read_section( e, e->symheader.cbSsExtOffset, (char *) e->stringtab,
  			e->symheader.issExtMax) ;
  
      if (Verbose)
  	printf ("%d", e->symheader.iextMax * sizeof (EXTR));
      /* Read the symbol table's symbols */
      (void) read_section( e, e->symheader.cbExtOffset, (char *) e->symtab,
  			e->symheader.iextMax * sizeof (EXTR) ) ;
    }
  }

  return ;
}


/* relocate one item */

#define HI16MASK    0xFFFF0000
#define LOW16MASK   0x0000FFFF
#define LOW26MASK   0x03FFFFFF
#define HI6MASK     0xFC000000
#define HI4MASK     0xF0000000
#define LO2MASK     0x00000003

#ifndef ORIGINAL
static int foundSC[100];
#endif

/*
 * doload_relocate - Relocate symbols.
 */

static void
doload_relocate(register struct doload_environment *e,
		register char *cp,
		register struct reloc *rp,
		struct reloc *sectionRelocBound )
		/* Used for bounds checking below. */
{
	register long tw ;
	register long disp  = 0; /* "displacement" : how much it moved ! */
	register long original_tw ;

#ifdef	VERBOSE
	if (Verbose > 2)
	printf ("doload_relocate: rp=0x%x\n", rp);
#endif	/*VERBOSE*/

	if ( rp->r_extern ) {

	  /* If binary has been stripped, there's no chance of locating
	   * the external symbol so bail out.
	   */
	  if (!e->symtab) {
	    printf ("\nUndefined external symbol, binary is stripped.\n");
	    doload_punt(e,DLerr_UndefinedSym);
	  }

	  /* All references to external symbols have not been 
           * resolved yet.  Instead, they are assigned the address
	   * 0x0.  So we have to add the current value into
	   * the displacement.
	   */
	  disp = e->symtab[ rp->r_symndx ].asym.value;
	  switch (e->symtab[rp->r_symndx ].asym.sc) {

	  case scNil:
	    disp = 0 ;
	    break ; /* an ABS gp symbol ? */

	  case scText:
	    disp += (long) e->contents[ DLn_text ]
	      - e->sections[ DLn_text ].s_vaddr ;
	    break ;

	  case scRData :
	    disp += (long) e->contents[ DLn_rdata ]
	      - e->sections[ DLn_rdata ].s_vaddr ;
	    break ;
	    
	  case scData :
	    disp += (long) e->contents[ DLn_data]
	      - e->sections[ DLn_data ].s_vaddr ;
	    break ;

	  case scBss :
	    disp += (long) e->contents[ DLn_bss ]
	      - e->sections[ DLn_bss ].s_vaddr ;
	    break ;

	  case scSData :
	  case scSBss :
#ifdef HANDLEGP
	    disp = (long) e->contents[ e->firstGpSection ]
	      - e->sections[ e->firstGpSection ].s_vaddr ;
	    break ;
#else
	    printf ("\nUnexpected storage class scSData or scSBss\n");
	    doload_punt(e,DLerr_BadExtSC);
#endif
	  case scAbs:
	    /* These we don't have to do anything with */
	    break;

	  case scUndefined:
	    printf ("\nUndefined external symbol \"%s\"\n",
	      (char *)(e->stringtab + e->symtab[rp->r_symndx].asym.iss));
	      doload_punt(e,DLerr_UndefinedSym);
	    break;

	  default:
	    if (! foundSC[e->symtab[rp->r_symndx ].asym.sc]) {
	      printf ("\nUnexpected storage class ");
	      prt_class (e->symtab[rp->r_symndx ].asym.sc);
	      printf (" \n");
	      doload_punt(e,DLerr_BadExtSC);
	    }
	    break;
	  }
	}
	else {
	  /* Either fixup to the patched gp section or the rest of it */
	  switch ( rp->r_symndx ) {

	  case R_SN_NULL :
	    disp = 0 ;
	    break ; /* an ABS gp symbol ? */

	  case R_SN_TEXT :
	    disp = (long) e->contents[ DLn_text ]
	      - e->sections[ DLn_text ].s_vaddr ;
	    break ;

	  case R_SN_RDATA :
	    disp = (long) e->contents[ DLn_rdata ]
	      - e->sections[ DLn_rdata ].s_vaddr ;
	    break ;

	  case R_SN_DATA :
	    disp = (long) e->contents[ DLn_data ]
	      - e->sections[ DLn_data ].s_vaddr ;
	    break ;

	  case R_SN_BSS :
	    disp = (long) e->contents[ DLn_bss ]
	      - e->sections[ DLn_bss ].s_vaddr ;
	    break ;

	  case R_SN_INIT :
	    doload_punt(e,DLerr_HasInit);
	    /* NOT REACHED */

	  case R_SN_SDATA :
	  case R_SN_SBSS :
	  case R_SN_LIT8 :
	  case R_SN_LIT4 :
#ifdef HANDLEGP
	    disp = (long) e->contents[ e->firstGpSection ]
	      - e->sections[ e->firstGpSection ].s_vaddr ;
	    break ;
#else
	    doload_punt(e,DLerr_HasGP);
	    /* NOT REACHED */
#endif
	  }
	}

#ifdef HANDLEGP
	if ( rp->r_type == R_GPREL || rp->r_type == R_LITERAL )
		disp += e->aouthdr->gp_value - (long) &_gp ;
#endif

	if ( !disp ) { /* No change ( we mmap'ed at a "good" address ) */
		return ;
	}

	switch ( rp->r_type ) {

	case R_REFHALF:
		* (short *) cp += disp ;
		break ;

	case R_REFWORD:
		* (long *) cp += disp ;
		break ;

	case R_JMPADDR:
		original_tw = * (long *) cp ;
		tw = ( original_tw & LOW26MASK ) << 2 ;
		if (!rp->r_extern) {
		  /* This is a local reference, which means
		   * that the top nibble of the unrelocated
		   * address is taken from the pc; in this case
		   * that's rp->r_vaddr - disp, since we assume
		   * that the jump instruction has moved exactly
		   * as far as the jump target. Externals are relocated
		   * from address 0x0, so disp has the right high
		   * nibble for them.
		   */
		  tw |= (rp->r_vaddr - disp) & HI4MASK;
		}
		/* At this point tw contains the old 32-bit address.*/
		tw += disp ;
		/* The REAL restriction on mips jumps is that both
		 * the target of the instruction and the instruction
		 * itself must lie within the same 28-bit segment.
		 * i.e. They must have the same high order nibble !
		 * This is significantly worse than having a +/- 2^27
		 * bit range.
		 */

		/* At this point, tw is the real jump address
		 * and rp->r_vaddr is the address where we are
		 * jumping from.  All we need check is
		 * that 1) They have the same high-order nibble,
		 * and 2) the bottom two bits are zero.
		 */

		if ((tw & HI4MASK)!=((rp->r_vaddr+4) & HI4MASK)
		    || ((tw & LO2MASK) != 0)){
			doload_punt( e, DLerr_BadJUMPADDR);
			/*NOTREACHED*/
		}
		/* We get the high nibble from the PC which we just
		 * checked.
		 */

		tw = (tw & ~HI4MASK) >> 2 ;
		tw |= original_tw & HI6MASK ;
		* (long *) cp = tw ;
		break ;

	case R_REFHI: {
	  short HighResult;

	  if ( rp >= sectionRelocBound
	      || ( rp + 1 )->r_type != R_REFLO ) {
	    doload_punt( e, DLerr_REFLOSplit);
	    /*NOTREACHED*/
	  }

	  tw = ULowShortOf( rp->r_vaddr ) ;
	  
	  /* Now get the bottom part */
	  tw <<= 16 ;
	  tw += LowShortOf( ( rp + 1 )->r_vaddr ) ;

	  tw += disp ;

	  if ( tw & 0x8000 ) {
            /* offset is negative */
            HighResult = (tw >> 16) + 1;
          } else {
            HighResult = (tw >> 16);
          }

	  ULowShortOf( rp->r_vaddr ) = HighResult ;

	  break;
	}

/* Interestingly enough, it is not necessary to know what the
 * corresponding REFHI was in order to calculate the relocated
 * value for a REFLO.  Let us suppose that the old address
 * is represented by a = a1*2^16 + a0 and the
 * displacement by d = d1*2^16 + d0.  Then the new address is
 *   
 *    a + d = (a1+d1)*2^16 + a0+d0.
 *
 * The lower 16 bits of a+d have the value (a0 + d0) mod 2^16
 * and the high 16 bits of a+d have the value 
 * (a1 + d1 + (a0 + d0) div 2^16).
 * 
 * So we see that the value of the REFHI depends on 
 * the value of the REFLO, due to the (a0 + d0) div 2^16 term,
 * but the value of the REFLO may be calculated without
 * reference to the REFHI.  As long as we have calculated
 * the correct displacement, it works.
 */

	case R_REFLO:
	  tw = LowShortOf( rp->r_vaddr );
	  tw += disp;
	  ULowShortOf( rp->r_vaddr ) = (unsigned short) tw;

	  break ;

	case R_GPREL:
	case R_LITERAL:
#ifdef HANDLEGP
		{
			original_tw = tw = LowShortOf( rp->r_vaddr ) ;

			tw += disp ;
			if ( tw > 32767 || tw < -32768 ) {
				doload_punt( e, DLerr_BadGpReloc);
				/*NOTREACHED*/
			}
			LowShortOf( rp->r_vaddr ) = tw ;
		}
		break ;
#else
		doload_punt( e, DLerr_HasGpReloc);
		break;
#endif
	}

	return ;
}

/* 
 * doload_dorelocs - Adjust virtual addresses in relocation records 
 * to be zeroed where the text segment was actually loaded into 
 * this process' VM.
 */
static void
doload_dorelocs( register struct doload_environment *e )
{
	register struct reloc *rp ;
	register struct scnhdr *tmpSection ;
	register long int sectionOffset ;
	register int count ;
	register struct reloc *startRp ;

	rp = e->rtab ;
	tmpSection = e->sections ;
	for ( count=0 ; count < e->filehdr->f_nscns ; count++, tmpSection++ ) {

		startRp = rp ;
		sectionOffset = tmpSection->s_vaddr
				   - (long) e->contents[count] ;
		if ( sectionOffset )
			for ( ; rp < startRp + tmpSection->s_nreloc ; rp++ )
				rp->r_vaddr -= sectionOffset ;

		for ( rp = startRp ;
		      rp < startRp + tmpSection->s_nreloc ;
		      rp++ ) {
			doload_relocate( e, (char *)rp->r_vaddr, rp,
			     startRp + tmpSection->s_nreloc ) ;
		}
	}

	FlushAllCaches();

	/* all done */
	return ;
}

/*
 * dload - Read and relocate module. Return the entry point or 
 * NULL if error.
 */
void (* dload(
	       int inFD,	/* open fd for package file */
	       void *textaddr, /* address to map text */
	       void *dataaddr, /* address to map data */
	       struct execinfo *ei,  /* pointer to buffer containing headers */
	       int *status     /* error return code */
))()
{
	struct doload_environment E ;

	/* set up environment */
	bzero( &E, (int)sizeof(E)) ;
	E.runTimeGnum = 0 ; /* equivalent to ld -G 0 */
	E.fd = inFD ;
	E.text = textaddr;
	E.data = dataaddr;

	/* set up headers in environment */
	E.filehdr = &(ei->fh);
	E.aouthdr = &(ei->ah);
	E.sections = ei->sh;

	if ( setjmp( E.errorJump ) ) {
		*status = E.status;
		doload_cleanup( &E ) ;
		return NULL ;
	}

	/* read sections into memory */
	doload_read( &E ) ;

	/* do relocation */
	doload_dorelocs( &E ) ;

	/* all done */
	doload_cleanup( &E ) ;

	*status = 0;

	return (void (*)())
		( (long int) E.text + (long int)E.aouthdr->entry 
		 - (long int)E.aouthdr->text_start ) ;
}


static void doload_punt(struct doload_environment *e, int message)
{
  printf ("Relocation error: \"");
  prt_lderr (message);
  printf ("\" \n");
  e->status = message;
  longjmp(e->errorJump, 1);
  /*NOTREACHED*/
}

/*
 * we use dmabuf_malloc() instead of malloc() because most of the allocated
 * buffers will be used for reading through DMA
 */
static char *safe_malloc(struct doload_environment *e, long size)
{
    char *result;
    
    result = (char *)dmabuf_malloc((size_t)size);
    if (result == NULL)
	doload_punt(e, DLerr_OutOfMem);
    return result;
}

static void safe_read(struct doload_environment *e, char *thing, long size)
{	
    ULONG count;

    if ((Read(e->fd, thing, (int)size, &count) != ESUCCESS) || 
	    (size != count))
	doload_punt(e, DLerr_BadRead);
}

static void prt_class(int class)
{
	char *s;
	switch (class) {
		case scNil:
			s = "scNil";
			break;
		case scText:
			s = "scText";
			break;
		case scData:
			s = "scData";
			break;
		case scBss:
			s = "scBss";
			break;
		case scRegister:
			s = "scRegister";
			break;
		case scAbs:
			s = "scAbs";
			break;
		case scUndefined:
			s = "scUndefined";
			break;
		case scCdbLocal:
			s = "scCdbLocal";
			break;
		case scBits:
			s = "scBits";
			break;
		case scCdbSystem:
			s = "scCdbSystem";
			break;
		case scRegImage:
			s = "scRegImage";
			break;
		case scInfo:
			s = "scInfo";
			break;
		case scUserStruct:
			s = "scUserStruct";
			break;
		case scSData:
			s = "scSData";
			break;
		case scSBss:
			s = "scSBss";
			break;
		case scRData:
			s = "scRData";
			break;
		case scVar:
			s = "scVar";
			break;
		case scCommon:
			s = "scCommon";
			break;
		case scSCommon:
			s = "scSCommon";
			break;
		case scVarRegister:
			s = "scVarRegister";
			break;
		case scVariant:
			s = "scVariant";
			break;
		case scSUndefined:
			s = "scSUndefined";
			break;
		case scInit:
			s = "scInit";
			break;
		case scBasedVar:
			s = "scBasedVar";
			break;
		default:
			s = "Unknown";
			break;
	}
	printf(s);
}

static void prt_lderr (int err)
{
	char *s;

	switch (err) {
		case DLerr_REFLOSplit:
			s =  "2 REFLOS off same REFHI split over 64K boundary";
			break;
		case DLerr_NoText:
			s =  "no .text section present";
			break;
		case DLerr_HasInit:
			s =  "unexpected .init section present";
			break;
		case DLerr_HasGP:
			s =  "unexpected gp sections present";
			break;
		case DLerr_HasLib:
			s =  "unexpected .lib section present";
			break;
		case DLerr_BadSeek:
			s =  "lseek failure";
			break;
		case DLerr_Prob:
			s =  "load error";
			break;
		case DLerr_RogueScn:
			s =  "unexpected section encountered";
			break;
		case DLerr_OutOfMem:
			s =  "unsuccessful malloc";
			break;
		case DLerr_BadRead:
			s =  "read failure";
			break;
		case DLerr_BadJUMPADDR:
			s =  "relocation of a JUMPADDR overflows";
			break;
		case DLerr_REFLODangle:
			s =  "REFLO to sym with diff index than last REFHI";
			break;
		case DLerr_REFLOWidow:
			s =  "REFLO not preceded by REFHI";
			break;
		case DLerr_BadGpReloc:
			s =  "GPREL reloc of external doesn't work";
			break;
		case DLerr_HasGpReloc:
			s =  "unexpected gp-relative relocs";
			break;
		case DLerr_CFFailed:
			s =  "cache flush failed";
			break;
		case DLerr_BadExtSC:
			s =  "unexpected section encountered";
			break;
		case DLerr_UndefinedSym:
			s =  "undefined symbol encountered";
			break;
		default:
			s =  "unknown error message";
			break;
	}
	printf(s);
}

static void prt_section(int scn)
{
	char *s;

	switch (scn) {
	case	STYP_REG:
		s =  "STYP_REG";
		break;
	case	STYP_DSECT:
		s =  "STYP_DSECT";
		break;
	case	STYP_NOLOAD:
		s =  "STYP_NOLOAD";
		break;
	case	STYP_GROUP:
		s =  "STYP_GROUP";
		break;
	case	STYP_PAD:
		s =  "STYP_PAD";
		break;
	case	STYP_COPY:
		s =  "STYP_COPY";
		break;
	case	STYP_TEXT:
		s =  "STYP_TEXT";
		break;
	case	STYP_DATA:
		s =  "STYP_DATA";
		break;
	case	STYP_BSS:
		s =  "STYP_BSS";
		break;
	case	STYP_RDATA:
		s =  "STYP_RDATA";
		break;
	case	STYP_SDATA:
		s =  "STYP_SDATA";
		break;
	case	STYP_SBSS:
		s =  "STYP_SBSS";
		break;
	case	STYP_UCODE:
		s =  "STYP_UCODE";
		break;
	case	STYP_LIT8:
		s =  "STYP_LIT8";
		break;
	case	STYP_LIT4:
		s =  "STYP_LIT4";
		break;
	case	S_NRELOC_OVFL:
		s =  "S_NRELOC_OVFL";
		break;
	case	STYP_LIB:
		s =  "STYP_LIB";
		break;
	case	STYP_INIT:
		s =  "STYP_INIT";
		break;
	default:
		s =  "UNKNOWN SECTION TYPE";
		break;
	}
	printf(s);
}
#endif	/* K64U64 */
