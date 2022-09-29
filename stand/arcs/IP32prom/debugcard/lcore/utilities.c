/*
 * -------------------------------------------------------------
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:42:23 $
 * -------------------------------------------------------------
 *  descriptions:   A collection of all sorts of utilities.
 */

#ifdef  DEBUG
#define GETC  getchar
#else
#define GETC  getc
#endif

#include <inttypes.h>
#include <st16c1451.h>
#include <uartio.h>
#include <asm_support.h>
#include <debugcard.h>
/* #include <utilities.h> */

void DISPtlbs () {}
void DISPcache () {}
void DISPregs () {}
void MOOSEmemory () {}

/*
 * Function: block_error, print error message and return.
 */
void
BlockRD_error(int64_t* expect, int64_t* observ,
              uint64_t* erroraddr, uint32_t* instaddr)
{
  /*
   * Print the error message for block read, break 2.
   */
  printf ("BlockRD 2 error: Error test address = 0x%0x\n"
          "                     expected data = 0x%0lx\n"
          "                     observed data = 0x%0lx\n",
          erroraddr, *expect, *observ) ;
  return ;
}          
   
/*
 * Function: block_error, print error message and return.
 */
void
block_error(uint64_t* expect, uint64_t* observ, uint64_t* erroraddr,
            uint64_t* epc)
{
  uint64_t expected = *expect ; 
  uint64_t observed = *observ ;
  
  /*
   * Print the error message for block read, break 2.
   */
  printf ("\n"
          "break 2 Error     EPC = 0x%0x\n"
          "        Test addredss = 0x%0x\n"
          "        Expected data = 0x%0lx\n"
          "        Observed data = 0x%0lx\n",
          epc, erroraddr, expected, observed);
  return ;
}          
   
/*
 * Function: DUMPregs, dump the regsiters saved in the register
 *           dump area.
 */
void
DUMPregs(char* errmsg, uint64_t* IU_dumparea, uint32_t* CP0_dumparea,
         uint64_t* CRIME_dumparea) 
{
  /*
   * Define the register dumparea data structure.
   */
  uint64_t* IUptr    = IU_dumparea ;
  uint32_t* CP0ptr   = CP0_dumparea ; 
  uint64_t* CRIMEptr = CRIME_dumparea ;

  register int idx, idx2; 

  /*
   * print error message.
   */
  printf ("%s\n", errmsg) ;

  /*
   * Now print IU general registers.
   */
  idx  = 0 ;
  printf ("IU general registers dump:\n") ;
  while (idx < 32) {
    printf ("%d:\t0x%0lx, 0x%0lx\n", idx, IUptr[idx], IUptr[idx+1]) ; 
    idx += 2 ;
  }

  /*
   * Print CP0 registers first.
   */
  idx  = 0 ;
  printf ("COP0 registers dump:\n") ;
  while (idx < 32) {
    printf ("%d:\t0x%0x, 0x%0x, 0x%0x, 0x%0x\n", idx,
            CP0ptr[idx], CP0ptr[idx+1], CP0ptr[idx+2], CP0ptr[idx+3]) ;
    idx += 4 ;
  }
  
  /*
   * Now print CRIME cpu interface registers.
   */
  idx = idx2 = 0 ;
  printf ("CRIME cpu interface registers dump:\n") ;
  while (idx < 27) {
    if (idx == 12) {
      printf ("CRIME memory controller registers dump:\n") ;
      idx2 = 0x0;
    }
    if (idx == 26)
      printf ("%d:\t0x%0lx\n", idx, CRIMEptr[idx]) ;
    else
      printf ("%d:\t0x%0lx, 0x%0lx\n", idx, CRIMEptr[idx], CRIMEptr[idx+1]) ; 
    idx  += 2 ;
    idx2 += 2 ;
  }
  
  /*
   * And we done!
   */
  return ; 
    
}

/*
 * Function: ASK for test address.
 */
uint32_t
GETaddr ()
{
  char     askaddr [] = "\n     address     = 0x" ; 
  uint32_t addr ;

  addr = -1  ;
  printf ("%s", askaddr)    ;
  while ((addr = gethex()) == -1) {
    printf ("\n"
            "*** INVALID ADDRESS ***"
            "\n") ;
    printf ("%s", askaddr)    ;
  }
  return addr ;
}
    

/*
 * Function: ASK for test data.
 */
uint64_t
GETdata ()
{
  char      datap [] = "\n     data        = 0x" ; 
  uint64_t  data ;

  data = 0  ;
  printf ("%s", datap)    ;
  data = gethex() ; 

  return data ;
}

  
/*
 * Function: ASK for test data size.
 */
int32_t
GETdatasize ()
{
  char     asksize [] = "\n     size(bits)  = 0x" ; 
  int32_t  size ;

  size = 0  ;
  printf ("%s", asksize) ;
  while (((size = (int32_t) gethex ()) != 8) &&
         (size != 16) && (size != 32) && (size != 64)) {
    printf ("\n"
            "*** INVALID DATA SIZE ***"
            "\n") ;
    printf ("%s", asksize) ;
  }

  return size ;
}

  
/*
 * Function: read the given address - assume the address is
 *                                    physical address, if 
 *                                    its virtual address assume
 *                                    it already mapped.
 */
void
READfunction ()
{
  char        c ;
  uint64_t    *addr, data ;
  uint32_t    address ;
  int         dsize ;

  do {
    
    printf ("\n\n\nREAD function --- \n") ;

    /*
     * Get the address, the address will get converted into
     * a 32 bits unsigned int type.
     */
    address = GETaddr () ;
    
    /*
     * Get the data size for the read operation, it can only be
     * 8, 16, 32, and 64, if its valid data size then goahead
     * do the read operation.
     */
    switch (dsize = GETdatasize()) {
    case 8:
      data = (uint64_t)(*(uint8_t *)address)  ;
      break ; 
    case 16:
      data = (uint64_t)(*(uint16_t *)address) ;
      break ; 
    case 32:
      data = (uint64_t)(*(uint32_t *)address) ;
      break ; 
    case 64:
      data = (uint64_t)(*(uint64_t *)address) ;
      break ;
    default:
      printf ("\n\nHah, what is this -- unknown data size %d\n", dsize) ;
      break ;
    }
    
    /*
     * See if we should continue the same function.
     */
    printf ("\n\n[READ] Address = 0x%0x, Data = 0x%0lX\n", address, data) ;
    printf ("\nESC to exit READ function, any key to continue. ") ;
    
  } while ((c = GETC ()) != '\033') ;

  return ;
  
}

/*
 * Functions: Write data to the given address
 *            - assume the address is physical address, if 
 *              its virtual address then assume that the 
 *              VA->PA is already mapped.
 */
void
WRITEfunction ()
{
  
  char        c ;
  uint64_t    data ; 
  uint32_t    address ;
  int         dsize ;

  do {
    
    printf ("\n\n\nWRITE function --- \n") ;

    /*
     * Get the address, the address will get converted into
     * a 32 bits unsigned int type.
     */
    address = GETaddr ()        ;
    data    = GETdata ()        ;
    dsize   = GETdatasize ()    ;
    
    /*
     * Get the data size for the read operation, it can only be
     * 8, 16, 32, and 64, if its valid data size then goahead
     * do the read operation.
     */
    switch (dsize) {
    case 8:
      * (uint8_t *) address = (uint8_t) data ;
      break ; 
    case 16:
      address = (address >> 1) << 1 ;
      * (uint16_t *) address = (uint16_t) data ;
      break ; 
    case 32:
      address = (address >> 2) << 2 ;
      * (uint32_t *) address = (uint32_t) data ;
      break ; 
    case 64:
      address = (address >> 3) << 3 ;
      * (uint64_t *) address = data ;
      break ;
    default:
      printf ("\n\nHah, what is this -- unknow data size %d\n", dsize) ;
      printf ("Debug call ------ GETdatasize in WRITEfunction () ------\n") ;
      break ;
    }
    
    /*
     * Now print the data we just wrote and see if we should continue.
     */
    printf ("\n\n[Write] Address = 0x%0x, data = 0x%0lX\n",address, data) ;    
    printf ("\nESC to exit WRITE function, any key to continue. ") ;
  } while ((c = GETC ()) != '\033') ;

  return ;
  
}

/*
 * Functions: Write data to the given address, then read it back.
 *            - assume the address is physical address, if 
 *              its virtual address then assume that the 
 *              VA->PA is already mapped.
 */
void
WRnRDfunction ()
{
  char        c ;
  uint64_t    data, rdata ; 
  uint32_t    address ;
  int         dsize ;

  do {
    
    printf ("\n\n\nWRITE then READ function --- \n") ;

    /*
     * Get the address, the address will get converted into
     * a 32 bits unsigned int type.
     */
    address = GETaddr ()        ;
    data    = GETdata ()        ; 
    dsize   = GETdatasize ()    ;
    
    /*
     * Get the data size for the read operation, it can only be
     * 8, 16, 32, and 64, if its valid data size then goahead
     * do the read operation.
     */
    printf ("\n\n") ;
    switch (dsize) {
    case 8:
      * (uint8_t *) address = (uint8_t) data ;
      rdata = * (uint8_t *) address ; 
      break ; 
    case 16:
      address = (address >> 1) << 1 ;
      * (uint16_t *) address = (uint16_t) data ;
      rdata = * (uint16_t *) address ; 
      break ; 
    case 32:
      address = (address >> 2) << 2 ;
      * (uint32_t *) address = (uint32_t) data ;
      rdata = * (uint32_t *) address ;
      break ; 
    case 64:
      address = (address >> 3) << 3 ;
      * (uint64_t *) address = data ;
      rdata = * (uint64_t *) address ;
      break ;
    default:
      printf ("\n\nHah, what is this -- unknow data size %d\n", dsize) ;
      printf ("Debug call ------ GETdatasize in WRnRDfunction () ------\n") ;
      break ;
    }

    /*
     * Now print the data just read back, and see if we should continue.
     */
    printf ("[Write] Address = 0x%0x, data = 0x%0lX\n",address, data) ;
    printf ("[Read]  Address = 0x%0x, data = 0x%0lX\n",address, rdata) ; 
    printf ("\nESC to exit WRITE function, any key to continue. ") ;
  } while ((c = GETC ()) != '\033') ;

  return ;
}

/*
 * Functions: Read/Merge/Write data from/to address.
 *            - assume the address is physical address, if 
 *              its virtual address then assume that the 
 *              VA->PA is already mapped.
 */
void
RDmWRfunction ()
{
  char        c ;
  uint64_t    data, rdata ; 
  uint32_t    address ;
  int         dsize ;

  do {
    
    printf ("\n\n\nREAD/MERGE/WRITE function --- \n") ;

    /*
     * Get the address, the address will get converted into
     * a 32 bits unsigned int type.
     */
    address = GETaddr ()        ;
    data    = GETdata ()        ; 
    dsize   = GETdatasize ()    ;
    
    /*
     * Get the data size for the read operation, it can only be
     * 8, 16, 32, and 64, if its valid data size then goahead
     * do the read operation.
     */
    printf ("\n\n") ;
    switch (dsize) {
    case 8:
      rdata = * (uint8_t *) address ;
      data = data ^ rdata ; 
      * (uint8_t *) address = (uint8_t) data ;
      break ; 
    case 16:
      address = (address >> 1) << 1 ;
      rdata = * (uint16_t *) address ;
      data = data ^ rdata ; 
      * (uint16_t *) address = (uint16_t) data ;
      break ; 
    case 32:
      address = (address >> 2) << 2 ;
      rdata = * (uint32_t *) address ;
      data = data ^ rdata ; 
      * (uint32_t *) address = (uint32_t) data ;
      break ; 
    case 64:
      address = (address >> 3) << 3 ;
      rdata = * (uint64_t *) address ;
      data = data ^ rdata ; 
      * (uint64_t *) address = data ;
      break ;
    default:
      printf ("\n\nHah, what is this -- unknow data size %d\n", dsize) ;
      printf ("Debug call ------ GETdatasize in RDmWRfunction () ------\n") ;
      break ;
    }
    
    /*
     * See if we should continue.
     */
    printf ("[Read]  Address = 0x%0x, data =  0x%0lX\n", address, rdata) ;    
    printf ("[Write] Address = 0x%0x, data =  0x%0lX\n", address, data) ;    
    printf ("\nESC to exit READ/MERGE/WRITE function, any key to continue. ") ;
  } while ((c = GETC ()) != '\033') ;

  return ;
}

    
/*
 * Functions print the utilities menus.
 */
void
UTILmenu ()
{
    printf ("\n\n"
            "Moosehead Debugcard utilities\n\n"
            "     [1] Read <address>\n"
            "     [2] Write <address>\n"
            "     [3] Write then read <address>\n"
            "     [4] Read/XOR/Write <address>\n"
            "     [5] Display TLBs entries\n"
            "     [6] Display Primary data cache entries\n"
            "     [7] Display CPU registers\n"
            "     [8] Size memory (Moosehead)\n"
            "     [9] Exit\n"
            "\n"
            "     =====> ") ;
    return ;
}

/*
 * Body of the utilities cmd loop.
 */
void
do_utilities ()
{
  char c ;

  while (1) {
    UTILmenu () ;
    switch (c = GETC ()) {
    case '1':
      putchar (c) ;
      READfunction () ;
      break ;
    case '2':
      putchar (c) ;
      WRITEfunction () ;
      break ; 
    case '3':
      putchar (c) ;
      WRnRDfunction () ;
      break ; 
    case '4':
      putchar (c) ;
      RDmWRfunction () ;
      break ; 
    case '5':
      putchar (c) ;
      DISPtlbs () ;
      break ; 
    case '6':
      putchar (c) ;
      DISPcache () ;
      break ; 
    case '7':
      putchar (c) ;
      DISPregs () ;
      break ; 
    case '8':
      putchar (c) ;
      MOOSEmemory () ;
      break ;
    case '9':
      return ;
      break ;
    default:
      putchar (c) ;
      break ;
    }
  }
}

/*
 * -------------------------------------------------------------
 *
 * $Log: utilities.c,v $
 * Revision 1.1  1997/08/18 20:42:23  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.4  1996/10/31  21:51:57  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.3  1996/04/04  23:17:28  kuang
 * Added more diagnostic support and some general cleanup
 *
 * Revision 1.2  1995/12/30  03:28:42  kuang
 * First moosehead lab bringup checkin, corresponding to 12-29-95 d15
 *
 * Revision 1.1  1995/11/15  00:42:51  kuang
 * initial checkin
 *
 * Revision 1.2  1995/11/14  23:35:25  kuang
 * Rearranged the rcs keywords and get ready for ptool checkin
 *
 * Revision 1.1  1995/11/01  21:31:48  kuang
 * Initial revision
 *
 * -------------------------------------------------------------
 */

/* END OF FILE */
