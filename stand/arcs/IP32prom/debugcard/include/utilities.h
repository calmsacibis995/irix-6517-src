/*
 * -------------------------------------------------------------
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:41:48 $
 * -------------------------------------------------------------
 */

/*
 *  Functions prototypes.
 */
#if defined(_LANGUAGE_C)
#include <inttypes.h>

extern uint32_t     GETaddr         (void) ;
extern uint64_t     GETdata         (void) ;
extern int32_t      GETdatasize     (void) ;
extern void         READfunction    (void) ;
extern void         WRITEfunction   (void) ;
extern void         WRnRDfunction   (void) ;
extern void         RDmWRfunction   (void) ;
extern void         UTILmenu        (void) ;
extern void         do_utilities    (void) ;

#endif

/*
 * -------------------------------------------------------------
 *
 * $Log: utilities.h,v $
 * Revision 1.1  1997/08/18 20:41:48  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.2  1996/10/31  21:51:38  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.1  1995/11/15  00:41:27  kuang
 * initial checkin
 *
 * Revision 1.1  1995/11/14  22:58:49  kuang
 * Initial revision
 *
 * -------------------------------------------------------------
 */

/* END OF FILE */
