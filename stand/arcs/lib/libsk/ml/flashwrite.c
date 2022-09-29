/*
 * flashwrite.c
 *
 * Code to update the flash
 */
#include <assert.h>
#include <fault.h>
#include <flash.h>
#include <string.h>
#include <setjmp.h>
#include <libsc.h>
#include <arcs/errno.h>

extern int validHdr(const FlashROM*, FlashSegment*);
extern int validBody(FlashSegment*);
/** extern void DPRINTF(char *, ...); **/
#define DPRINTF if (Debug) printf


/*******************
 * writeFlashSegment
 * -----------------
 * Write a segment to flash
 * -----------------
 */
/* define newSegChksum(f,n)   (__RUP(((char*)f)+n->segLen,long*)-1) */
/* moved to IP32/include/flash.h    */
int
writeFlashSegment(const FlashROM*	flash,
		  FlashSegment* seg,
		  char*         segName,
		  char*         vsnData,
		  long*		body,
		  int		bodyLen)
{
  long           hdr[136];	/* Working buffer for seg header */
  FlashSegment*  hp = (FlashSegment*)hdr;
  long*          fp = hdr ;
  long           xsum = 0;
  jmpbufptr	 oldfault = nofault;
  jmp_buf	 jb;
  long*          tp = (long*)seg;
  long*          ChkSumLoc ;
  long           bytecount ; 

  assert(strlen(segName)< 255 && strlen(vsnData) < 255);

  if (!setjmp(jb)){
    /* Initialize the header */
    bzero(hdr,sizeof hdr);
    hp->magic = FLASH_SEGMENT_MAGIC;
    hp->nameLen = strlen(segName);
    hp->vsnLen = strlen(vsnData);
    hp->segLen = hdrSize(hp) + __RUP(bodyLen,long) + 4;
    bcopy(segName,hp->name,hp->nameLen);
    bcopy(vsnData,version(hp),hp->vsnLen);
    ChkSumLoc = newSegChksum(seg,hp) ;
    
    /* Enable flash update */
    (*flash->writeEnable)(1);

    /* Initialize the FLASH write byte count. */
    bytecount = 0 ;

    /* Copy header to FLASH */
    while (fp != chksum(hp)) {
      xsum += *fp;
      (*flash->write)(tp++,*fp++,&bytecount);
    }
    (*flash->write)(tp++,-xsum,&bytecount);

    /* Copy body to FLASH */
    /*    while(tp!=segChksum(seg)){ BUG.....BUG......*/
    xsum = 0;
    while(tp != ChkSumLoc) {
      xsum += *body;
      (*flash->write)(tp++,*body++,&bytecount);
    }
    (*flash->write)(tp++,-xsum,&bytecount);
    (*flash->writeFlush)();
    
    /* Disable flash update */
    (*flash->writeEnable)(0);
  }
  
  nofault = oldfault;
  return ESUCCESS;
}


/*********************
 * rewriteFlashSegment
 * -------------------
 * A completely re-written routine which (I hope) now
 * doing the right things.
 * -------------------
 */
int
rewriteFlashSegment(const FlashROM* flash, FlashSegment* seg, long* buf)
{
  long          hdr[136];	/* Working buffer for seg header */
  register long	xsum, indx ; 
  register int  sts = EFAULT;
  register long *xp, *ChkSumLoc, *fp,*hp ; 
  jmpbufptr	oldfault = nofault;
  jmp_buf	jb;
  long      bytecount ; 

  /*
   * Use a jumpbuf to survive a memory fault.
   * Check that the header is valid before doing updating anything.
   */
  if (!setjmp(jb)) {
    if (validHdr(flash,seg)) {
      /*
       * Copy data to segment computing checksum on the fly. 
       * Note that since we aren't modifying the header, we start
       * after the header checksum.
       * WRONG!!!!!!!!!!!!!!!!!!!!!!!!
       * O-------------------O BUG ALART O--------------------O
       *  we'll have to copy the whold header out of flash and
       *  then write it back in order to avoid the problem of
       *  crossing the flash sector.
       * 0-------------------O BUG ALART O--------------------O
      for (xp = (long*)chksum(seg)+1, xsum = 0;
           xp != (long*)segChksum(seg); xp++) {
       * O-------------------O BUG ALART O--------------------O
       * O-------------------O BUG ALART O--------------------O
       */

      /*
       * Prepare the header of the segment.
       */
      fp = (long*)seg ;
      indx = 0 ;
      ChkSumLoc = (long*)chksum(seg) ;
      while (fp != ChkSumLoc) { /* a Wildcard copy routine.         */
        hdr[indx++] = *fp++ ;   /* A word read from flash.          */
      }
      hdr[indx] = *fp ;         /* Also read the checksum.          */
      /* xp = (long*)chksum(seg)+1 ;  and this is a BUG             */
      ChkSumLoc = segChksum(seg); /* This is the checksum location  */
      xp = (long*)seg ;         /* of the segment body.             */ 
      xsum = 0 ;                /* xp point to the first location   */ 
                                /* of the segment.                  */
      (*flash->writeEnable)(1); /* Enable flash update              */
      bytecount = 0 ;           /* And set the initial bye count.   */
      while (xsum != indx) {    /* Write the header of the segment  */
        (*flash->write)(xp++, hdr[xsum++], &bytecount);
      }
      				/* Checksum of the header.   */
      (*flash->write)(xp++,hdr[xsum], &bytecount);
      xsum = 0 ;                /* re-initialize the checksum to 0  */
      while (xp != ChkSumLoc) { /* Now the body of the segment.     */
	xsum += *buf;           /* Compute the body checksum.       */
                                /* Call flash write routine.        */
        (*flash->write)(xp++, *buf++, &bytecount);
      }
                                /* Write the checksum.              */
      (*flash->write)(xp, -xsum, &bytecount);
      (*flash->writeEnable)(0); /* Disable flash update             */
      sts = ESUCCESS;           /* Return a success indicator.      */
    } else {                    /* We in deeeeep trouble ...........*/
      DPRINTF ("<fw> <rewriteFlashSegment> NO VALID FLASH SEGMENT\n") ;
    }
  }
  nofault = oldfault;

  return sts;
}






