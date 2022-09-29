/*
 * $Id: memorytst.c,v 1.1 1997/08/18 20:40:56 philw Exp $
 */

#include <inttypes.h>
#include <debugcard.h>
#include <utilities.h>
#include <asm_support.h>
#include <st16c1451.h>
#include <uartio.h>
#include <Dflash.h>
#include <diags.h>	
#include <sys/regdef.h>
#include <inttypes.h>
#include <post1supt.h>

#define ESC     0x1b
#define CTLC    0x03  
#define BS      0x08  
#define BELL    0x07  

extern uint32_t SizeMEM (void) ;

/*--------
 * brotate
 *--------
 * Spinning the little bar to indicate that I'm alive.
 */
unsigned char
brotate(unsigned char bar)
{
  unsigned char next = BELL ;
  
  switch (bar) {
  case '\174':          /* found '|'            */
    putchar (bar) ;     /* print the '|'        */
    next = '\057' ;     /* next will be '/'     */
    break ; 
  case '\057':          /* found '/'            */
    putchar (bar) ;     /* print the '/'        */
    next = '\055' ;     /* next will be '-'     */
    break ; 
  case '\055':          /* found '-'            */
    putchar (bar) ;     /* print the '-'        */
    next = '\134' ;     /* next will be '\'     */
    break ;
  case '\134':          /* found '\'            */
    putchar (bar) ;     /* print the '\'        */
    next = '\174' ;     /* next will be '|'     */
    break ; 
  default:
    putchar('\174');    /* print '|'            */
    next = '\057' ;     /* next will be '/'     */
  }
  putchar(BS)     ;     /* print back space.    */
  return next     ;     /* return next char.    */
}
  
/*---------
 * merrloop
 *---------
 * Loop on error, setup an error loopcount to indicate
 * how many time the error occur.
 */
void
merrloop(uint64_t* addr, uint64_t* expected, uint64_t* data)
{
  volatile uint64_t*    taddr;
  volatile uint64_t     expd = *expected;
  volatile uint64_t     obsvd= *data;
  int                   count, NOerror ;
  unsigned char         c, indicator = '\174' ; 

  /*
   * print the looping message.
   */
  printf ("\n      "
          "ENTERING ERROR LOOP:"
          "     addr=0x%0x, expected=0x%0lx, observed=0x%0lx  ",
          addr, *expected, *data);
  
  /*
   * enter the infinate loop.
   */
  taddr = addr;
  count = 0   ;     /* Initialize the error/no error loop count.    */
  NOerror = 0 ;     /* Set the no error flags to NULL               */
  
  while (((c=(getchar()&0xff)) != ESC) && (c != CTLC)) { /* The way */
    indicator = brotate(indicator) ;
    *taddr = expd  ;/* to break the loop is ESC or CTLC key stroke. */
    obsvd = *taddr ;
    if (expd == obsvd) {
      if (NOerror == 0) {
        printf ("\n      "
                "ERROR went away after %d Occurance, continue looping "
                , count) ;
        NOerror = 1 ;
        count   = 0 ;
      }
    } else {
      if (NOerror == 1) {
        printf ("\n      "
                "ERROR coming back after %d No error Occurance, continue looping "
                , count) ;
        NOerror = 0 ;
        count   = 1 ;
      }
    }
    count += 1 ;
  }
  printf ("\n");
  return ; 
}
      
/*-----------
 * merrloop32
 *-----------
 * Loop on error, setup an error loopcount to indicate
 * how many time the error occur.
 */
void
merrloop32(uint32_t* addr, uint32_t expected, uint32_t data)
{
  volatile uint32_t *taddr ;
  volatile uint32_t expd = expected;
  volatile uint32_t obsvd= data;
  int               count, NOerror ;
  unsigned char     c, indicator = '\174' ; 

  /*
   * print the looping message.
   */
  printf ("\n      "
          "ENTERING ERROR LOOP:\n"
          "     addr=0x%0x, expected=0x%0x, observed=0x%0x  ",
          addr, expected, data);
  
  /*
   * enter the infinate loop.
   */
  taddr = addr;
  count = 1   ;     /* Initialize the error/no error loop count.    */
  NOerror = 0 ;     /* Set the no error flags to NULL               */
  
  while (((c=(getchar()&0xff)) != ESC) && (c != CTLC)) { /* The way */
    indicator = brotate(indicator) ;
    *taddr = expd  ;/* to break the loop is ESC or CTLC key stroke. */
    obsvd = *taddr ;
    if (expd == obsvd) {
      if (NOerror == 0) {
        printf ("\n      "
                "ERROR went away after %d Occurance, continue looping "
                , count) ;
        NOerror = 1 ;
        count   = 0 ;
      }
    } else {
      if (NOerror == 1) {
        printf ("\n      "
                "ERROR coming back after %d No error Occurance, continue looping "
                , count) ;
        NOerror = 0 ;
        count   = 1 ;
      }
    }
    count += 1 ;
  }
  printf ("\n") ;
  return ; 
}
      
/*--------------
 * memory_walk_1
 *--------------
 * walking 1 through the 256 bits memory bus.
 */
void
memory_walk_1(uint32_t testaddr)
{
  uint64_t      expected, exptd, data, *baddr ;
  int           idx1, idx2 ;
  unsigned char indicator = '\174' ; 

  printf ("\n Walking one test (256 bits) @ address 0x%x  ", testaddr);
  baddr = (uint64_t *)((testaddr & ~0x1f)|0xa0000000);
  for (idx1 = 3 ; idx1 >= 0 ; idx1 -= 1) {
    expected = 1 ;
    while (expected != 0x0) {
      for (idx2 = 3 ; idx2 >= 0 ; idx2 -= 1) {
        /* Write 256 bits data pattern to testaddr. */
        if (idx2 == idx1) baddr[idx2] = expected ;
        else              baddr[idx2] = 0x0      ;
      }
      for (idx2 = 3 ; idx2 >= 0 ; idx2 -= 1) {
        /* verify 256 bits data pattern from testaddr. */
        data = baddr[idx2] ; 
        if (idx2 == idx1) exptd = expected ; 
        else              exptd = 0x0 ;
        if (data != exptd) {
          printf (" addr=0x%0x, expected=0x%0lx, observed=0x%0lx  ",
                  baddr, exptd, data) ;
          merrloop(baddr, &exptd, &data) ;
        }
      }
      expected = expected << 1 ; 
    }
    indicator = brotate(indicator) ;
  }
  printf ("\n  Memory walking 1 test completed.\n") ;    
  return ; 
}

/*--------------
 * memory_walk_0
 *--------------
 * walking 0 through the 256 bits memory bus.
 */
void
memory_walk_0(uint32_t testaddr)
{
  uint64_t      expected, exptd, data, *baddr ;
  int           idx1, idx2 ;
  unsigned char indicator = '\174' ; 

  printf ("\n Walking zero test (256 bits) @ address 0x%x  ", testaddr);
  baddr = (uint64_t *)(0xa0000000|(testaddr & ~0x1f));
  for (idx1 = 3 ; idx1 >= 0 ; idx1 -= 1) {
    expected = 1 ;
    while (expected != 0x0) {
      for (idx2 = 3 ; idx2 >= 0 ; idx2 -= 1) {
        /* Write 256 bits data pattern to testaddr. */
        if (idx2 == idx1) baddr[idx2] = ~expected ;
        else              baddr[idx2] = ~0x0      ;
      }
      for (idx2 = 3 ; idx2 >= 0 ; idx2 -= 1) {
        /* verify 256 bits data pattern from testaddr. */
        data = baddr[idx2] ; 
        if (idx2 == idx1) exptd = ~expected ; 
        else              exptd = ~0x0 ;
        if (data != exptd) {
          printf (" addr=0x%0x, expected=0x%0lx, observed=0x%0lx  "
                  ,baddr, exptd, data) ;
          merrloop(baddr, &exptd, &data) ;
        }
      }
      expected = expected << 1 ; 
    }
    indicator = brotate(indicator) ;
  }
  printf ("\n  Memory walking 0 test completed.\n") ;  
  return ; 
}

/*------------------
 * memory_walk_addr1
 *------------------
 * walking 1 through the the address bits.
 */
void
memory_walk_addr1(uint32_t mSize) 
{
  int           idx1, idx2 ; 
  unsigned char indicator = '\174' ;
  unsigned char *testaddr, c ;
  uint32_t      data, data1, expected, tmp, memsize ; 

  /*
   * Save memory size.
   */
  memsize = mSize ;
  printf ("\n Address walking 1 test for %d Mbytes ", memsize/(1024*1024)) ;
  
  /*
   * find the bits in the range.
   * idx1 hold the max 1 bit position wiithin the range.
   */
  for (idx1 = 1 ; idx1 < memsize ; idx1 = idx1 << 1) ;
  idx1 = idx1 >> 1 ;
  
  /*OLD
   * Now write address to addressed byte, flood the memory.
   */
  * (unsigned char *)0xa0000000 = (unsigned char) 0xff ;
  data = 1 ;
  for (idx2 = 1 ; idx2 <= idx1 ; idx2 = idx2 << 1) {
    * (unsigned char *) (0xa0000000|idx2) = (unsigned char)(data & 0xff) ;
    data += 1 ;
  }
  
  /*
   * Check the result then write inverted data..
   */
  expected = 1 ;
  for (idx2 = 1 ; idx2 <= idx1 ; idx2 = idx2 << 1) {
    data  = (uint32_t)(* (unsigned char *) (0xa0000000|idx2) & 0xff) ;
    if (expected != data) {
      testaddr = (unsigned char *) (0xa0000000|idx2) ; 
      printf ("\n    pass1: addr=0x%0x, expected=0x%0x, observed=0x%0x  ",
              testaddr, expected, data) ;
      while ( ((c = getchar()) != ESC) && (c != CTLC) ) {
        *testaddr = expected ;
        data = *testaddr ;
        indicator = brotate(indicator) ;
      }
    }
    * (unsigned char *)(0xa0000000|idx2) = (unsigned char)(~expected & 0xff) ;
    expected += 1 ;
    indicator = brotate(indicator) ;
  }
  
  /*
   * Final pass, verify the result.
   */
  tmp = 1 ;
  for (idx2 = 1 ; idx2 <= idx1 ; idx2 = idx2 << 1) {
    data = (uint32_t)(* (unsigned char *) (0xa0000000|idx2) & 0xff) ;
    expected = ~tmp & 0xff ; 
    if (expected != data) {
      testaddr = (unsigned char *) (0xa0000000|idx2) ; 
      printf ("\n    pass2: addr=0x%0x, expected=0x%0x, observed=0x%0x  ",
              testaddr, expected, data) ;
      while (((c=(getchar()&0xff)) != ESC) && (c != CTLC)) {
        *testaddr = expected ;
        data = *testaddr ;
        indicator = brotate(indicator) ;
      }
    }
    tmp += 1 ;
    indicator = brotate(indicator) ;
  }
  
  printf ("\n  Address walking 1 test completed.\n ");
  return ; 
}

/*------------------
 * memory_walk_addr0
 *------------------
 * walking 0 through the the address bits.
 */
void
memory_walk_addr0(uint32_t mSize)
{
  int           idx1, idx2, mask ;
  unsigned char indicator = '\174' ;
  unsigned char *testaddr, c ;
  uint32_t      data, expected, tmp, memsize ;

  /*
   * Save memory size and print test header.
   */
  memsize = mSize ;
  printf ("\n Address walking 0 test for %d Mbytes ", memsize/(1024*1024)) ;
  
  /*
   * find the bits in the range.
   * idx1 hold the max 1 bit position wiithin the range.
   */
  for (idx1 = 1 ; idx1 < memsize ; idx1 = idx1 << 1) ;
  mask = idx1 - 1 ;     /* Set the bit mask for the test range. */
  
  /*
   * Now write address to addressed byte, flood the memory.
   */
  * (unsigned char *)0xa0000000 = (unsigned char) 0xff ;  
  data = 1 ;
  for (idx2 = 1 ; idx2 < idx1 ; idx2 = idx2 << 1) {
    testaddr = (unsigned char *) ((~idx2 & mask) | 0xa0000000) ;
    * testaddr = (unsigned char)(data & 0xff) ;
    data += 1 ;
  }
  
  /*
   * Check the result then write inverted data..
   */
  expected = 1 ;
  for (idx2 = 1 ; idx2 < idx1 ; idx2 = idx2 << 1) {
    testaddr = (unsigned char *) ((~idx2 & mask) | 0xa0000000) ;
    data = (uint32_t) (*testaddr & 0xff) ;
    if (expected != data) {
      printf ("\n    pass1: addr=0x%0x, expected=0x%0x, observed=0x%0x  ",
              testaddr, expected, data) ;
      while (((c=(getchar()&0xff)) != ESC) && (c != CTLC)) {
        *testaddr = expected ;
        data = *testaddr ;
        indicator = brotate(indicator) ;
      }
    }
    *testaddr = (unsigned char)(~expected & 0xff) ;
    expected += 1 ;
    indicator = brotate(indicator) ;
  }
  
  /*
   * Final pass, verify the result.
   */
  tmp = 1 ;
  for (idx2 = 1 ; idx2 < idx1 ; idx2 = idx2 << 1) {
    testaddr = (unsigned char *) ((~idx2 & mask) | 0xa0000000) ;
    data = (uint32_t)(*testaddr & 0xff) ;
    expected = ~tmp & 0xff ; 
    if (expected != data) {
      printf ("\n    pass2: addr=0x%0x, expected=0x%0x, observed=0x%0x  ",
              testaddr, expected, data) ;
      while (((c=(getchar()&0xff)) != ESC) && (c != CTLC)) {
        *testaddr = expected ;
        data = *testaddr ;
        indicator = brotate(indicator) ;
      }
    }
    tmp += 1 ;
    indicator = brotate(indicator) ;
  }
  
  printf ("\n  Address walking 0 test completed.\n ");
  return ; 
}

/*---------------------
 * memory_2pattern_test
 *---------------------
 * two pattern 0x5a5a5aa5a5a5L and 0xa5a5a55a5a5aL
 * write/write/read test.
 */
void
memory_2pattern_test(uint32_t* Start, uint32_t* End) 
{
  volatile uint32_t expected, data, *testaddr1st, *testaddr2nd;
  unsigned char indicator = '\174' ; 

  printf ("\n Memory 2 pattern Write/Write/Read test:   ") ;

  /*
   * create the test pattern - single word, 0xa5a5a5a5
   */
  expected = 0xa5a5a5a5 ;

  /*
   * start write/write/read test.
   */
  testaddr1st = Start ;
  testaddr2nd = Start + 1 ;
  for (; testaddr2nd < End ; testaddr1st = testaddr2nd, testaddr2nd += 1) {
    *testaddr1st = expected  ;  /* Write    */
    *testaddr2nd = ~expected ;  /* Write    */
    data = *testaddr1st      ;  /* Read     */
    if (data != expected) {     /* Memory Write/Read error  */
      printf (" addr=0x%0x, expected=0x%0x, observed=0x%0x  ",
              testaddr1st, expected, data) ;
      merrloop32((uint32_t*)testaddr1st, expected, data) ;
    }
    indicator = brotate(indicator) ;
  }
  printf ("\n  Memory 2 pattern write/write/read test completed.\n") ;
  return ; 
}

/*---------------
 * memory_addrtst
 *---------------
 * memory address test, write test address to the addressed doubleword
 * with the pattern of (~address<<32|address)
 * there will be 3 passes in this test
 * pass1: Write address pattern to all memory
 * pass2: Verify previous write, then write inverted pattern to addressed
 *        doubleword.
 * pass3: Verify the inverted data pattern.
 */
void
memory_addrtst(uint64_t* Start, uint64_t* End)
{
  uint64_t      expected, data, *baddr, *eaddr ; 
  int           tmp, indx ;
  unsigned char indicator = '\174' ; 

  printf ("\n Memory Address test:") ;
  
  /*
   * Pass1: Write data pattern to addressed double word.
   */
  printf ("\n    pass1: Write addr pattern to addressed doubleword  ") ;
  for (baddr = Start, eaddr = End ; baddr < eaddr ; baddr += 1) {
    expected = (uint64_t)baddr ;
    data = (expected << 32) >> 32 ;
    *baddr = (~data << 32) | data ;   /* Write data pattern.    */
    indicator = brotate(indicator) ;  /* spnning the wheel      */
  }

  /*
   * Pass2: Verify pass1.
   */
  printf ("\n    pass2: Verify/Write inverted addr pattern to addressed doubleword  ") ;
  for (baddr = Start, eaddr = End ; baddr < eaddr ; baddr += 1) {
    data = (uint64_t)baddr ;
    data = (data << 32) >> 32 ;
    expected = (~data << 32) | data ;   /* Create expected data pattern */
    data = *baddr ;                     /* Read data back.              */
    if (data != expected) {
      printf (" addr=0x%0x, expected=0x%0lx, observed=0x%0lx  ",
              baddr, expected, data) ;
      merrloop(baddr, &expected, &data) ;
    }
    *baddr = ~expected ;                /* Write inverted data          */
    indicator = brotate(indicator) ;    /* spinning the wheel           */
  }

  /*
   * Pass3: Verify pass2.
   */
  printf ("\n    pass3: Verify pass2.   ") ;
  for (baddr = Start, eaddr = End ; baddr < eaddr ; baddr += 1) {
    data = (uint64_t)baddr ;
    data = (data << 32) >> 32 ;
    expected = ~((~data<<32)|data) ;    /* Create expected data pattern */
    data = *baddr ;                     /* Read data back.              */
    if (data != expected) {
      printf (" addr=0x%0x, expected=0x%0lx, observed=0x%0lx  ",
              baddr, expected, data) ;
      merrloop(baddr, &expected, &data) ;
    }
    indicator = brotate(indicator) ;    /* spinning the wheel           */
  }

  /*
   * and we are done!
   */
  printf ("\n  Memory address test completed.\n") ;
  return ;
}


/*
 *-------------------------
 * $Author: philw $
 * $Date: 1997/08/18 20:40:56 $
 * $Revision: 1.1 $
 *-------------------------
 *
 * $Log: memorytst.c,v $
 * Revision 1.1  1997/08/18 20:40:56  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.3  1996/10/31  21:50:55  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.4  1996/10/04  21:08:44  kuang
 * Cleanup the sources list
 *
 * Revision 1.3  1996/10/04  20:09:00  kuang
 * Fixed some general problem in the diagmenu area
 *
 * Revision 1.2  1996/04/04  23:33:47  kuang
 * Added more diagnostic supports
 *
 * Revision 1.1  1996/01/02  22:19:24  kuang
 * new and improved memory diags
 *
 *-------------------------
 */
/* END OF FILE */
