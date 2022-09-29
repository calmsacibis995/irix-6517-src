/*
 * flash.c
 *
 * Read-only flash processing.
 */
#include <assert.h>
#include <flash.h>
#include <string.h>
#include <libsc.h>

/** extern void DPRINTF(char *, ...); **/
int validBody(FlashSegment* seg);
int validHdr(const FlashROM* flash, FlashSegment* seg);

/*********************
 * findFlashSegmentHdr
 * -------------------
 * Locate a valid flash segment header
 */
FlashSegment*
findFlashSegmentHdr(const FlashROM* flash, char* name, FlashSegment* seg)
{
  while (seg = nextSegment(flash,seg))
    if ((strlen(name) == seg->nameLen) &&
	(strncmp(name,seg->name,seg->nameLen) == 0)) break ;
  return seg;
}


/******************
 * findFlashSegment
 * ----------------
 * Locate a valid flash segment by name
 */
FlashSegment*
findFlashSegment(const FlashROM* flash, char* name, FlashSegment* seg)
{
  while(seg = findFlashSegmentHdr(flash,name,seg))
    if (validBody(seg)) break ;     
  return seg;
}


/*************
 * nextSegment
 *------------
 * Return address of next segment with valid header.
 * The pointer to the "current" segment may point to an invalid
 * segment header in which case we search page by page.
 *
 * If passed zero as the current pointer, we switch to the flash base.
 * This allows nextSegment to find the "first" segment as well as the next
 * segment, avoiding the need for clients to know where to start the search.
 */
FlashSegment*
nextSegment(const FlashROM* flash, FlashSegment* seg)
{
  long          xsum;
  long*         xp;
  FlashSegment* current = seg;
  
  /* Loop until we find a valid segment header or run out of flash */

  while(1) {
    if (current==0)
      current = flash->base;
    else
      if (!validHdr(flash,current))
	/* Header invalid, advance to next flash page */
	current = (FlashSegment*)((char*)current + flash->pageSize);
      else{
	/* Header valid, advance to next segment */
	long t;
	t = (long)((char*)current + current->segLen + flash->pageSize-1);
	t &= ~(flash->pageSize-1);
	current = (FlashSegment*)t;
      }

    /*
     * Exit the loop if we've advanced past the end of the flash or if
     * we've advanced to a valid segment header.
     */
    if (current>= flash->limit || validHdr(flash,current))
      break;
  }
    
  /*
   * If past the bounds of the flash, or if a zero length segment name,
   * set current to zero (last segment)
   */
  if (current >= flash->limit || current->nameLen==0)
    current = 0;

  return current;
}


/***********
 * validBody
 * ---------
 * Verify that segment body is valid
 */
int
validBody(FlashSegment* seg)
{
  long  xsum;
  long* xp = (long*)body(seg) ;
  long* xpLimit = segChksum(seg)+1 ;  /* -> first long following segChksum() */

  xsum = 0 ;
  while (xp != xpLimit) xsum += *xp++ ;
  return xsum == 0 ;
}


/**********
 * validHdr
 * --------
 */
int
validHdr(const FlashROM* flash, FlashSegment* seg)
{
  long  xsum;
  long* xp;
  long* xpLimit = chksum(seg)+1; /* -> first long following chksum() */

  /*
   * Check header alignment, magic, checksum is in second long of an
   *  aligned long long, and segment length is an integral number of long longs
   */
  if (((long)seg & (flash->pageSize-1))    || /* Must be page aligned */
      (seg->magic != FLASH_SEGMENT_MAGIC)  || /* Must have right magic */
      (seg->segLen & 3)!=0)		      /* Total length multiple of 4 */
    return 0;

  /* Check header checksum */
  for (xsum=0, xp = (long*)seg;
       xp != xpLimit;
       xp++)
    xsum += *xp;
  return xsum == 0;
}

