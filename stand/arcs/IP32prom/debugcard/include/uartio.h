/*
 * -------------------------------------------------------------
 *  $Revision: 1.1 $
 *  $Date: 1997/08/18 20:41:46 $
 * -------------------------------------------------------------
 *  define the printf prototypes.
 */

#ifdef  _LANGUAGE_C

void        printudec   (uint64_t, int, int) ;
void        printdec    (uint64_t, int, int) ;
void        printhex    (uint64_t, int, int, char) ;
void        printoct    (uint64_t, int, int) ;
void        printbin    (uint64_t, int) ;
void        printf      (char *,...) ;
uint64_t    gethex      (void)   ;

#endif



/*
 * -------------------------------------------------------------
 *
 * $Log: uartio.h,v $
 * Revision 1.1  1997/08/18 20:41:46  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.2  1996/10/31  21:51:36  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.1  1995/11/15  00:41:26  kuang
 * initial checkin
 *
 * Revision 1.1  1995/11/14  22:57:39  kuang
 * Initial revision
 *
 * -------------------------------------------------------------
 */

/* END OF FILE */
