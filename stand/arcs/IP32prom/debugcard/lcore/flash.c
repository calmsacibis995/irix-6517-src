/*
 * -------------------------------------------------------------
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:42:07 $
 * -------------------------------------------------------------
 *  descriptions:   The MH flash related routines.
 */

#include <inttypes.h>
#include <st16c1451.h>
#include <uartio.h>
#include <asm_support.h>
#include <debugcard.h>
#include <flash.h>
#include <Dflash.h>

#if 0
#define DEBUGonly
#else
#undef DEBUGonly
#endif

#if defined(DEBUGonly)
static char FLASH_IMAGE[1] ; 
static char FLASH_IMAGE_END[1] ;
#else
extern char FLASH_IMAGE[], FLASH_IMAGE_END[] ;
#endif
extern void count2m (void)    ;
extern uint32_t rd_cause (void);
extern void     unexpected_intr (void);

/*
 * Compare two strings, return 1 if two string are identical 
 * 0 otherwise.
 */
int
strcmp (char *str1, char *str2, int len)
{
  int   stringlen ;

  stringlen = len ; 
  while ((stringlen > 0) && (*str1++ == *str2++)) --stringlen ;
  return (stringlen == 0) ;
}


/*
 * Read the flash id number.
 */
char
flashid ()
{
  int   wait ;
  char  *ptr, id1, id2 ;
  uint64_t FCTL ;

  FCTL = * (uint64_t *) FLASH_CNTL ;
  if (!enablelow(0,0)) {
    printf ("ERROR: LOWPROM address space is already on\n");
    unexpected_intr();
  }
  * (uint64_t *) FLASH_CNTL = ((FCTL|1) & 0xff) ; 
  * (char     *) 0xbfc05555 = (char) 0xaa ;
  * (char     *) 0xbfc02aaa = (char) 0x55 ;
  * (char     *) 0xbfc05555 = (char) 0x90 ;
  
  count2m () ; 

  * (uint64_t *) FLASH_CNTL = (FCTL & 0xfe) ;
  id1 = * (char *) 0xbfc00000 ; 
  id2 = * (char *) 0xbfc00001 ; 
  
  * (uint64_t *) FLASH_CNTL = ((FCTL|1) & 0xff) ; 
  * (char     *) 0xbfc05555 = (char) 0xaa ;
  * (char     *) 0xbfc02aaa = (char) 0x55 ;
  * (char     *) 0xbfc05555 = (char) 0xf0 ;
  
  count2m () ; 
  
  * (uint64_t *) FLASH_CNTL = FCTL ;
  if (!disablelow(0,0)) {
    printf ("ERROR: LOWPROM address space was not on\n");
    unexpected_intr();
  }
}


/*
 * Load the MH flash, using the flash image found in the
 * debugcard image data section.
 */
#define Debug
/* #define NoUART */
#define EnablePass2

int
loadFlash ()
{

  char      tmpb, value ; 
  uint32_t  fragment, flashptr, promptr, endptr ;
  uint32_t  tmp, ftmp, flash_v, value_v ;
  int       byteptr, idx, bitshift, byte_count ; 
  uint32_t  checksum = 0x0 ;
  uint64_t  MACE_FLASH ; 

  fragment = (uint32_t) (FLASH_IMAGE) ;
  flashptr = (uint32_t) FLASH_ALIAS ; 
  promptr  = ((uint32_t)FLASH_IMAGE + 7)  & 0xfffffff8  ;
  endptr   = ((uint32_t)FLASH_IMAGE_END)  & 0xfffffff8  ;
  
  printf ("\n\n") ;
  printf ("pass 1: Loading MH Flash from 0x%0x to 0x%0x\n", fragment, endptr) ;

  MACE_FLASH = * (uint64_t *) FLASH_CNTL ; 
  if (!enablelow(0,0)) {
    printf ("ERROR: LOWPROM address space is already on\n");
    unexpected_intr();
  }
  byte_count = 0 ;
  * (uint64_t *) FLASH_CNTL = 1 ;
  
  while (fragment != promptr) {
    * (char *)flashptr = value = * (char *)fragment ;
    checksum = checksum ^ ((uint32_t)value & 0xff) ;
    byte_count += 1 ;
    value_v = (uint32_t)(value & 0xff) ;
    while (byte_count == 256) {
      byte_count = byte_count << 3; /* extend the polling loop so we    */
                                    /* won't disturb the flash while it */
                                    /* do the write things.             */
      while (byte_count != 0x0) byte_count -= 1 ;
      flash_v = (uint32_t)(*(char*)flashptr & 0xff) ; 
      if (flash_v != value_v) byte_count = 256 ;
      else byte_count = 0 ;
    }
#if !defined(Debug) && !defined(NoUART)
    printf ("From: 0x%0x To: 0x%0x ", fragment, flashptr) ;
    printf ("Checksum: 0x%0x", checksum) ;
    for (idx = 52; idx > 0; idx -= 1) putchar(0x8) ;
#endif
    flashptr += 1 ;
    fragment += 1 ; 
  }

#if !defined(Debug) && !defined(NoUART)
  printf ("\n") ;
  printf ("0x%0x, 0x%0x\n", promptr, endptr) ;
#endif
  while (promptr != endptr) {
    tmp = * (uint32_t *)promptr ;
    for (bitshift = 24, byteptr = 0 ; byteptr < 4 ;
         byteptr += 1, bitshift -= 8) {
      * (char *)flashptr = value = (char)((tmp >> bitshift) & 0xff) ;
      checksum = checksum ^ ((uint32_t)value & 0xff) ;      
      byte_count += 1 ;
      value_v = (uint32_t)(value & 0xff) ;
      while (byte_count == 256) {
      byte_count = byte_count << 3; /* extend the polling loop so we    */
                                    /* won't disturb the flash while it */
                                    /* do the write things.             */
        while (byte_count != 0x0) byte_count -= 1 ; 
        flash_v = (uint32_t)(*(char*) flashptr & 0xff) ; 
        if (flash_v != value_v) byte_count = 256 ;
        else byte_count = 0 ;
      }
#if !defined(Debug) && !defined(NoUART)
      printf ("From: 0x%0x To: 0x%0x ", promptr, flashptr) ;
      printf ("Checksum: 0x%0x", checksum) ;
      for (idx = 52; idx > 0; idx -= 1) putchar(0x8) ;
#endif
      flashptr += 1 ;
    }
    promptr += 4 ; 
  }

#if !defined(Debug) && !defined(NoUART)  
  printf ("\n") ;
  printf ("0x%0x\n", endptr) ;
#endif
  
  fragment = (uint32_t)FLASH_IMAGE_END ;
  while (fragment != endptr) {
    * (char *) flashptr = value = * (char *) endptr ;
    checksum = checksum ^ ((uint32_t)value & 0xff) ;
    byte_count += 1 ;
    value_v = (uint32_t)(value & 0xff) ; 
    while (byte_count == 256) {
      byte_count = byte_count << 3; /* extend the polling loop so we    */
                                    /* won't disturb the flash while it */
                                    /* do the write things.             */
      while (byte_count != 0x0) byte_count -= 1 ;
      flash_v = (uint32_t)(*(char*)flashptr & 0xff) ; 
      if (flash_v != value) byte_count = 256 ;
      else byte_count = 0 ;
    }
#if !defined(Debug) && !defined(NoUART)
    printf ("From: 0x%0x To: 0x%0x ", fragment, flashptr) ;
    printf ("Checksum: 0x%0x", checksum) ;
    for (idx = 52; idx > 0; idx -= 1) putchar(0x8) ;
#endif    
    endptr   += 1 ; 
    flashptr += 1 ; 
  }
  
  value = * (uint32_t *)fragment ;
  idx = (value == checksum) ; 
  printf ("pass 1: Original checksum = 0x%0x, Computed checksum = 0x%0x\n",
          value, checksum) ;
  if (!idx) {
    * (uint64_t *) FLASH_CNTL = MACE_FLASH & ~1 ; 
    if (!disablelow(0,0)) {
      printf ("ERROR: LOWPROM address space was not on\n");
      unexpected_intr();
    }
    return (idx) ;
  }
  
#if defined(EnablePass2)

  /*
   * Turn on Green light and turn off Red light.
   */
  * (uint64_t *) FLASH_CNTL = (MACE_FLASH & ~(0x10)) | 0x20 ;

  /*
   * Read everything back from flash and compare the result.
   */
  checksum = 0 ;
  fragment = (uint32_t) FLASH_IMAGE ;
  flashptr = (uint32_t) FLASH_ALIAS ; 
  promptr  = ((uint32_t)FLASH_IMAGE + 7)  & 0xfffffff8  ;
  endptr   = ((uint32_t)FLASH_IMAGE_END)  & 0xfffffff8  ;

  printf ("pass 2: Verifing MH Flash from 0x%0x to 0x%0x\n", fragment, endptr) ;

  while (fragment != promptr) {
    value    = *(char*)flashptr ;
    checksum = checksum ^ ((uint32_t)value & 0xff) ;
    flash_v  = (uint32_t)(value & 0xff) ;
    value_v  = (uint32_t)(*(char*)fragment & 0xff) ;
    if (value_v != flash_v) {
#if defined(NoUART)    
      tmp = (uint32_t) 0xbff00000 ;
      MACE_FLASH = (MACE_FLASH & ~(0x20)) | 0x10 ;
      * (uint64_t *) FLASH_CNTL = MACE_FLASH ;
      while (1) {
        * (uint32_t *) (tmp + 0x100) = (uint32_t) value_v  ;
        * (uint32_t *) (tmp + 0x200) = (uint32_t) flash_v  ;
        * (uint32_t *) (tmp + 0x300) = (uint32_t) flashptr ;
        * (uint32_t *) (tmp + 0x400) = (uint32_t) fragment ;
        for (ftmp = 1024 ; ftmp != 0x0 ; ftmp --) {}  ;
        * (uint64_t *) FLASH_CNTL = MACE_FLASH ^ 0x10 ;
      }
#endif      
      printf ("       MH Flash Load error\n"
              "          expected = 0x%0bx\n"
              "          observed = 0x%0bx\n",
              "          flashaddr= 0x%0x\n",
              "          promaddr = 0x%0x\n",
              value_v, flash_v, flashptr, fragment) ;
      return (0) ;
    }
    flashptr += 1 ;
    fragment += 1 ; 
  }

  while (promptr != endptr) {
    tmp  = * (uint32_t *) promptr ;
    ftmp = * (uint32_t *) flashptr ;
    if (tmp != ftmp) {
#if defined(NoUART)    
      tmp = (uint32_t) 0xbff00000 ;
      MACE_FLASH = (MACE_FLASH & ~(0x20)) | 0x10 ;
      * (uint64_t *) FLASH_CNTL = MACE_FLASH ;
      while (1) {
        * (uint32_t *) (tmp + 0x100) = (uint32_t) value_v  ;
        * (uint32_t *) (tmp + 0x200) = (uint32_t) flash_v  ;
        * (uint32_t *) (tmp + 0x300) = (uint32_t) flashptr ;
        * (uint32_t *) (tmp + 0x400) = (uint32_t) fragment ;
        for (ftmp = 1024 ; ftmp != 0x0 ; ftmp --) {}  ;
        * (uint64_t *) FLASH_CNTL = MACE_FLASH ^ 0x10 ;
      }
#endif      
      printf ("       MH Flash Load error\n"
              "          expected = 0x%0lx\n"
              "          observed = 0x%0lx\n",
              "          flashaddr= 0x%0x\n",
              "          promaddr = 0x%0x\n",
              tmp, ftmp, flashptr, fragment) ;
      return (0) ;
    }
    for (bitshift = 24, byteptr = 0 ; byteptr < 4 ;
         byteptr += 1, bitshift -= 8)
      checksum = checksum ^ ((ftmp >> bitshift) & 0xff) ;
    promptr  += 4 ;
    flashptr += 4 ;
  }

  fragment = (uint32_t)FLASH_IMAGE_END ;
  while (fragment != endptr) {
    value    = *(char*) flashptr ;
    checksum = checksum ^ ((uint32_t)value & 0xff) ;
    flash_v  = (uint32_t)(value & 0xff) ;
    value_v  = (*(char*)endptr & 0xff) ;
    if (value_v != flash_v) {
#if defined(NoUART)    
      tmp = (uint32_t) 0xbff00000 ;
      MACE_FLASH = (MACE_FLASH & ~(0x20)) | 0x10 ;
      * (uint64_t *) FLASH_CNTL = MACE_FLASH ;
      while (1) {
        * (uint32_t *) (tmp + 0x100) = (uint32_t) value_v  ;
        * (uint32_t *) (tmp + 0x200) = (uint32_t) flash_v  ;
        * (uint32_t *) (tmp + 0x300) = (uint32_t) flashptr ;
        * (uint32_t *) (tmp + 0x400) = (uint32_t) fragment ;
        for (ftmp = 1024 ; ftmp != 0x0 ; ftmp --) {}  ;
        * (uint64_t *) FLASH_CNTL = MACE_FLASH ^ 0x10 ;
      }
#endif      
      
      tmp = (uint32_t) 0xbff00000 ;
      * (uint32_t *) (tmp + 0x100) = (uint32_t) value_v  ;
      * (uint32_t *) (tmp + 0x200) = (uint32_t) flash_v  ;
      * (uint32_t *) (tmp + 0x300) = (uint32_t) flashptr ;
      * (uint32_t *) (tmp + 0x400) = (uint32_t) fragment ;
      printf ("       MH Flash Load error\n"
              "          expected = 0x%0bx\n"
              "          observed = 0x%0bx\n",
              "          flashaddr= 0x%0x\n",
              "          promaddr = 0x%0x\n",
              value, tmpb, flashptr, endptr) ;
      return (0) ;
    }
    endptr   += 1 ; 
    flashptr += 1 ; 
  }
  
  value = * (uint32_t *)fragment ;
  idx = (value == checksum) ; 
  printf ("pass 2: Original checksum = 0x%0x, Computed checksum = 0x%0x\n",
          value, checksum) ;
#endif
  
  * (uint64_t *) FLASH_CNTL = MACE_FLASH & ~1 ; 
  if (!disablelow(0,0)) {
    printf ("ERROR: LOWPROM address space was not on\n");
    unexpected_intr();
  }
  return (idx) ;
}


/*
 * Find it way to MH flash, write unique data patterns 
 * to MH flash then read it back and verify the result.
 */
int
probeFlash ()
{
  char *ptr, pattern ;
  int  i, error = 1 ;

  if (!enablelow(0,0)) {
    printf ("ERROR: LOWPROM address space is already on\n");
    unexpected_intr();
  }
  * (uint64_t *) FLASH_CNTL = 1 ; 
  ptr = (char *) FLASH_ALIAS ;
  for (i = 0 ; i < 8 ; i++) *ptr++ = i ;
  ptr = (char *) FLASH_ALIAS ;
  for (i = 0 ; i < 8 ; i++) if (*ptr++ != i) error = 0 ;
  * (uint64_t *) FLASH_CNTL = 0 ; 
  if (!disablelow(0,0)) {
    printf ("ERROR: LOWPROM address space was not on\n");
    unexpected_intr();
  }
  return (error) ;
}


/*
 * Verify the MH flash has valid segment header,
 * checkout the sloader segment is more then enough.
 */
int
verifyFlash ()
{
  FlashSegment  *ptr ;
  int           validhdr, i, nlen ; 
  char          *nptr ;
  
  /*
   * Verify the header magic number, and segment name.
   */
  printf ("verifyFlash: status = 0x%0x\n", rd_cause());
  if (!enablelow(0,0)) {
    printf ("ERROR: LOWPROM address space is already on\n");
    unexpected_intr();
  }
  printf ("verifyFlash: status = 0x%0x\n", rd_cause());
  ptr = (FlashSegment *) FLASH_ALIAS ; 
  if ((ptr->magic == FLASH_SEGMENT_MAGICx) &&
      strcmp (ptr->name, SLOADER_FLASH_SEGMENT, ptr->nameLen)) {
    /*
     * We have a valid sloader header.
     */
    validhdr = 1 ;
    printf ("\n\nSLOADER SEGMENT HEADER FOUND IN MH PROM:\n") ;
  } else {
    /*
     * Found an invalid sloader header.
     */
    validhdr = 0 ;
    printf ("\nAN INVALID SLOADER SEGMENT HEADER FOUND IN MH PROM:\n") ;
  }

  /*
   * print the header information.
   */
  printf ("\n"
          "0xbfc00000:Reserved\t0x%0lx\n"
          "0xbfc00008:Magic   \t0x%0x\n"
          "0xbfc0000C:SegLen  \t0x%0x\n"
          "0xbfc00010:NameLen \t0x%0x\n"
          "0xbfc00011:VsnLen  \t0x%0x\n"
          "0xbfc00012:SegType \t0x%0x\n",
          ptr->reserved, ptr->magic, ptr->segLen, ptr->nameLen,
          ptr->vsnLen, ptr->segType) ;
  nlen = (int) ptr->nameLen ;
  nptr = (char *) (&ptr->name) ;
  printf ("0xbfc00014:SegName \t") ;
  for (i = 0 ; i < nlen ; i++) putchar (*nptr++) ;
  printf ("\n\n") ;
  
  /*
   * See if we got a valid sloader header.
   */
  printf ("verifyFlash: status = 0x%0x\n", rd_cause());
  if (!disablelow(0,0)) {
    printf ("ERROR: LOWPROM address space was not on\n");
    unexpected_intr();
  }
  printf ("verifyFlash: status = 0x%0x\n", rd_cause());
  return (validhdr) ;
}


/*
 * Jump to flash, 
 * [01] Disable secondary cache if there is any.
 * [02] Invalid all primary cache line, this will destory
 *      debugcard's stack.
 * [03] Invalid all TLB entries.
 */
int
jump2Flash ()
{
  if (verifyFlash()) {
    printf ("\n\n") ;
    _jump2Flash (0,0) ;
  }
  return (0) ;
}


/*
 * Flash write functions
 * This function accept a 8 bit unsigned data and write it to
 * the flash at addr, also update the checksum get passed in
 * RETURN: current byte count.
 */

#define FSSize  256

#ifdef  Sim
#define FSWAIT  2
#else
#define FSWAIT  2048
#endif

typedef unsigned char uchar ;

void
Fsh8Wrt(uchar data, uchar* addr, uint32_t* chksmptr, int* Bcnt) 
{
  int*              bptr      = Bcnt  ;
  volatile uchar*   fptr      = addr  ;
  register int      bytecount = *Bcnt ;
  register uchar    bdata     = data & 0xff ;
  register uint32_t ChkSum    = *chksmptr & 0xff ; 

  *fptr = bdata ;           /* Write data to flash.         */
                            /* Update running checksum.     */
  *chksmptr = ChkSum ^ bdata ;
  bytecount += 1 ;          /* Update running bytecount.    */
  while (bytecount == FSSize) { /* can not cross sector.    */
    for (bytecount=FSWAIT; bytecount > 0; bytecount -= 1) ;
    if (bdata != *fptr)
      bytecount = 256 ;
    else {
      bytecount = 0 ;
      break ;
    }
  }
  *bptr = bytecount ;
  return ;
}


void
Fsh32Wrt(uint32_t data, uchar* addr, uint32_t* chksmptr, int* Bcnt) 
{
  register int      bitshift, byteptr ;
  int*              bptr      = Bcnt  ; 
  register int      bytecount = *Bcnt ;
  register uchar    value     = 0x0   ;
  volatile uchar*   fptr      = addr  ; 
  register uint32_t wdata     = data  ;
  register uint32_t ChkSum    = *chksmptr & 0x000000ff ; 

  for (bitshift = 24, byteptr = 0 ; byteptr < 4 ;
       byteptr += 1, bitshift -= 8) {/* Loop for 4 byte flash write.    */
                                    /* Write to flash.                  */
    fptr[byteptr] = value = (uchar)((wdata >> bitshift) & 0x000000ff) ;
    ChkSum = ChkSum ^ value ;       /* Compute the checkSum.            */
    bytecount += 1 ;                /* Update running bytecount.        */
    while (bytecount == FSSize) {   /* can not cross sector.            */
      for (bytecount=FSWAIT; bytecount > 0; bytecount -= 1) ;
      if (value != fptr[byteptr])
        bytecount = 256 ;
      else {
        bytecount = 0 ;
        break ;
      }
    }
  }
  *bptr = bytecount ;
  *chksmptr = ChkSum ; 
  return ;
}


int
Fsh8RDt(uchar* paddr, uchar* faddr, uint32_t* chksmptr) 
{
  register uchar    pdata   = *paddr & (uchar)0xff ;
  register uchar    fdata   = *faddr & (uchar)0xff ;
  register uint32_t ChkSum  = *chksmptr & (uint32_t)0xff ;
  uint32_t*         chkptr  = chksmptr ; 

  *chkptr = ChkSum ; 
  if (pdata != fdata) {
    printf ("       MH Flash Load error\n"
            "          expected = 0x%0bx\n"
            "          observed = 0x%0bx\n",
            "          promaddr = 0x%0x\n",
            "          flashaddr= 0x%0x\n",
            pdata, fdata, paddr, faddr) ;
    return 1 ;
  } else
    return 0 ;
}


int
Fsh32RDt(uint32_t* paddr, uint32_t* faddr, uint32_t* chksmptr)
{
  register int      bitshift, byteptr  ;
  register uint32_t pdata     = *paddr ;
  register uint32_t fdata     = *faddr ;
  register uint32_t ChkSum    = *chksmptr & (uint32_t)0xff ;

  /* Creating 4 byte checksum for current word. */
  for (bitshift = 24, byteptr = 0 ; byteptr < 4 ;
       byteptr += 1, bitshift -= 8) 
    ChkSum = ChkSum ^ (uint32_t)((fdata >> bitshift) & 0xff) ;

  /* Update the running checksum.   */
  *chksmptr = ChkSum ;
  
  /* Verify the data */
  if (pdata != fdata) {
    printf ("       MH Flash Load error\n"
            "          expected = 0x%0x\n"
            "          observed = 0x%0x\n",
            "          promaddr = 0x%0x\n",
            "          flashaddr= 0x%0x\n",
            pdata, fdata, paddr, faddr) ;
    return 4 ;
  } else
    return 0 ; 
}


/*
 * Upload the MH flash, using the flash image found in the
 * debugcard image data section.
 */
int
UPloadFlash ()
{

  uint32_t  fragment, flashptr, promptr, endptr ;
  int       idx, byte_count ;
  uint32_t  value, checksum = 0x0 ;
  uint64_t  MACE_FLASH ;
  int       ErrorCount = 0x0 ;


  /*
   * [1] Align to double word boundary.
   */
  fragment = (uint32_t) (FLASH_IMAGE) ;
  flashptr = (uint32_t) FLASH_ALIAS ; 
  promptr  = ((uint32_t)FLASH_IMAGE + 7)  & 0xfffffff8  ;
  endptr   = ((uint32_t)FLASH_IMAGE_END)  & 0xfffffff8  ;

  printf ("\n\n"
          "  pass 1: Loading MH Flash from 0x%0x to 0x%0x\n", fragment, endptr) ;
  
  MACE_FLASH = * (uint64_t *) FLASH_CNTL ; 
  if (!enablelow(0,0)) {
    printf ("ERROR: LOWPROM address space is already on\n");
    unexpected_intr();
  }
  byte_count = 0 ;
  * (uint64_t *) FLASH_CNTL = 1 ;
  
  while (fragment != promptr) {
    Fsh8Wrt(*(uchar*)fragment,(uchar*)flashptr,&checksum,&byte_count) ;
    flashptr += 1 ;
    fragment += 1 ; 
  }

  /*
   * [2] Write a 32 bits chunk to flash.
   */
  while (promptr != endptr) {
    Fsh32Wrt(*(uint32_t*)promptr,(uchar*)flashptr,&checksum,&byte_count) ;
    flashptr += 4 ;
    promptr  += 4 ; 
  }

  /*
   * [3] Write the remainning not doubleword aligned bytes to flash.
   */
  fragment = (uint32_t)FLASH_IMAGE_END ;
  while (fragment != endptr) {
    Fsh8Wrt(*(uchar*)endptr,(uchar*)flashptr,&checksum,&byte_count) ;
    endptr   += 1 ; 
    flashptr += 1 ; 
  }

  /*
   * [4] Varify the checksum.
   */
  value = * (uint32_t *)fragment ;
  idx = (value == checksum) ; 
  printf ("  pass 1: Original checksum = 0x%0x, Computed checksum = 0x%0x\n",
          value, checksum) ;
  if (!idx) {
    * (uint64_t *) FLASH_CNTL = MACE_FLASH & ~1 ; 
    if (!disablelow(0,0)) {
      printf ("ERROR: LOWPROM address space was not on\n");
      unexpected_intr();
    }
    return (idx) ;
  }
  
#if defined(EnablePass2)

  /*
   * [5] Turn on Green light and turn off Red light.
   */
  * (uint64_t *) FLASH_CNTL = (MACE_FLASH & ~(0x10)) | 0x20 ;

  /*
   * [6] Read everything back from flash and compare the result.
   */
  checksum = 0 ;
  fragment = (uint32_t) FLASH_IMAGE ;
  flashptr = (uint32_t) FLASH_ALIAS ; 
  promptr  = ((uint32_t)FLASH_IMAGE + 7)  & 0xfffffff8  ;
  endptr   = ((uint32_t)FLASH_IMAGE_END)  & 0xfffffff8  ;

  printf ("  pass 2: Verifing MH Flash from 0x%0x to 0x%0x\n", fragment, endptr) ;

  ErrorCount = 0x0 ; 
  while (fragment != promptr) {
    ErrorCount = ErrorCount +
      Fsh8RDt((uchar*)fragment,(uchar*)flashptr,&checksum) ;
    flashptr += 1 ;
    fragment += 1 ; 
  }

  while (promptr != endptr) {
    ErrorCount = ErrorCount +
      Fsh32RDt((uint32_t*)promptr,(uint32_t*)flashptr,&checksum) ;
    promptr  += 4 ;
    flashptr += 4 ;
  }

  /* Set fragment point to the last byte of the image. */
  fragment = (uint32_t)FLASH_IMAGE_END ;
  
  while (endptr != fragment) {
    ErrorCount = ErrorCount +
      Fsh8RDt((uchar*)endptr,(uchar*)flashptr,&checksum) ;
    endptr   += 1 ; 
    flashptr += 1 ; 
  }
  
  value = * (uint32_t *)fragment ;
  idx = (value == checksum) ; 
  printf ("  pass 2: Original checksum = 0x%0x, Computed checksum = 0x%0x\n",
          value, checksum) ;
#endif
  
  * (uint64_t *) FLASH_CNTL = MACE_FLASH & ~1 ; 
  if (!disablelow(0,0)) {
    printf ("ERROR: LOWPROM address space was not on\n");
    unexpected_intr();
  }
  return (idx) ;
}

/*
 * -------------------------------------------------------------
 *
 * $Log: flash.c,v $
 * Revision 1.1  1997/08/18 20:42:07  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.5  1996/10/31  21:51:50  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.5  1996/10/04  20:09:10  kuang
 * Fixed some general problem in the diagmenu area
 *
 * Revision 1.4  1996/04/11  01:25:04  kuang
 * Fixed the flash loading problem
 *
 * Revision 1.3  1996/04/04  23:17:25  kuang
 * Added more diagnostic support and some general cleanup
 *
 * Revision 1.2  1995/12/30  03:28:14  kuang
 * First moosehead lab bringup checkin, corresponding to 12-29-95 d15
 *
 * Revision 1.1  1995/11/15  00:42:45  kuang
 * initial checkin
 *
 * Revision 1.3  1995/11/14  23:07:31  kuang
 * *** empty log message ***
 *
 * -------------------------------------------------------------
 */

/* END OF FILE */
